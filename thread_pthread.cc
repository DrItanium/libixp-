/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#define _XOPEN_SOURCE 600
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "ixp_local.h"
#include "thread_pthread.h"

static pthread_key_t errstr_k;

namespace ixp {

static void
mlock(IxpMutex *m) {
	pthread_mutex_lock((pthread_mutex_t*)m->aux);
}

static int
mcanlock(IxpMutex *m) {
	return !pthread_mutex_trylock((pthread_mutex_t*)m->aux);
}

static void
munlock(IxpMutex *m) {
	pthread_mutex_unlock((pthread_mutex_t*)m->aux);
}

static void
mdestroy(IxpMutex *m) {
	pthread_mutex_destroy((pthread_mutex_t*)m->aux);
	free(m->aux);
}

static int
initmutex(IxpMutex *m) {
	if (auto mutex = (pthread_mutex_t*)emalloc(sizeof (pthread_mutex_t)); pthread_mutex_init(mutex, nullptr)) {
        free(mutex);
        return 1;
    } else {
        m->aux = mutex;
        return 0;
    }
}

static void
rlock(IxpRWLock *rw) {
	pthread_rwlock_rdlock((pthread_rwlock_t*)rw->aux);
}

static int
canrlock(IxpRWLock *rw) {
	return !pthread_rwlock_tryrdlock((pthread_rwlock_t*)rw->aux);
}

static void
wlock(IxpRWLock *rw) {
	pthread_rwlock_rdlock((pthread_rwlock_t*)rw->aux);
}

static int
canwlock(IxpRWLock *rw) {
	return !pthread_rwlock_tryrdlock((pthread_rwlock_t*)rw->aux);
}

static void
rwunlock(IxpRWLock *rw) {
	pthread_rwlock_unlock((pthread_rwlock_t*)rw->aux);
}

static void
rwdestroy(IxpRWLock *rw) {
	pthread_rwlock_destroy((pthread_rwlock_t*)rw->aux);
	free(rw->aux);
}

static int
initrwlock(IxpRWLock *rw) {
	pthread_rwlock_t *rwlock;

	rwlock = (pthread_rwlock_t*)emalloc(sizeof *rwlock);
	if(pthread_rwlock_init(rwlock, nullptr)) {
		free(rwlock);
		return 1;
	}

	rw->aux = rwlock;
	return 0;
}

static void
rsleep(IxpRendez *r) {
	pthread_cond_wait((pthread_cond_t*)r->aux, (pthread_mutex_t*)r->mutex->aux);
}

static int
rwake(IxpRendez *r) {
	pthread_cond_signal((pthread_cond_t*)r->aux);
	return 0;
}

static int
rwakeall(IxpRendez *r) {
	pthread_cond_broadcast((pthread_cond_t*)r->aux);
	return 0;
}

static void
rdestroy(IxpRendez *r) {
	pthread_cond_destroy((pthread_cond_t*)r->aux);
	free(r->aux);
}

namespace concurrency {
    bool PThreadImpl::init(IxpRWLock* a) { return ixp::initrwlock(a); }
    void PThreadImpl::rlock(IxpRWLock* a) { ixp::rlock(a); }
    bool PThreadImpl::canrlock(IxpRWLock* a) { return ixp::canrlock(a); }
    void PThreadImpl::runlock(IxpRWLock* a) { ixp::rwunlock(a); }
    void PThreadImpl::wlock(IxpRWLock* a) { ixp::wlock(a); }
    bool PThreadImpl::canwlock(IxpRWLock* a) { return ixp::canwlock(a); }
    void PThreadImpl::wunlock(IxpRWLock* a) { ixp::rwunlock(a); }
    void PThreadImpl::destroy(IxpRWLock* a) { ixp::rwdestroy(a); }
    bool PThreadImpl::init(IxpMutex* a) { return ixp::initmutex(a); }
    bool PThreadImpl::canlock(IxpMutex* a) { return ixp::mcanlock(a); }
    void PThreadImpl::lock(IxpMutex* a) { ixp::mlock(a); }
    void PThreadImpl::unlock(IxpMutex* a) { ixp::munlock(a); }
    void PThreadImpl::destroy(IxpMutex* a) { ixp::mdestroy(a); }
    bool PThreadImpl::init(IxpRendez* r) {
        if(auto cond = (pthread_cond_t*)emalloc(sizeof(pthread_cond_t)); pthread_cond_init(cond, nullptr)) {
            free(cond);
            return true;
        } else {
            r->aux = cond;
            return false;
        }
    }
    bool PThreadImpl::wake(IxpRendez* a) { return ixp::rwake(a); }
    bool PThreadImpl::wakeall(IxpRendez* a) { return ixp::rwakeall(a); }
    void PThreadImpl::sleep(IxpRendez* a) { ixp::rsleep(a); }
    void PThreadImpl::destroy(IxpRendez* a) { ixp::rdestroy(a); }
    char* PThreadImpl::errbuf() { 
        auto ret = (char*)pthread_getspecific(errstr_k);
        if (!ret) {
            ret = (char*)ixp::emallocz(IXP_ERRMAX);
            pthread_setspecific(errstr_k, (void*)ret);
        }
        return ret;
    }
} // end namespace concurrency 
} // end namespace ixp
