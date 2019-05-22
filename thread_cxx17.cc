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
#include <unistd.h>
#include <memory>
#include <shared_mutex>
#include <condition_variable>
#include <mutex>
#include "thread_cxx17.h"


namespace jyq::concurrency {
    using RawRWLock = std::shared_ptr<std::shared_mutex>;
    using RawMutexLock = std::shared_ptr<std::mutex>;
    using RawRendez = std::shared_ptr<std::condition_variable>;
    bool 
    Cxx17ThreadImpl::init(jyq::RWLock* rw) { 
        rw->setAux(std::make_shared<std::shared_mutex>());
        return false;
    }
    bool 
    Cxx17ThreadImpl::init(jyq::Rendez* r) {
        r->setAux(std::make_shared<std::condition_variable>());
        return false;
    }
    bool 
    Cxx17ThreadImpl::init(jyq::Mutex* m) { 
        m->setAux(std::make_shared<std::mutex>());
        return false;
    }
    void 
    Cxx17ThreadImpl::destroy(jyq::Rendez* r) { 
        r->resetAux();
    }

    void 
    Cxx17ThreadImpl::destroy(jyq::Mutex* m) { 
        m->resetAux();
    }

    void 
    Cxx17ThreadImpl::destroy(jyq::RWLock* rw) { 
        rw->resetAux();
    }

    char* 
    Cxx17ThreadImpl::errbuf() { 
        static char* buf = nullptr;
        if (!buf) {
            buf = new char[ErrorMax];
        }
        return buf;
    }
    bool 
    Cxx17ThreadImpl::wake(jyq::Rendez* r) { 
        r->unpackAux<RawRendez>()->notify_one();
        return false;
    }
    bool 
    Cxx17ThreadImpl::wakeall(jyq::Rendez* r) { 
        r->unpackAux<RawRendez>()->notify_all();
        return 0;
    }   

    void Cxx17ThreadImpl::rlock(jyq::RWLock* rw) { 
        auto rwlock = rw->unpackAux<RawRWLock>();
        rw->unpackAux<RawRWLock>()->lock_shared();
    }
    bool Cxx17ThreadImpl::canrlock(jyq::RWLock* rw) { 
        return rw->unpackAux<RawRWLock>()->try_lock_shared();
    }
    void Cxx17ThreadImpl::runlock(jyq::RWLock* rw) { 
        rw->unpackAux<RawRWLock>()->unlock_shared();
    }
    void Cxx17ThreadImpl::wlock(jyq::RWLock* rw) { 
        rw->unpackAux<RawRWLock>()->lock();
    }
    bool Cxx17ThreadImpl::canwlock(jyq::RWLock* rw) { 
        return rw->unpackAux<RawRWLock>()->try_lock();
    }
    void Cxx17ThreadImpl::wunlock(jyq::RWLock* rw) { 
        rw->unpackAux<RawRWLock>()->unlock();
    }
    bool Cxx17ThreadImpl::canlock(jyq::Mutex* m) { 
        return m->unpackAux<RawMutexLock>()->try_lock();
    }
    void Cxx17ThreadImpl::lock(jyq::Mutex* m) { 
        m->unpackAux<RawMutexLock>()->lock();
    }
    void Cxx17ThreadImpl::unlock(jyq::Mutex* m) { 
        m->unpackAux<RawMutexLock>()->unlock();
    }
    void Cxx17ThreadImpl::sleep(jyq::Rendez* r) { 
        //pthread_cond_wait(r->unpackAux<RawRendez>().get(), 
         //                 r->getMutex()->unpackAux<RawMutexLock>().get());
    }
} // end namespace jyq::concurrency 
