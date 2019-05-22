/**
 * @file
 * Thread implementation which uses C++17 library features
 * @copyright 
 * © 2019 Joshua Scoggins 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <cerrno>
#include <pthread.h>
#include <cstdlib>
#include <unistd.h>
#include <memory>
#include "thread_cxx17.h"


namespace jyq::concurrency {
    using RawRWLock = std::shared_ptr<pthread_rwlock_t>;
    using RawMutexLock = std::shared_ptr<pthread_mutex_t>;
    using RawRendez = std::shared_ptr<pthread_cond_t>;
    bool 
    Cxx17ThreadImpl::init(jyq::RWLock* rw) { 
        if (auto rwlock = std::make_shared<pthread_rwlock_t>(); pthread_rwlock_init(rwlock.get(), nullptr)) {
        //if (auto rwlock = new pthread_rwlock_t; pthread_rwlock_init(rwlock, nullptr)) {
            return true;
        } else {
            rw->setAux(rwlock);
            return false;
        }
    }
    bool 
    Cxx17ThreadImpl::init(jyq::Rendez* r) {
        if (auto cond = std::make_shared<pthread_cond_t>(); pthread_cond_init(cond.get(), nullptr)) {
            return true;
        } else {
            r->setAux(cond);
            return false;
        }
    }
    bool 
    Cxx17ThreadImpl::init(jyq::Mutex* m) { 
        if (auto mutex = std::make_shared<pthread_mutex_t>(); pthread_mutex_init(mutex.get(), nullptr)) {
            return true;
        } else {
            m->setAux(mutex);
            return false;
        }
    }
    void 
    Cxx17ThreadImpl::destroy(jyq::Rendez* r) { 
        auto val = r->unpackAux<RawRendez>();
        pthread_cond_destroy(val.get());
        r->resetAux();
    }

    void 
    Cxx17ThreadImpl::destroy(jyq::Mutex* m) { 
        auto mut = m->unpackAux<RawMutexLock>();
        pthread_mutex_destroy(mut.get());
        m->resetAux();
    }

    void 
    Cxx17ThreadImpl::destroy(jyq::RWLock* rw) { 
        auto val = rw->unpackAux<RawRWLock>();
        pthread_rwlock_destroy(val.get());
        rw->resetAux();
    }

    char* 
    Cxx17ThreadImpl::errbuf() { 
        static pthread_key_t errstr_k;
        auto ret = (char*)pthread_getspecific(errstr_k);
        if (!ret) {
            ret = new char[ErrorMax];
            pthread_setspecific(errstr_k, (void*)ret);
        }
        return ret;
    }
    bool 
    Cxx17ThreadImpl::wake(jyq::Rendez* r) { 
        auto val = r->unpackAux<RawRendez>();
        pthread_cond_signal(val.get());
        return false;
    }
    bool 
    Cxx17ThreadImpl::wakeall(jyq::Rendez* r) { 
        auto val = r->unpackAux<RawRendez>();
        pthread_cond_broadcast(val.get());
        return 0;
    }   

    void Cxx17ThreadImpl::rlock(jyq::RWLock* rw) { 
        pthread_rwlock_rdlock(rw->unpackAux<RawRWLock>().get());
    }
    bool Cxx17ThreadImpl::canrlock(jyq::RWLock* rw) { 
        return !pthread_rwlock_tryrdlock(rw->unpackAux<RawRWLock>().get());
    }
    void Cxx17ThreadImpl::runlock(jyq::RWLock* rw) { 
        pthread_rwlock_unlock(rw->unpackAux<RawRWLock>().get());
    }
    void Cxx17ThreadImpl::wlock(jyq::RWLock* rw) { 
        pthread_rwlock_rdlock(rw->unpackAux<RawRWLock>().get());
    }
    bool Cxx17ThreadImpl::canwlock(jyq::RWLock* rw) { 
        return !pthread_rwlock_tryrdlock(rw->unpackAux<RawRWLock>().get());
    }
    void Cxx17ThreadImpl::wunlock(jyq::RWLock* rw) { 
        pthread_rwlock_unlock(rw->unpackAux<RawRWLock>().get());
    }
    bool Cxx17ThreadImpl::canlock(jyq::Mutex* m) { 
        return !pthread_mutex_trylock(m->unpackAux<RawMutexLock>().get()); 
    }
    void Cxx17ThreadImpl::lock(jyq::Mutex* m) { 
        pthread_mutex_lock(m->unpackAux<RawMutexLock>().get());
    }
    void Cxx17ThreadImpl::unlock(jyq::Mutex* m) { 
        pthread_mutex_unlock(m->unpackAux<RawMutexLock>().get());
    }
    void Cxx17ThreadImpl::sleep(jyq::Rendez* r) { 
        pthread_cond_wait(r->unpackAux<RawRendez>().get(), 
                          r->getMutex()->unpackAux<RawMutexLock>().get());
    }
} // end namespace jyq::concurrency 
