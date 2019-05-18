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
        concurrency::Locker<Mutex> theLock(p9conn->getWLock());
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
    if (auto srv = this->conn.getSrv(); srv) {
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

    p9conn->getRLock().lock();
    if (c->recvmsg(p9conn->getRMsg()) == 0) {
        p9conn->getRLock().unlock();
        hangup(c);
        return;
    }
    if (p9conn->getRMsg().unpack(fcall) == 0) {
        p9conn->getRLock().unlock();
        hangup(c);
        return;
    }
    p9conn->getRLock().unlock();

    p9conn->operator++();
    Req9 req;
    req.setConn(p9conn);
	req.srv = p9conn->getSrv();
    req.setIFcall(fcall);
    p9conn->setConn(c);

    if (auto result = p9conn->getTagMap().emplace(fcall.getTag(), req); result.second) {
        result.first->second.handle();
    } else {
        req.respond(Eduptag);
    }
}
Req9*
Conn9::retrieveTag(uint16_t id) {
    return _tagmap.get(id);
}

Fid*
Conn9::retrieveFid(int id) {
    return _fidmap.get(id);
}
void
Req9::handle() {
    auto& p9conn = *_conn;

	if(printfcall) {
		printfcall(&getIFcall());
    }
    static std::map<FType, std::function<void()>> dispatchTable = {
        { FType::TVersion, 
            [this]() {
                static std::string str9p("9P");
                static std::string str9p2000("9P2000");
                static std::string strUnknown("unknown");
                std::string ver(getIFcall().version.getVersion());
                if(!strcmp(ver.c_str(), str9p.c_str())) {
                    getOFcall().version.setVersion(str9p.data());
                } else if(!strcmp(ver.c_str(), str9p2000.c_str())) {
                    getOFcall().version.setVersion(str9p2000.data());
                } else {
                    getOFcall().version.setVersion(strUnknown.data());
                }
                getOFcall().version.setSize(getIFcall().version.size());
                respond(nullptr);
            } },
        {FType::TAttach, 
            [&p9conn, srv = p9conn.getSrv(), this]() {
                auto newfid = createfid(p9conn.getFidMap(), getIFcall().getFid(), p9conn);
                fid = newfid;
                if (!fid) {
                    respond(Edupfid);
                } else {
                    /* attach is a required function */
                    srv->attach(this);
                }
            } },
        { FType::TClunk, 
            [&p9conn, srv = p9conn.getSrv(), this]() {
                if (fid = p9conn.retrieveFid(getIFcall().getFid()); !fid) {
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
            [&p9conn, srv = p9conn.getSrv(), this]() {
                if (oldreq = p9conn.retrieveTag(getIFcall().tflush.getOldTag()); !oldreq) {
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
            [&p9conn, srv = p9conn.getSrv(), this]() {
                if (fid = p9conn.retrieveFid(getIFcall().getFid()); !fid) {
                    respond(Enofid);
                } else if (fid->omode != -1) {
                    respond(Eopen);
                } else if(!(fid->qid.getType()&uint8_t(QType::DIR))) {
                    respond(Enotdir);
                } else if(!p9conn.getSrv()->create) {
                    respond(Enofunc);
                } else {
                    srv->create(this);
                }
            } },
        { FType::TOpen, 
            [&p9conn, srv = p9conn.getSrv(), this]() {
                if (fid = p9conn.retrieveFid(getIFcall().getFid()); !fid) {
                    respond(Enofid);
                } else if ((fid->qid.getType()&uint8_t(QType::DIR)) && (getIFcall().getTopen().getMode()|uint8_t(OMode::RCLOSE)) != (uint8_t(OMode::READ)|uint8_t(OMode::RCLOSE))) {
                    respond(Eisdir);
                } else if (getOFcall().ropen.setQid(fid->qid); !p9conn.getSrv()->open) {
                    respond(Enofunc);
                } else {
                    srv->open(this);
                }
            } },
        { FType::TRead, 
            [&p9conn, srv = p9conn.getSrv(), this]() {
                if (fid = p9conn.retrieveFid(getIFcall().getFid()); !fid) {
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
            [&p9conn, srv = p9conn.getSrv(), this]() {
                if (fid = p9conn.retrieveFid(getIFcall().getFid()); !fid) {
                    respond(Enofid);
                } else if (!srv->remove) {
                    respond(Enofunc);
                } else {
                    srv->remove(this);
                }
            } },
        { FType::TStat, 
            [&p9conn, srv = p9conn.getSrv(), this]() {
                if (fid = p9conn.retrieveFid(getIFcall().getFid()); !fid) {
                    respond(Enofid);
                } else if (!srv->stat) {
                    respond(Enofunc);
                } else {
                    srv->stat(this);
                }
            } },
        { FType::TWalk,
            [&p9conn, srv = p9conn.getSrv(), this]() {
                if (fid = p9conn.retrieveFid(getIFcall().getFid()); !fid) {
                    respond(Enofid);
                    return;
                }
                if(fid->omode != -1) {
                    respond("cannot walk from an open fid");
                    return;
                }
                if(getIFcall().twalk.size() && !(fid->qid.getType()&uint8_t(QType::DIR))) {
                    respond(Enotdir);
                    return;
                }
                if((getIFcall().getFid() != getIFcall().twalk.getNewFid())) {
                    if (newfid = createfid(p9conn.getFidMap(), getIFcall().twalk.getNewFid(), p9conn); !newfid) {
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
            [&p9conn, srv = p9conn.getSrv(), this]() {
                if (fid = p9conn.retrieveFid(getIFcall().getFid()); !fid) {
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
            [&p9conn, srv = p9conn.getSrv(), this]() {
                if (fid = p9conn.retrieveFid(getIFcall().getFid()); !fid) {
                    respond(Enofid);
                } else if(~getIFcall().twstat.getStat().getType()) {
                    respond("wstat of type");
                } else if(~getIFcall().twstat.getStat().getDev()) {
                    respond("wstat of dev");
                } else if(~getIFcall().twstat.getStat().getQid().getType() || (ulong)~getIFcall().twstat.getStat().getQid().getVersion() || ~getIFcall().twstat.getStat().getQid().getPath()) {
                    respond("wstat of qid");
                } else if(getIFcall().twstat.getStat().getMuid() && getIFcall().twstat.getStat().getMuid()[0]) {
                    respond("wstat of muid");
                } else if(~getIFcall().twstat.getStat().getMode() && ((getIFcall().twstat.getStat().getMode()&(uint32_t)(DMode::DIR))>>24) != (fid->qid.getType()&uint8_t(QType::DIR))) {
                    respond("wstat on DMDIR bit");
                } else if(!srv->wstat) {
                    respond(Enofunc);
                } else {
                    srv->wstat(this);
                }
            } },
    };
    if (auto ptr = dispatchTable.find(getIFcall().getType()); ptr == dispatchTable.end()) {
        respond(Enofunc);
    } else {
        ptr->second();
    }
}

/**
 * Function: respond
 *
 * Sends a response to the given request. The response is
 * constructed from the P<getOFcall()> member of the P<req> parameter, or
 * from the P<error> parameter if it is non-null. In the latter
 * case, the response is of type RError, while in any other case it
 * is of the same type as P<req>->P<getOFcall()>, which must match the
 * request type in P<req>->P<getIFcall()>.
 *
 * See also:
 *	T<Req9>, V<printfcall>
 */
void
Req9::respond(const char *error) {
	Conn9 *p9conn;
	int msize;

	p9conn = _conn;

	switch(getIFcall().getType()) {
	case FType::TVersion:
		assert(error == nullptr);
		free(getIFcall().version.getVersion());
        {
            concurrency::Locker<Mutex> theRlock(p9conn->getRLock());
            concurrency::Locker<Mutex> theWlock(p9conn->getWLock());
		    msize = jyq::min<int>(getOFcall().version.size(), maximum::Msg);
            p9conn->alloc(msize);
        }
        getOFcall().version.setSize(msize);
		break;
	case FType::TAttach:
		if(error) {
            destroyfid(*p9conn, fid->fid);
        }
		free(getIFcall().tattach.getUname());
		free(getIFcall().tattach.getAname());
		break;
	case FType::TOpen:
	case FType::TCreate:
		if(!error) {
			getOFcall().ropen.setIoUnit(p9conn->getRMsg().size() - 24);
			fid->iounit = getOFcall().ropen.getIoUnit();
			fid->omode = getIFcall().getTopen().getMode();
			fid->qid = getOFcall().ropen.getQid();
		}
		free(getIFcall().tcreate.getName());
		break;
	case FType::TWalk:
		if(error || getOFcall().rwalk.size() < getIFcall().twalk.size()) {
			if(getIFcall().getFid() != getIFcall().twalk.getNewFid() && newfid) {
				destroyfid(*p9conn, newfid->fid);
            }
			if(!error && getOFcall().rwalk.empty()) {
				error = Enofile.c_str();
            }
		}else{
            if (getOFcall().rwalk.empty()) {
				newfid->qid = fid->qid;
            } else {
				newfid->qid = getOFcall().rwalk.getWqid()[getOFcall().rwalk.size()-1];
            }
		}
		free(*getIFcall().twalk.getWname());
		break;
	case FType::TWrite:
		free(getIFcall().getTWrite().getData());
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
        if (oldreq = p9conn->retrieveTag(getIFcall().tflush.getOldTag()); oldreq) {
            oldreq->respond(Eintr);
        }
		break;
	case FType::TWStat:
		//Stat::free(&getIFcall().twstat.getStat());
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

    getOFcall().setTag(getIFcall().getTag());

    if (!error) {
        getOFcall().setType(FType(((uint8_t)getIFcall().getType()) + 1));
    } else {
        getOFcall().setType(FType::RError);
		getOFcall().error.setEname((char*)error);
	}

	if(printfcall) {
		printfcall(&getOFcall());
    }

    p9conn->removeTag(getIFcall().getTag());

    if (p9conn->getConn()) {
        concurrency::Locker<Mutex> theLock(p9conn->getWLock());
        msize = p9conn->getWMsg().pack(getOFcall());
        if (p9conn->getConn()->getConnection().sendmsg(p9conn->getWMsg()) != msize) {
			hangup(p9conn->getConn());
        }
	}

	switch(getOFcall().getType()) {
	case FType::RStat:
		free(getOFcall().rstat.getStat());
		break;
	case FType::RRead:
		free(getOFcall().getRRead().getData());
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
    p9conn->setConn(nullptr);
    ReqList collection;
    if (p9conn->referenceCountGreaterThan(1)) {
        p9conn->fidExec<ReqList&>([](auto context, Fid::Map::iterator arg) {
                ++arg->second.conn;
                context.emplace_back();
                context.back().getIFcall().setType(FType::TClunk);
                context.back().getIFcall().setNoTag();
                context.back().getIFcall().setFid(arg->second.fid);
                context.back().fid = &arg->second;
                context.back().setConn(&arg->second.conn);
                }, collection);
        p9conn->tagExec<ReqList>([](auto context, Conn9::TagMap::iterator arg) {
                    arg->second.getConn()->operator++();
                    context.emplace_back();
                    context.back().getIFcall().setType(FType::TFlush);
                    context.back().getIFcall().setNoTag();
                    context.back().getIFcall().tflush.setOldTag(arg->second.getIFcall().getTag());
                    context.back().setConn(arg->second.getConn());
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
        p9conn.setSrv(std::any_cast<Srv9*>(aux));
        p9conn.alloc(1024);
        //p9conn.rmsg.alloc(1024);
        //p9conn.wmsg.alloc(1024);
        srv.listen(fd, &p9conn, handlefcall, cleanupconn);
    }
}

void
Conn9::alloc(uint n) {
    _rmsg.alloc(n);
    _wmsg.alloc(n);
}
} // end namespace jyq
