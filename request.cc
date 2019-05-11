/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <utility>
#include "Msg.h"
#include "jyq.h"
#include "map.h"
#include "argv.h"
#include "Conn.h"
#include "socket.h"
#include "Server.h"
#include "PrintFunctions.h"


namespace jyq {

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



static void
decref_p9conn(Conn9 *p9conn) {
    {
        concurrency::Locker<Mutex> theLock(p9conn->wlock);
        p9conn->operator--();
        if (p9conn->referenceCountGreaterThan(0)) {
            return;
        }
    }
    // this is dumb, leave the code disabled for now. What needs to happen is that
    // the reference count associated with the connection needs to cause destruction
    // once the reference count equals zero. Right now, this is done manually, it should
    // be done through C++ destructors for maximum cleanliness
    //
    // Right now, the fid object holds onto a reference instead of a pointer. Thus
    // we could replace it with a std::shared_ptr if we desired. That way the destruction of the
    // final shared_ptr would yield destruction
    


	//assert(p9conn->conn == nullptr);

	//concurrency::threadModel->mdestroy(&p9conn->rlock);
	//concurrency::threadModel->mdestroy(&p9conn->wlock);

    //if (p9conn->rmsg.data) {
    //    delete[] p9conn->rmsg.data;
    //}
    //if (p9conn->wmsg.data) {
    //    delete[] p9conn->wmsg.data;
    //}
	//free(p9conn); // don't like this at all :(
}
Fid::Fid(uint32_t f, Fid::Map& m, Conn9& c) : fid(f), omode(-1), map(m), conn(c) {
    ++conn;
}

static Fid* 
createfid(Fid::Map& map, int fid, Conn9& p9conn) {
    if (auto result = map.emplace(std::make_pair(fid, Fid(fid, map, p9conn))); result.second) {
        return &result.first->second;
    } else {
        return nullptr;
    }
}
Fid::~Fid() {
    if (auto srv = this->conn.srv; srv) {
       if (srv->freefid) {
           srv->freefid(this);
       }
    }
    decref_p9conn(&this->conn); // this should not be done like this
}
static bool 
destroyfid(Conn9& p9conn, ulong fid) {
    return p9conn.removeFid(fid);
}

static void
handlefcall(Conn *c) {
	Fcall fcall;

	auto p9conn = std::any_cast<Conn9*>(c->aux);

    p9conn->rlock.lock();
    if (c->recvmsg(p9conn->rmsg) == 0) {
        p9conn->rlock.unlock();
        hangup(c);
        return;
    }
	if(msg2fcall(&p9conn->rmsg, &fcall) == 0) {
        p9conn->rlock.unlock();
        hangup(c);
        return;
    }
    p9conn->rlock.unlock();

    p9conn->operator++();
    Req9 req;
	req.conn = p9conn;
	req.srv = p9conn->srv;
	req.ifcall = fcall;
	p9conn->conn = c;

    if (auto result = p9conn->tagmap.emplace(fcall.hdr.tag, req); result.second) {
        result.first->second.handle();
    } else {
        req.respond(Eduptag);
    }
}
Req9*
Conn9::retrieveTag(uint16_t id) {
    return tagmap.get(id);
}

Fid*
Conn9::retrieveFid(int id) {
    return fidmap.get(id);
}
void
Req9::handle() {
	auto& p9conn = *conn;

	if(printfcall) {
		printfcall(&ifcall);
    }
    static std::map<FType, std::function<void()>> dispatchTable = {
        { FType::TVersion, 
            [this]() {
                std::string ver(ifcall.version.version);
                if(!strcmp(ver.c_str(), "9P")) {
                    ofcall.version.version = "9P";
                } else if(!strcmp(ver.c_str(), "9P2000")) {
                    ofcall.version.version = "9P2000";
                } else {
                    ofcall.version.version = "unknown";
                }
                ofcall.version.setSize(ifcall.version.size());
                respond(nullptr);
            } },
        {FType::TAttach, 
            [&p9conn, srv = p9conn.srv, this]() {
                auto newfid = createfid(p9conn.fidmap, ifcall.getFid(), p9conn);
                fid = newfid;
                if (!fid) {
                    respond(Edupfid);
                } else {
                    /* attach is a required function */
                    srv->attach(this);
                }
            } },
        { FType::TClunk, 
            [&p9conn, srv = p9conn.srv, this]() {
                if (fid = p9conn.retrieveFid(ifcall.getFid()); !fid) {
                    respond(Enofid);
                } else {
                    if(!srv->clunk) {
                        respond(nullptr);
                        return;
                    }
                    srv->clunk(this);
                }
            } },
        { FType::TFlush, 
            [&p9conn, srv = p9conn.srv, this]() {
                if (oldreq = p9conn.retrieveTag(ifcall.tflush.oldtag); !oldreq) {
                    respond(Enotag);
                } else {
                    if(!srv->flush) {
                        respond(Enofunc);
                        return;
                    }
                    srv->flush(this);
                }
            } },
        {FType::TCreate, 
            [&p9conn, srv = p9conn.srv, this]() {
                if (fid = p9conn.retrieveFid(ifcall.getFid()); !fid) {
                    respond(Enofid);
                } else if (fid->omode != -1) {
                    respond(Eopen);
                } else if(!(fid->qid.type&uint8_t(QType::DIR))) {
                    respond(Enotdir);
                } else if(!p9conn.srv->create) {
                    respond(Enofunc);
                } else {
                    srv->create(this);
                }
            } },
        { FType::TOpen, 
            [&p9conn, srv = p9conn.srv, this]() {
                if (fid = p9conn.retrieveFid(ifcall.getFid()); !fid) {
                    respond(Enofid);
                } else if ((fid->qid.type&uint8_t(QType::DIR)) && (ifcall.topen.getMode()|uint8_t(OMode::RCLOSE)) != (uint8_t(OMode::READ)|uint8_t(OMode::RCLOSE))) {
                    respond(Eisdir);
                } else if (ofcall.ropen.qid = fid->qid; !p9conn.srv->open) {
                    respond(Enofunc);
                } else {
                    srv->open(this);
                }
            } },
        { FType::TRead, 
            [&p9conn, srv = p9conn.srv, this]() {
                if (fid = p9conn.retrieveFid(ifcall.getFid()); !fid) {
                    respond(Enofid);
                } else if (fid->omode == -1 || fid->omode == uint8_t(OMode::WRITE)) {
                    respond(Enoread);
                } else if (!srv->read) {
                    respond(Enofunc); 
                } else {
                    srv->read(this);
                }
            } },
        { FType::TRemove, 
            [&p9conn, srv = p9conn.srv, this]() {
                if (fid = p9conn.retrieveFid(ifcall.getFid()); !fid) {
                    respond(Enofid);
                } else if (!srv->remove) {
                    respond(Enofunc);
                } else {
                    srv->remove(this);
                }
            } },
        { FType::TStat, 
            [&p9conn, srv = p9conn.srv, this]() {
                if (fid = p9conn.retrieveFid(ifcall.getFid()); !fid) {
                    respond(Enofid);
                } else if (!srv->stat) {
                    respond(Enofunc);
                } else {
                    srv->stat(this);
                }
            } },
        { FType::TWalk,
            [&p9conn, srv = p9conn.srv, this]() {
                if (fid = p9conn.retrieveFid(ifcall.getFid()); !fid) {
                    respond(Enofid);
                    return;
                }
                if(fid->omode != -1) {
                    respond("cannot walk from an open fid");
                    return;
                }
                if(ifcall.twalk.size() && !(fid->qid.type&uint8_t(QType::DIR))) {
                    respond(Enotdir);
                    return;
                }
                if((ifcall.getFid() != ifcall.twalk.getNewFid())) {
                    if (newfid = createfid(p9conn.fidmap, ifcall.twalk.getNewFid(), p9conn); !newfid) {
                        respond(Edupfid);
                        return;
                    }
                } else {
                    newfid = fid;
                }
                if(!srv->walk) {
                    respond(Enofunc);
                } else {
                    srv->walk(this);
                }

            } },
        { FType::TWrite,
            [&p9conn, srv = p9conn.srv, this]() {
                if (fid = p9conn.retrieveFid(ifcall.getFid()); !fid) {
                    respond(Enofid);
                } else if((fid->omode&3) != (uint8_t(OMode::WRITE)) && (fid->omode&3) != (uint8_t(OMode::RDWR))) {
                    respond("write on fid not opened for writing");
                } else if(!srv->write) {
                    respond(Enofunc);
                } else {
                    srv->write(this);
                }
            }
        },
        { FType::TWStat,
            [&p9conn, srv = p9conn.srv, this]() {
                if (fid = p9conn.retrieveFid(ifcall.getFid()); !fid) {
                    respond(Enofid);
                } else if(~ifcall.twstat.getStat().type) {
                    respond("wstat of type");
                } else if(~ifcall.twstat.getStat().dev) {
                    respond("wstat of dev");
                } else if(~ifcall.twstat.getStat().qid.type || (ulong)~ifcall.twstat.getStat().qid.version || ~ifcall.twstat.getStat().qid.path) {
                    respond("wstat of qid");
                } else if(ifcall.twstat.getStat().muid && ifcall.twstat.getStat().muid[0]) {
                    respond("wstat of muid");
                } else if(~ifcall.twstat.getStat().mode && ((ifcall.twstat.getStat().mode&(uint32_t)(DMode::DIR))>>24) != (fid->qid.type&uint8_t(QType::DIR))) {
                    respond("wstat on DMDIR bit");
                } else if(!srv->wstat) {
                    respond(Enofunc);
                } else {
                    srv->wstat(this);
                }
            } },
    };
    if (auto ptr = dispatchTable.find(ifcall.getType()); ptr == dispatchTable.end()) {
        respond(Enofunc);
    } else {
        ptr->second();
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
        {
            concurrency::Locker<Mutex> theRlock(p9conn->rlock);
            concurrency::Locker<Mutex> theWlock(p9conn->wlock);
		    msize = jyq::min<int>(ofcall.version.size(), maximum::Msg);
            p9conn->rmsg.alloc(msize);
            p9conn->wmsg.alloc(msize);
        }
        ofcall.version.setSize(msize);
		break;
	case FType::TAttach:
		if(error) {
            destroyfid(*p9conn, fid->fid);
        }
		free(ifcall.tattach.uname);
		free(ifcall.tattach.aname);
		break;
	case FType::TOpen:
	case FType::TCreate:
		if(!error) {
			ofcall.ropen.iounit = p9conn->rmsg.size() - 24;
			fid->iounit = ofcall.ropen.iounit;
			fid->omode = ifcall.topen.getMode();
			fid->qid = ofcall.ropen.qid;
		}
		free(ifcall.tcreate.getName());
		break;
	case FType::TWalk:
		if(error || ofcall.rwalk.size() < ifcall.twalk.size()) {
			if(ifcall.hdr.fid != ifcall.twalk.getNewFid() && newfid) {
				destroyfid(*p9conn, newfid->fid);
            }
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
		free(ifcall.twrite.getData());
		break;
	case FType::TRemove:
		if(fid) {
			destroyfid(*p9conn, fid->fid);
        }
		break;
	case FType::TClunk:
		if(fid) {
			destroyfid(*p9conn, fid->fid);
        }
		break;
	case FType::TFlush:
        if (oldreq = p9conn->retrieveTag(ifcall.tflush.oldtag); oldreq) {
            oldreq->respond(Eintr);
        }
		break;
	case FType::TWStat:
		Stat::free(&ifcall.twstat.getStat());
		break;
	case FType::TRead:
	case FType::TStat:
		break;		
	/* Still to be implemented: auth */
	default:
		if(!error) {
			assert(!"Respond called on unsupported fcall type");
        }
		break;
	}

	ofcall.hdr.tag = ifcall.hdr.tag;

    if (!error) {
        ofcall.setType(FType(((uint8_t)ifcall.getType()) + 1));
    } else {
        ofcall.setType(FType::RError);
		ofcall.error.ename = (char*)error;
	}

	if(printfcall) {
		printfcall(&ofcall);
    }

    p9conn->removeTag(ifcall.hdr.tag);

	if(p9conn->conn) {
        concurrency::Locker<Mutex> theLock(p9conn->wlock);
		msize = fcall2msg(&p9conn->wmsg, &ofcall);
        if (p9conn->conn->getConnection().sendmsg(p9conn->wmsg) != msize) {
			hangup(p9conn->conn);
        }
	}

	switch(ofcall.hdr.type) {
	case FType::RStat:
		free(ofcall.rstat.getStat());
		break;
	case FType::RRead:
		free(ofcall.rread.getData());
		break;
    default:
        break;
	}
	//free(req);
	decref_p9conn(p9conn);
}

static void
cleanupconn(Conn *c) {
    using ReqList = std::list<Req9>;
    auto p9conn = std::any_cast<Conn9*>(c->aux);
	p9conn->conn = nullptr;
    ReqList collection;
    if (p9conn->referenceCountGreaterThan(1)) {
        p9conn->fidExec<ReqList&>([](auto context, Fid::Map::iterator arg) {
                ++arg->second.conn;
                context.emplace_back();
                context.back().ifcall.setType(FType::TClunk);
                context.back().ifcall.setNoTag();
                context.back().ifcall.setFid(arg->second.fid);
                context.back().fid = &arg->second;
                context.back().conn = &arg->second.conn;
                }, collection);
        p9conn->tagExec<ReqList>([](auto context, Conn9::TagMap::iterator arg) {
                    arg->second.conn->operator++();
                    context.emplace_back();
                    context.back().ifcall.setType(FType::TFlush);
                    context.back().ifcall.setNoTag();
                    context.back().ifcall.tflush.oldtag = arg->second.ifcall.hdr.tag;
                    context.back().conn = arg->second.conn;
                }, collection);
	}
    for (auto& req : collection) {
        req.handle();
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
 * disconnects, libjyq generates whatever flush and clunk events are
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
Conn::serve9conn() {

	if(auto fd = accept(_fd, nullptr, nullptr); fd < 0) {
		return;
    } else {
        Conn9 p9conn;
        ++p9conn;
        p9conn.srv = std::any_cast<decltype(p9conn.srv)>(this->aux);
        p9conn.rmsg.alloc(1024);
        p9conn.wmsg.alloc(1024);
        srv.listen(fd, &p9conn, handlefcall, cleanupconn);
    }
}
} // end namespace jyq
