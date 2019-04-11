/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#define _XOPEN_SOURCE 600
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "ixp_local.h"

static pthread_key_t errstr_k;


static char*
errbuf(void) {

	auto ret = (char*)pthread_getspecific(errstr_k);
	if(ret == nullptr) {
		ret = (char*)emallocz(IXP_ERRMAX);
		pthread_setspecific(errstr_k, (void*)ret);
	}
	return ret;
}

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
	if (auto mutex = (pthread_mutex_t*)emalloc(sizeof (pthread_mutex_t)); pthread_mutex_init(mutex, nil)) {
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
	if(pthread_rwlock_init(rwlock, nil)) {
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

static int
initrendez(IxpRendez *r) {

    if(auto cond = (pthread_cond_t*)emalloc(sizeof(pthread_cond_t)); pthread_cond_init(cond, nil)) {
        free(cond);
        return 1;
    } else {
        r->aux = cond;
        return 0;
    }
}

static IxpThread ixp_pthread = {
	/* RWLock */
	.initrwlock = initrwlock,
	.rlock = rlock,
	.canrlock = canrlock,
	.runlock = rwunlock,
	.wlock = wlock,
	.canwlock = canwlock,
	.wunlock = rwunlock,
	.rwdestroy = rwdestroy,
	/* Mutex */
	.initmutex = initmutex,
	.lock = mlock,
	.canlock = mcanlock,
	.unlock = munlock,
	.mdestroy = mdestroy,
	/* Rendez */
	.initrendez = initrendez,
	.sleep = rsleep,
	.wake = rwake,
	.wakeall = rwakeall,
	.rdestroy = rdestroy,
	/* Other */
	.errbuf = errbuf,
	.read = read,
	.write = write,
	.select = select,
};

/**
 * Function: ixp_pthread_init
 *
 * This function initializes libixp for use in multithreaded
 * programs using the POSIX thread system. When using libixp in such
 * programs, this function must be called before any other libixp
 * functions. This function is part of libixp_pthread, which you
 * must explicitly link against.
 */
int
ixp_pthread_init() {
	int ret;

	ret = pthread_key_create(&errstr_k, free);
	if(ret) {
		werrstr("can't create TLS value: %s", ixp_errbuf());
		return 1;
	}

	ixp_thread = &ixp_pthread;
	return 0;
}
