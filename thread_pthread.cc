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
        pthread_rwlock_t *rwlock;

        rwlock = (pthread_rwlock_t*)emalloc(sizeof *rwlock);
        if(pthread_rwlock_init(rwlock, nullptr)) {
            free(rwlock);
            return 1;
        }

        rw->aux = rwlock;
        return 0;
    }
    bool 
    PThreadImpl::init(IxpRendez* r) {
        if(auto cond = (pthread_cond_t*)emalloc(sizeof(pthread_cond_t)); pthread_cond_init(cond, nullptr)) {
            free(cond);
            return true;
        } else {
            r->aux = cond;
            return false;
        }
    }
    bool 
    PThreadImpl::init(IxpMutex* m) { 
        if (auto mutex = (pthread_mutex_t*)emalloc(sizeof (pthread_mutex_t)); pthread_mutex_init(mutex, nullptr)) {
            free(mutex);
            return true;
        } else {
            m->aux = mutex;
            return false;
        }
    }
    void 
    PThreadImpl::destroy(IxpRendez* r) { 
        pthread_cond_destroy((pthread_cond_t*)r->aux);
        free(r->aux);
    }

    void 
    PThreadImpl::destroy(IxpMutex* m) { 
	    pthread_mutex_destroy((pthread_mutex_t*)m->aux);
	    free(m->aux);
    }

    void 
    PThreadImpl::destroy(IxpRWLock* rw) { 
        pthread_rwlock_destroy((pthread_rwlock_t*)rw->aux);
        free(rw->aux);
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
        pthread_cond_signal((pthread_cond_t*)r->aux);
        return false;
    }
    bool 
    PThreadImpl::wakeall(IxpRendez* r) { 
    	pthread_cond_broadcast((pthread_cond_t*)r->aux);
       return 0;
    }   

    void PThreadImpl::rlock(IxpRWLock* rw) { pthread_rwlock_rdlock((pthread_rwlock_t*)rw->aux); }
    bool PThreadImpl::canrlock(IxpRWLock* rw) { return !pthread_rwlock_tryrdlock((pthread_rwlock_t*)rw->aux); }
    void PThreadImpl::runlock(IxpRWLock* rw) { pthread_rwlock_unlock((pthread_rwlock_t*)rw->aux); }
    void PThreadImpl::wlock(IxpRWLock* rw) { pthread_rwlock_rdlock((pthread_rwlock_t*)rw->aux); }
    bool PThreadImpl::canwlock(IxpRWLock* rw) { return !pthread_rwlock_tryrdlock((pthread_rwlock_t*)rw->aux); }
    void PThreadImpl::wunlock(IxpRWLock* rw) { pthread_rwlock_unlock((pthread_rwlock_t*)rw->aux); }
    bool PThreadImpl::canlock(IxpMutex* m) { return !pthread_mutex_trylock((pthread_mutex_t*)m->aux); }
    void PThreadImpl::lock(IxpMutex* m)   { pthread_mutex_lock((pthread_mutex_t*)m->aux); }
    void PThreadImpl::unlock(IxpMutex* m) { pthread_mutex_unlock((pthread_mutex_t*)m->aux); }
    void PThreadImpl::sleep(IxpRendez* r) { pthread_cond_wait((pthread_cond_t*)r->aux, (pthread_mutex_t*)r->mutex->aux); }
} // end namespace ixp::concurrency 
