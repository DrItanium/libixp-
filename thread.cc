/* Public Domain --Kris Maglione */
#include <unistd.h>
#include "ixp_local.h"


static char*
errbuf(void) {
	static char errbuf[IXP_ERRMAX];

	return errbuf;
}

static void
mvoid(IxpMutex *m) {
	USED(m);
	return;
}

static int
mtrue(IxpMutex *m) {
	USED(m);
	return 1;
}

static int
mfalse(IxpMutex *m) {
	USED(m);
	return 0;
}

static void
rwvoid(IxpRWLock *rw) {
	USED(rw);
	return;
}

static int
rwtrue(IxpRWLock *rw) {
	USED(rw);
	return 1;
}

static int
rwfalse(IxpRWLock *m) {
	USED(m);
	return 0;
}

static void
rvoid(IxpRendez *r) {
	USED(r);
	return;
}

static int
rfalse(IxpRendez *r) {
	USED(r);
	return 0;
}

static void
rsleep(IxpRendez *r) {
	USED(r);
    ixp::fatalPrint("rsleep called when not implemented\n");
}

static IxpThread ixp_nothread = {
	/* RWLock */
	.initrwlock = rwfalse,
	.rlock = rwvoid,
	.canrlock = rwtrue,
	.runlock = rwvoid,
	.wlock = rwvoid,
	.canwlock = rwtrue,
	.wunlock = rwvoid,
	.rwdestroy = rwvoid,
	/* Mutex */
	.initmutex = mfalse,
	.lock = mvoid,
	.canlock = mtrue,
	.unlock = mvoid,
	.mdestroy = mvoid,
	/* Rendez */
	.initrendez = rfalse,
	.sleep = rsleep,
	.wake = rfalse,
	.wakeall = rfalse,
	.rdestroy = rvoid,
	/* Other */
	.errbuf = errbuf,
	.read = read,
	.write = write,
	.select = select,
};

IxpThread*          ixp_thread = &ixp_nothread;
