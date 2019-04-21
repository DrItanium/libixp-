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


static std::string
	Eduptag = "tag in use",
	Edupfid = "fid in use",
	Enofunc = "function not implemented",
	Eopen = "fid is already open",
	Enofile = "file does not exist",
	Enoread = "file not open for reading",
	Enofid = "fid does not exist",
	Enotag = "tag does not exist",
	Enotdir = "not a directory",
	Eintr = "interrupted",
	Eisdir = "cannot perform operation on a directory";

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

    p9conn->tagmap.free(nullptr);
    p9conn->fidmap.free(nullptr);

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
	if(map->insert(fid, f, false))
		return f;
	free(f);
	return nullptr;
}

static bool 
destroyfid(Conn9 *p9conn, ulong fid) {
	Fid *f;

	f = (Fid*)p9conn->fidmap.rm(fid);
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
	Fcall fcall;
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

    if (!p9conn->tagmap.insert(fcall.hdr.tag, req, false)) {
        req->respond(Eduptag);
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

	switch(r->ifcall.getType()) {
	default:
		r->respond(Enofunc);
		break;
	case FType::TVersion:
		if(!strcmp(r->ifcall.version.version, "9P"))
			r->ofcall.version.version = "9P";
		else if(!strcmp(r->ifcall.version.version, "9P2000"))
			r->ofcall.version.version = "9P2000";
		else
			r->ofcall.version.version = "unknown";
		r->ofcall.version.setSize(r->ifcall.version.size());
		r->respond(nullptr);
		break;
	case FType::TAttach:
		if(!(r->fid = (decltype(r->fid))(createfid(&p9conn->fidmap, r->ifcall.hdr.fid, p9conn)))) {
			r->respond(Edupfid);
			return;
		}
		/* attach is a required function */
		srv->attach(r);
		break;
	case FType::TClunk:
		if(r->fid = (decltype(r->fid))p9conn->fidmap.get(r->ifcall.hdr.fid); !r->fid) {
			r->respond(Enofid);
			return;
		}
		if(!srv->clunk) {
			r->respond(nullptr);
			return;
		}
		srv->clunk(r);
		break;
	case FType::TFlush:
        if (r->oldreq = decltype(r->oldreq)(p9conn->tagmap.get(r->ifcall.tflush.oldtag)); !r->oldreq) {
			r->respond(Enotag);
			return;
		}
		if(!srv->flush) {
			r->respond(Enofunc);
			return;
		}
		srv->flush(r);
		break;
	case FType::TCreate:
        if (r->fid = decltype(r->fid)(p9conn->fidmap.get(r->ifcall.hdr.fid)); !r->fid) {
			r->respond(Enofid);
			return;
		}
		if(r->fid->omode != -1) {
			r->respond(Eopen);
			return;
		}
		if(!(r->fid->qid.type&uint8_t(QType::DIR))) {
			r->respond(Enotdir);
			return;
		}
		if(!p9conn->srv->create) {
			r->respond(Enofunc);
			return;
		}
		p9conn->srv->create(r);
		break;
	case FType::TOpen:
        if (r->fid = decltype(r->fid)(p9conn->fidmap.get(r->ifcall.hdr.fid)); !r->fid) {
			r->respond(Enofid);
			return;
		}
		if((r->fid->qid.type&uint8_t(QType::DIR)) && (r->ifcall.topen.mode|uint8_t(OMode::RCLOSE)) != (uint8_t(OMode::READ)|uint8_t(OMode::RCLOSE))) {
			r->respond(Eisdir);
			return;
		}
		r->ofcall.ropen.qid = r->fid->qid;
		if(!p9conn->srv->open) {
			r->respond(Enofunc);
			return;
		}
		p9conn->srv->open(r);
		break;
	case FType::TRead:
        if (r->fid = decltype(r->fid)(p9conn->fidmap.get(r->ifcall.hdr.fid)); !r->fid) {
			r->respond(Enofid);
			return;
		}
		if(r->fid->omode == -1 || r->fid->omode == uint8_t(OMode::WRITE)) {
			r->respond(Enoread);
			return;
		}
		if(!p9conn->srv->read) {
			r->respond(Enofunc);
			return;
		}
		p9conn->srv->read(r);
		break;
	case FType::TRemove:
		if(r->fid = decltype(r->fid)(p9conn->fidmap.get(r->ifcall.hdr.fid)); !r->fid) {
			r->respond(Enofid);
			return;
		}
		if(!p9conn->srv->remove) {
			r->respond(Enofunc);
			return;
		}
		p9conn->srv->remove(r);
		break;
	case FType::TStat:
		if(r->fid = decltype(r->fid)(p9conn->fidmap.get(r->ifcall.hdr.fid)); !r->fid) {
			r->respond(Enofid);
			return;
		}
		if(!p9conn->srv->stat) {
			r->respond(Enofunc);
			return;
		}
		p9conn->srv->stat(r);
		break;
	case FType::TWalk:
		if(r->fid = decltype(r->fid)(p9conn->fidmap.get(r->ifcall.hdr.fid)); !r->fid) {
			r->respond(Enofid);
			return;
		}
		if(r->fid->omode != -1) {
			r->respond("cannot walk from an open fid");
			return;
		}
		if(r->ifcall.twalk.size() && !(r->fid->qid.type&uint8_t(QType::DIR))) {
			r->respond(Enotdir);
			return;
		}
		if((r->ifcall.hdr.fid != r->ifcall.twalk.newfid)) {
			if(!(r->newfid = decltype(r->newfid)(createfid(&p9conn->fidmap, r->ifcall.twalk.newfid, p9conn)))) {
				r->respond(Edupfid);
				return;
			}
		}else
			r->newfid = r->fid;
		if(!p9conn->srv->walk) {
			r->respond(Enofunc);
			return;
		}
		p9conn->srv->walk(r);
		break;
	case FType::TWrite:
		if(r->fid = decltype(r->fid)(p9conn->fidmap.get(r->ifcall.hdr.fid)); !r->fid) {
			r->respond(Enofid);
			return;
		}
		if((r->fid->omode&3) != (uint8_t(OMode::WRITE)) && (r->fid->omode&3) != (uint8_t(OMode::RDWR))) {
			r->respond("write on fid not opened for writing");
			return;
		}
		if(!p9conn->srv->write) {
            r->respond(Enofunc);
			return;
		}
		p9conn->srv->write(r);
		break;
	case FType::TWStat:
		if(r->fid = decltype(r->fid)(p9conn->fidmap.get( r->ifcall.hdr.fid)); !r->fid) {
			r->respond(Enofid);
			return;
		}
		if(~r->ifcall.twstat.stat.type) {
			r->respond("wstat of type");
			return;
		}
		if(~r->ifcall.twstat.stat.dev) {
			r->respond("wstat of dev");
			return;
		}
		if(~r->ifcall.twstat.stat.qid.type || (ulong)~r->ifcall.twstat.stat.qid.version || ~r->ifcall.twstat.stat.qid.path) {
			r->respond("wstat of qid");
			return;
		}
		if(r->ifcall.twstat.stat.muid && r->ifcall.twstat.stat.muid[0]) {
			r->respond("wstat of muid");
			return;
		}
		if(~r->ifcall.twstat.stat.mode && ((r->ifcall.twstat.stat.mode&(uint32_t)(DMode::DIR))>>24) != (r->fid->qid.type&uint8_t(QType::DIR))) {
			r->respond("wstat on DMDIR bit");
			return;
		}
		if(!p9conn->srv->wstat) {
			r->respond(Enofunc);
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
Req9::respond(const char *error) {
	Conn9 *p9conn;
	int msize;

	p9conn = conn;

	switch(ifcall.hdr.type) {
	case FType::TVersion:
		assert(error == nullptr);
		free(ifcall.version.version);

		concurrency::threadModel->lock(&p9conn->rlock);
		concurrency::threadModel->lock(&p9conn->wlock);
		msize = ixp::min<int>(ofcall.version.size(), maximum::Msg);
		p9conn->rmsg.data = (decltype(p9conn->rmsg.data))ixp::erealloc(p9conn->rmsg.data, msize);
		p9conn->wmsg.data = (decltype(p9conn->wmsg.data))ixp::erealloc(p9conn->wmsg.data, msize);
		p9conn->rmsg.size = msize;
		p9conn->wmsg.size = msize;
		concurrency::threadModel->unlock(&p9conn->wlock);
		concurrency::threadModel->unlock(&p9conn->rlock);
        ofcall.version.setSize(msize);
		break;
	case FType::TAttach:
		if(error)
			destroyfid(p9conn, fid->fid);
		free(ifcall.tattach.uname);
		free(ifcall.tattach.aname);
		break;
	case FType::TOpen:
	case FType::TCreate:
		if(!error) {
			ofcall.ropen.iounit = p9conn->rmsg.size - 24;
			fid->iounit = ofcall.ropen.iounit;
			fid->omode = ifcall.topen.mode;
			fid->qid = ofcall.ropen.qid;
		}
		free(ifcall.tcreate.name);
		break;
	case FType::TWalk:
		if(error || ofcall.rwalk.size() < ifcall.twalk.size()) {
			if(ifcall.hdr.fid != ifcall.twalk.newfid && newfid)
				destroyfid(p9conn, newfid->fid);
			if(!error && ofcall.rwalk.empty()) {
				error = Enofile.c_str();
            }
		}else{
            if (ofcall.rwalk.empty()) {
				newfid->qid = fid->qid;
            } else {
				newfid->qid = ofcall.rwalk.wqid[ofcall.rwalk.size()-1];
            }
		}
		free(*ifcall.twalk.wname);
		break;
	case FType::TWrite:
		free(ifcall.twrite.data);
		break;
	case FType::TRemove:
		if(fid)
			destroyfid(p9conn, fid->fid);
		break;
	case FType::TClunk:
		if(fid)
			destroyfid(p9conn, fid->fid);
		break;
	case FType::TFlush:
		if (oldreq = decltype(oldreq)( p9conn->tagmap.get(ifcall.tflush.oldtag)); oldreq) 
            oldreq->respond(Eintr);
		break;
	case FType::TWStat:
		Stat::free(&ifcall.twstat.stat);
		break;
	case FType::TRead:
	case FType::TStat:
		break;		
	/* Still to be implemented: auth */
	default:
		if(!error)
			assert(!"Respond called on unsupported fcall type");
		break;
	}

	ofcall.hdr.tag = ifcall.hdr.tag;

    if (!error) {
        ofcall.setType(FType(((uint8_t)ifcall.getType()) + 1));
    } else {
        ofcall.setType(FType::RError);
		ofcall.error.ename = (char*)error;
	}

	if(printfcall)
		printfcall(&ofcall);

    p9conn->tagmap.rm(ifcall.hdr.tag);;

	if(p9conn->conn) {
		concurrency::threadModel->lock(&p9conn->wlock);
		msize = fcall2msg(&p9conn->wmsg, &ofcall);
		if(sendmsg(p9conn->conn->fd, &p9conn->wmsg) != msize)
			hangup(p9conn->conn);
		concurrency::threadModel->unlock(&p9conn->wlock);
	}

	switch(ofcall.hdr.type) {
	case FType::RStat:
		free(ofcall.rstat.stat);
		break;
	case FType::RRead:
		free(ofcall.rread.data);
		break;
    default:
        break;
	}
	//free(req);
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
	flush_req->ifcall.setType(FType::TFlush);
	flush_req->ifcall.setNoTag();
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
	clunk_req->ifcall.setType(FType::TClunk);
	clunk_req->ifcall.setNoTag();
	clunk_req->ifcall.setFid(fid->fid);
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
        p9conn->fidmap.exec(voidfid, &req);
		p9conn->tagmap.exec(voidrequest, &req);
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

        p9conn->tagmap.init(p9conn->taghash, nelem(p9conn->taghash));
        p9conn->fidmap.init(p9conn->fidhash, nelem(p9conn->fidhash));
        concurrency::threadModel->initmutex(&p9conn->rlock);
        concurrency::threadModel->initmutex(&p9conn->wlock);

        c->srv->listen(fd, p9conn, handlefcall, cleanupconn);
    }
}
} // end namespace ixp
