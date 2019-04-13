/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#define _XOPEN_SOURCE 600
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "ixp_local.h"
#include "thread_pthread.h"


namespace ixp::concurrency {
    bool 
    PThreadImpl::init(IxpRWLock* rw) { 
        if (auto rwlock = new pthread_rwlock_t; pthread_rwlock_init(rwlock, nullptr)) {
            delete rwlock;
            return true;
        } else {
            rw->aux = rwlock;
            return false;
        }
    }
    bool 
    PThreadImpl::init(IxpRendez* r) {
        if (auto cond = new pthread_cond_t; pthread_cond_init(cond, nullptr)) {
            delete cond; 
            return true;
        } else {
            r->aux = cond;
            return false;
        }
    }
    bool 
    PThreadImpl::init(IxpMutex* m) { 
        if (auto mutex = new pthread_mutex_t; pthread_mutex_init(mutex, nullptr)) {
            delete mutex;
            return true;
        } else {
            m->aux = mutex;
            return false;
        }
    }
    void 
    PThreadImpl::destroy(IxpRendez* r) { 
        auto val = std::any_cast<pthread_cond_t*>(r->aux);
        pthread_cond_destroy(val);
        delete val;
        r->aux.reset();
    }

    void 
    PThreadImpl::destroy(IxpMutex* m) { 
        auto mut = std::any_cast<pthread_mutex_t*>(m->aux);
        pthread_mutex_destroy(mut);
        delete mut;
        m->aux.reset();
    }

    void 
    PThreadImpl::destroy(IxpRWLock* rw) { 
        auto val = std::any_cast<pthread_rwlock_t*>(rw->aux);
        pthread_rwlock_destroy(val);
        delete val;
        rw->aux.reset();
    }

    char* 
    PThreadImpl::errbuf() { 
        static pthread_key_t errstr_k;
        auto ret = (char*)pthread_getspecific(errstr_k);
        if (!ret) {
            ret = (char*)ixp::emallocz(IXP_ERRMAX);
            pthread_setspecific(errstr_k, (void*)ret);
        }
        return ret;
    }
    bool 
    PThreadImpl::wake(IxpRendez* r) { 
        auto val = std::any_cast<pthread_cond_t*>(r->aux);
        pthread_cond_signal(val);
        return false;
    }
    bool 
    PThreadImpl::wakeall(IxpRendez* r) { 
        auto val = std::any_cast<pthread_cond_t*>(r->aux);
        pthread_cond_broadcast(val);
        return 0;
    }   

    void PThreadImpl::rlock(IxpRWLock* rw) { pthread_rwlock_rdlock(std::any_cast<pthread_rwlock_t*>(rw->aux)); }
    bool PThreadImpl::canrlock(IxpRWLock* rw) { return !pthread_rwlock_tryrdlock(std::any_cast<pthread_rwlock_t*>(rw->aux)); }
    void PThreadImpl::runlock(IxpRWLock* rw) { pthread_rwlock_unlock(std::any_cast<pthread_rwlock_t*>(rw->aux)); }
    void PThreadImpl::wlock(IxpRWLock* rw) { pthread_rwlock_rdlock(std::any_cast<pthread_rwlock_t*>(rw->aux)); }
    bool PThreadImpl::canwlock(IxpRWLock* rw) { return !pthread_rwlock_tryrdlock(std::any_cast<pthread_rwlock_t*>(rw->aux)); }
    void PThreadImpl::wunlock(IxpRWLock* rw) { pthread_rwlock_unlock(std::any_cast<pthread_rwlock_t*>(rw->aux)); }
    bool PThreadImpl::canlock(IxpMutex* m) { return !pthread_mutex_trylock(std::any_cast<pthread_mutex_t*>(m->aux)); }
    void PThreadImpl::lock(IxpMutex* m)   { pthread_mutex_lock(std::any_cast<pthread_mutex_t*>(m->aux)); }
    void PThreadImpl::unlock(IxpMutex* m) { pthread_mutex_unlock(std::any_cast<pthread_mutex_t*>(m->aux)); }
    void PThreadImpl::sleep(IxpRendez* r) { pthread_cond_wait(std::any_cast<pthread_cond_t*>(r->aux), std::any_cast<pthread_mutex_t*>(r->mutex->aux)); }
} // end namespace ixp::concurrency 
