/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "ixp_local.h"


namespace ixp {

static void handlereq(Req9 *r);
/**
 * Variable: printfcall
 *
 * When set to a non-null value, printfcall is called once for
 * every incoming and outgoing Fcall. It is intended to simplify the
 * writing of debugging code for clients, but may be used for any
 * arbitrary purpose.
 *
 * See also:
 *	F<respond>, F<serve9conn>
 */
std::function<void(Fcall*)> printfcall;


static char
	Eduptag[] = "tag in use",
	Edupfid[] = "fid in use",
	Enofunc[] = "function not implemented",
	Eopen[] = "fid is already open",
	Enofile[] = "file does not exist",
	Enoread[] = "file not open for reading",
	Enofid[] = "fid does not exist",
	Enotag[] = "tag does not exist",
	Enotdir[] = "not a directory",
	Eintr[] = "interrupted",
	Eisdir[] = "cannot perform operation on a directory";

constexpr auto TAG_BUCKETS = 61;
constexpr auto FID_BUCKETS = 61;

struct Conn9 {
	Map		tagmap;
	Map		fidmap;
	MapEnt*		taghash[TAG_BUCKETS];
	MapEnt*		fidhash[FID_BUCKETS];
	Srv9*	srv;
	Conn*	conn;
	Mutex	rlock;
	Mutex	wlock;
	Msg		rmsg;
	Msg		wmsg;
	int		ref;
};

static void
decref_p9conn(Conn9 *p9conn) {
	concurrency::threadModel->lock(&p9conn->wlock);
	if(--p9conn->ref > 0) {
		concurrency::threadModel->unlock(&p9conn->wlock);
		return;
	}
	concurrency::threadModel->unlock(&p9conn->wlock);

	assert(p9conn->conn == nullptr);

	concurrency::threadModel->mdestroy(&p9conn->rlock);
	concurrency::threadModel->mdestroy(&p9conn->wlock);

	mapfree(&p9conn->tagmap, nullptr);
	mapfree(&p9conn->fidmap, nullptr);

	free(p9conn->rmsg.data);
	free(p9conn->wmsg.data);
	free(p9conn);
}

static void*
createfid(Map *map, int fid, Conn9 *p9conn) {
	Fid *f;

	f = (Fid*)ixp::emallocz(sizeof *f);
	p9conn->ref++;
	f->conn = p9conn;
	f->fid = fid;
	f->omode = -1;
	f->map = map;
	if(mapinsert(map, fid, f, false))
		return f;
	free(f);
	return nullptr;
}

static bool 
destroyfid(Conn9 *p9conn, ulong fid) {
	Fid *f;

	f = (Fid*)maprm(&p9conn->fidmap, fid);
    if (!f)
		return false;

	if(p9conn->srv->freefid)
		p9conn->srv->freefid(f);

	decref_p9conn(p9conn);
	free(f);
	return true;
}

static void
handlefcall(Conn *c) {
	Fcall fcall = {0};
	Conn9 *p9conn;
	Req9 *req;

	p9conn = std::any_cast<decltype(p9conn)>(c->aux);

	concurrency::threadModel->lock(&p9conn->rlock);
	if(recvmsg(c->fd, &p9conn->rmsg) == 0)
		goto Fail;
	if(msg2fcall(&p9conn->rmsg, &fcall) == 0)
		goto Fail;
	concurrency::threadModel->unlock(&p9conn->rlock);

	req = (Req9*)ixp::emallocz(sizeof *req);
	p9conn->ref++;
	req->conn = p9conn;
	req->srv = p9conn->srv;
	req->ifcall = fcall;
	p9conn->conn = c;

	if(!mapinsert(&p9conn->tagmap, fcall.hdr.tag, req, false)) {
		respond(req, Eduptag);
		return;
	}

	handlereq(req);
	return;

Fail:
	concurrency::threadModel->unlock(&p9conn->rlock);
	hangup(c);
	return;
}

static void
handlereq(Req9 *r) {
	Conn9 *p9conn;
	Srv9 *srv;

	p9conn = r->conn;
	srv = p9conn->srv;

	if(printfcall)
		printfcall(&r->ifcall);

	switch(r->ifcall.hdr.type) {
	default:
		respond(r, Enofunc);
		break;
	case TVersion:
		if(!strcmp(r->ifcall.version.version, "9P"))
			r->ofcall.version.version = "9P";
		else if(!strcmp(r->ifcall.version.version, "9P2000"))
			r->ofcall.version.version = "9P2000";
		else
			r->ofcall.version.version = "unknown";
		r->ofcall.version.msize = r->ifcall.version.msize;
		respond(r, nullptr);
		break;
	case TAttach:
		if(!(r->fid = (decltype(r->fid))(createfid(&p9conn->fidmap, r->ifcall.hdr.fid, p9conn)))) {
			respond(r, Edupfid);
			return;
		}
		/* attach is a required function */
		srv->attach(r);
		break;
	case TClunk:
		if(!(r->fid = (decltype(r->fid))mapget(&p9conn->fidmap, r->ifcall.hdr.fid))) {
			respond(r, Enofid);
			return;
		}
		if(!srv->clunk) {
			respond(r, nullptr);
			return;
		}
		srv->clunk(r);
		break;
	case TFlush:
		if(!(r->oldreq = decltype(r->oldreq)(mapget(&p9conn->tagmap, r->ifcall.tflush.oldtag)))) {
			respond(r, Enotag);
			return;
		}
		if(!srv->flush) {
			respond(r, Enofunc);
			return;
		}
		srv->flush(r);
		break;
	case TCreate:
		if(!(r->fid = decltype(r->fid)(mapget(&p9conn->fidmap, r->ifcall.hdr.fid)))) {
			respond(r, Enofid);
			return;
		}
		if(r->fid->omode != -1) {
			respond(r, Eopen);
			return;
		}
		if(!(r->fid->qid.type&QTDIR)) {
			respond(r, Enotdir);
			return;
		}
		if(!p9conn->srv->create) {
			respond(r, Enofunc);
			return;
		}
		p9conn->srv->create(r);
		break;
	case TOpen:
		if(!(r->fid = decltype(r->fid)(mapget(&p9conn->fidmap, r->ifcall.hdr.fid)))) {
			respond(r, Enofid);
			return;
		}
		if((r->fid->qid.type&QTDIR) && (r->ifcall.topen.mode|P9_ORCLOSE) != (P9_OREAD|P9_ORCLOSE)) {
			respond(r, Eisdir);
			return;
		}
		r->ofcall.ropen.qid = r->fid->qid;
		if(!p9conn->srv->open) {
			respond(r, Enofunc);
			return;
		}
		p9conn->srv->open(r);
		break;
	case TRead:
		if(!(r->fid = decltype(r->fid)(mapget(&p9conn->fidmap, r->ifcall.hdr.fid)))) {
			respond(r, Enofid);
			return;
		}
		if(r->fid->omode == -1 || r->fid->omode == P9_OWRITE) {
			respond(r, Enoread);
			return;
		}
		if(!p9conn->srv->read) {
			respond(r, Enofunc);
			return;
		}
		p9conn->srv->read(r);
		break;
	case TRemove:
		if(!(r->fid = decltype(r->fid)(mapget(&p9conn->fidmap, r->ifcall.hdr.fid)))) {
			respond(r, Enofid);
			return;
		}
		if(!p9conn->srv->remove) {
			respond(r, Enofunc);
			return;
		}
		p9conn->srv->remove(r);
		break;
	case TStat:
		if(!(r->fid = decltype(r->fid)(mapget(&p9conn->fidmap, r->ifcall.hdr.fid)))) {
			respond(r, Enofid);
			return;
		}
		if(!p9conn->srv->stat) {
			respond(r, Enofunc);
			return;
		}
		p9conn->srv->stat(r);
		break;
	case TWalk:
		if(!(r->fid = decltype(r->fid)(mapget(&p9conn->fidmap, r->ifcall.hdr.fid)))) {
			respond(r, Enofid);
			return;
		}
		if(r->fid->omode != -1) {
			respond(r, "cannot walk from an open fid");
			return;
		}
		if(r->ifcall.twalk.nwname && !(r->fid->qid.type&QTDIR)) {
			respond(r, Enotdir);
			return;
		}
		if((r->ifcall.hdr.fid != r->ifcall.twalk.newfid)) {
			if(!(r->newfid = decltype(r->newfid)(createfid(&p9conn->fidmap, r->ifcall.twalk.newfid, p9conn)))) {
				respond(r, Edupfid);
				return;
			}
		}else
			r->newfid = r->fid;
		if(!p9conn->srv->walk) {
			respond(r, Enofunc);
			return;
		}
		p9conn->srv->walk(r);
		break;
	case TWrite:
		if(!(r->fid = decltype(r->fid)(mapget(&p9conn->fidmap, r->ifcall.hdr.fid)))) {
			respond(r, Enofid);
			return;
		}
		if((r->fid->omode&3) != P9_OWRITE && (r->fid->omode&3) != P9_ORDWR) {
			respond(r, "write on fid not opened for writing");
			return;
		}
		if(!p9conn->srv->write) {
			respond(r, Enofunc);
			return;
		}
		p9conn->srv->write(r);
		break;
	case TWStat:
		if(!(r->fid = decltype(r->fid)(mapget(&p9conn->fidmap, r->ifcall.hdr.fid)))) {
			respond(r, Enofid);
			return;
		}
		if(~r->ifcall.twstat.stat.type) {
			respond(r, "wstat of type");
			return;
		}
		if(~r->ifcall.twstat.stat.dev) {
			respond(r, "wstat of dev");
			return;
		}
		if(~r->ifcall.twstat.stat.qid.type || (ulong)~r->ifcall.twstat.stat.qid.version || ~r->ifcall.twstat.stat.qid.path) {
			respond(r, "wstat of qid");
			return;
		}
		if(r->ifcall.twstat.stat.muid && r->ifcall.twstat.stat.muid[0]) {
			respond(r, "wstat of muid");
			return;
		}
		if(~r->ifcall.twstat.stat.mode && ((r->ifcall.twstat.stat.mode&DMDIR)>>24) != r->fid->qid.type&QTDIR) {
			respond(r, "wstat on DMDIR bit");
			return;
		}
		if(!p9conn->srv->wstat) {
			respond(r, Enofunc);
			return;
		}
		p9conn->srv->wstat(r);
		break;
	/* Still to be implemented: auth */
	}
}

/**
 * Function: respond
 *
 * Sends a response to the given request. The response is
 * constructed from the P<ofcall> member of the P<req> parameter, or
 * from the P<error> parameter if it is non-null. In the latter
 * case, the response is of type RError, while in any other case it
 * is of the same type as P<req>->P<ofcall>, which must match the
 * request type in P<req>->P<ifcall>.
 *
 * See also:
 *	T<Req9>, V<printfcall>
 */
void
respond(Req9 *req, const char *error) {
	Conn9 *p9conn;
	int msize;

	p9conn = req->conn;

	switch(req->ifcall.hdr.type) {
	default:
		if(!error)
			assert(!"Respond called on unsupported fcall type");
		break;
	case TVersion:
		assert(error == nullptr);
		free(req->ifcall.version.version);

		concurrency::threadModel->lock(&p9conn->rlock);
		concurrency::threadModel->lock(&p9conn->wlock);
		msize = ixp::min<int>(req->ofcall.version.msize, IXP_MAX_MSG);
		p9conn->rmsg.data = (decltype(p9conn->rmsg.data))ixp::erealloc(p9conn->rmsg.data, msize);
		p9conn->wmsg.data = (decltype(p9conn->wmsg.data))ixp::erealloc(p9conn->wmsg.data, msize);
		p9conn->rmsg.size = msize;
		p9conn->wmsg.size = msize;
		concurrency::threadModel->unlock(&p9conn->wlock);
		concurrency::threadModel->unlock(&p9conn->rlock);
		req->ofcall.version.msize = msize;
		break;
	case TAttach:
		if(error)
			destroyfid(p9conn, req->fid->fid);
		free(req->ifcall.tattach.uname);
		free(req->ifcall.tattach.aname);
		break;
	case TOpen:
	case TCreate:
		if(!error) {
			req->ofcall.ropen.iounit = p9conn->rmsg.size - 24;
			req->fid->iounit = req->ofcall.ropen.iounit;
			req->fid->omode = req->ifcall.topen.mode;
			req->fid->qid = req->ofcall.ropen.qid;
		}
		free(req->ifcall.tcreate.name);
		break;
	case TWalk:
		if(error || req->ofcall.rwalk.nwqid < req->ifcall.twalk.nwname) {
			if(req->ifcall.hdr.fid != req->ifcall.twalk.newfid && req->newfid)
				destroyfid(p9conn, req->newfid->fid);
			if(!error && req->ofcall.rwalk.nwqid == 0)
				error = Enofile;
		}else{
			if(req->ofcall.rwalk.nwqid == 0)
				req->newfid->qid = req->fid->qid;
			else
				req->newfid->qid = req->ofcall.rwalk.wqid[req->ofcall.rwalk.nwqid-1];
		}
		free(*req->ifcall.twalk.wname);
		break;
	case TWrite:
		free(req->ifcall.twrite.data);
		break;
	case TRemove:
		if(req->fid)
			destroyfid(p9conn, req->fid->fid);
		break;
	case TClunk:
		if(req->fid)
			destroyfid(p9conn, req->fid->fid);
		break;
	case TFlush:
		if((req->oldreq = decltype(req->oldreq) (mapget(&p9conn->tagmap, req->ifcall.tflush.oldtag))))
			respond(req->oldreq, Eintr);
		break;
	case TWStat:
		Stat::free(&req->ifcall.twstat.stat);
		break;
	case TRead:
	case TStat:
		break;		
	/* Still to be implemented: auth */
	}

	req->ofcall.hdr.tag = req->ifcall.hdr.tag;

    if (!error)
		req->ofcall.hdr.type = req->ifcall.hdr.type + 1;
	else {
		req->ofcall.hdr.type = RError;
		req->ofcall.error.ename = (char*)error;
	}

	if(printfcall)
		printfcall(&req->ofcall);

	maprm(&p9conn->tagmap, req->ifcall.hdr.tag);;

	if(p9conn->conn) {
		concurrency::threadModel->lock(&p9conn->wlock);
		msize = fcall2msg(&p9conn->wmsg, &req->ofcall);
		if(sendmsg(p9conn->conn->fd, &p9conn->wmsg) != msize)
			hangup(p9conn->conn);
		concurrency::threadModel->unlock(&p9conn->wlock);
	}

	switch(req->ofcall.hdr.type) {
	case RStat:
		free(req->ofcall.rstat.stat);
		break;
	case RRead:
		free(req->ofcall.rread.data);
		break;
	}
	free(req);
	decref_p9conn(p9conn);
}

/* Flush a pending request */
static void
voidrequest(void *context, void *arg) {
	Req9 *orig_req, *flush_req;
	Conn9 *conn;

	orig_req = decltype(orig_req)(arg);
	conn = orig_req->conn;
	conn->ref++;

	flush_req = (Req9*)ixp::emallocz(sizeof *orig_req);
	flush_req->ifcall.hdr.type = TFlush;
	flush_req->ifcall.hdr.tag = NoTag;
	flush_req->ifcall.tflush.oldtag = orig_req->ifcall.hdr.tag;
	flush_req->conn = conn;

	flush_req->aux = *(void**)context;
	*(void**)context = flush_req;
}

/* Clunk an open Fid */
static void
voidfid(void *context, void *arg) {
	Conn9 *p9conn;
	Req9 *clunk_req;
	Fid *fid;

	fid = decltype(fid)(arg);
	p9conn = fid->conn;
	p9conn->ref++;

	clunk_req = (Req9*)ixp::emallocz(sizeof *clunk_req);
	clunk_req->ifcall.hdr.type = TClunk;
	clunk_req->ifcall.hdr.tag = NoTag;
	clunk_req->ifcall.hdr.fid = fid->fid;
	clunk_req->fid = fid;
	clunk_req->conn = p9conn;

	clunk_req->aux = *(void**)context;
	*(void**)context = clunk_req;
}

static void
cleanupconn(Conn *c) {
	Conn9 *p9conn;
	Req9 *req, *r;

    p9conn = std::any_cast<decltype(p9conn)>(c->aux);
	p9conn->conn = nullptr;
	req = nullptr;
	if(p9conn->ref > 1) {
		mapexec(&p9conn->fidmap, voidfid, &req);
		mapexec(&p9conn->tagmap, voidrequest, &req);
	}
	while((r = req)) {
        req = std::any_cast<decltype(req)>(r->aux);
        r->aux.reset();
		handlereq(r);
	}
	decref_p9conn(p9conn);
}

/* Handle incoming 9P connections */
/**
 * Type: Srv9
 * Type: Req9
 * Function: serve9conn
 *
 * The serve9conn handles incoming 9P connections. It is
 * ordinarily passed as the P<read> member to F<listen> with an
 * Srv9 structure passed as the P<aux> member. The handlers
 * defined in the Srv9 structure are called whenever a matching
 * Fcall type is received. The handlers are expected to call
 * F<respond> at some point, whether before they return or at
 * some undefined point in the future. Whenever a client
 * disconnects, libixp generates whatever flush and clunk events are
 * required to leave the connection in a clean state and waits for
 * all responses before freeing the connections associated data
 * structures.
 *
 * Whenever a file is closed and an T<Fid> is about to be freed,
 * the P<freefid> member is called to perform any necessary cleanup
 * and to free any associated resources.
 *
 * See also:
 *	F<listen>, F<respond>, F<printfcall>,
 *	F<Fcall>, F<Fid>
 */
void
serve9conn(Conn *c) {

	if(auto fd = accept(c->fd, nullptr, nullptr); fd < 0) {
		return;
    } else {
        auto p9conn = (Conn9*)ixp::emallocz(sizeof(Conn9));
        p9conn->ref++;
        p9conn->srv = std::any_cast<decltype(p9conn->srv)>(c->aux);
        p9conn->rmsg.size = 1024;
        p9conn->wmsg.size = 1024;
        p9conn->rmsg.data = (decltype(p9conn->rmsg.data))ixp::emalloc(p9conn->rmsg.size);
        p9conn->wmsg.data = (decltype(p9conn->wmsg.data))ixp::emalloc(p9conn->wmsg.size);

        mapinit(&p9conn->tagmap, p9conn->taghash, nelem(p9conn->taghash));
        mapinit(&p9conn->fidmap, p9conn->fidhash, nelem(p9conn->fidhash));
        concurrency::threadModel->initmutex(&p9conn->rlock);
        concurrency::threadModel->initmutex(&p9conn->wlock);

        listen(c->srv, fd, p9conn, handlefcall, cleanupconn);
    }
}
} // end namespace ixp
