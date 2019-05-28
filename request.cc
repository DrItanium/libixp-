/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
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
    auto wlock = p9conn->getWriteLock();
    p9conn->operator--();
    if (p9conn->referenceCountGreaterThan(0)) {
        return;
    }
}
Fid::Fid(uint32_t f, Conn9& c) : _fid(f), _omode(-1), _conn(c) {
    ++_conn;
}

static Fid* 
createfid(Fid::Map& map, int fid, Conn9& p9conn) {
    if (auto result = map.emplace(std::make_pair(fid, Fid(fid, p9conn))); result.second) {
        return &result.first->second;
    } else {
        return nullptr;
    }
}
Fid::~Fid() {
    if (auto srv = this->getConn().getSrv(); srv) {
       if (srv->freefid) {
           srv->freefid(this);
       }
    }
    decref_p9conn(&this->getConn()); // this should not be done like this
}
static bool 
destroyfid(Conn9& p9conn, ulong fid) {
    return p9conn.removeFid(fid);
}

static void
handlefcall(Conn *c) {
	Fcall fcall;

    auto p9conn = c->unpackAux<Conn9*>();
    auto rlock = p9conn->getReadLock();
    if (c->recvmsg(p9conn->getRMsg()) == 0) {
        rlock.unlock();
        hangup(c);
        return;
    }
    if (p9conn->getRMsg().unpack(fcall) == 0) {
        rlock.unlock();
        hangup(c);
        return;
    }
    rlock.unlock();

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
                std::string ver(getIFcall().getVersion().getVersion());
                if (ver == str9p) {
                    getOFcall().getVersion().setVersion(str9p.data());
                } else if(ver == str9p2000) {
                    getOFcall().getVersion().setVersion(str9p2000.data());
                } else {
                    getOFcall().getVersion().setVersion(strUnknown.data());
                }
                getOFcall().getVersion().setSize(getIFcall().getVersion().size());
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
                if (oldreq = p9conn.retrieveTag(getIFcall().getTflush().getOldTag()); !oldreq) {
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
                } else if (fid->getOmode() != -1) {
                    respond(Eopen);
                } else if(!(fid->getQid().getType()&uint8_t(QType::DIR))) {
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
                } else if ((fid->getQid().getType()&uint8_t(QType::DIR)) && (getIFcall().getTopen().getMode()|uint8_t(OMode::RCLOSE)) != (uint8_t(OMode::READ)|uint8_t(OMode::RCLOSE))) {
                    respond(Eisdir);
                } else if (getOFcall().getRopen().setQid(fid->getQid()); !p9conn.getSrv()->open) {
                    respond(Enofunc);
                } else {
                    srv->open(this);
                }
            } },
        { FType::TRead, 
            [&p9conn, srv = p9conn.getSrv(), this]() {
                if (fid = p9conn.retrieveFid(getIFcall().getFid()); !fid) {
                    respond(Enofid);
                } else if (auto omode = fid->getOmode(); omode == -1 || omode == uint8_t(OMode::WRITE)) {
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
                if(fid->getOmode() != -1) {
                    respond("cannot walk from an open fid");
                    return;
                }
                if(getIFcall().getTwalk().size() && !(fid->getQid().getType()&uint8_t(QType::DIR))) {
                    respond(Enotdir);
                    return;
                }
                if((getIFcall().getFid() != getIFcall().getTwalk().getNewFid())) {
                    if (newfid = createfid(p9conn.getFidMap(), getIFcall().getTwalk().getNewFid(), p9conn); !newfid) {
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
                } else if(auto omode = fid->getOmode(); (omode&3) != (uint8_t(OMode::WRITE)) && (omode&3) != (uint8_t(OMode::RDWR))) {
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
                } else if(~getIFcall().getTwstat().getStat().getType()) {
                    respond("wstat of type");
                } else if(~getIFcall().getTwstat().getStat().getDev()) {
                    respond("wstat of dev");
                } else if(~getIFcall().getTwstat().getStat().getQid().getType() || (ulong)~getIFcall().getTwstat().getStat().getQid().getVersion() || ~getIFcall().getTwstat().getStat().getQid().getPath()) {
                    respond("wstat of qid");
                } else if(getIFcall().getTwstat().getStat().getMuid() && getIFcall().getTwstat().getStat().getMuid()[0]) {
                    respond("wstat of muid");
                } else if(~getIFcall().getTwstat().getStat().getMode() && ((getIFcall().getTwstat().getStat().getMode()&(uint32_t)(DMode::DIR))>>24) != (fid->getQid().getType()&uint8_t(QType::DIR))) {
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
        if (!error) {
            throw Exception("error is null!");
        }
        getIFcall().getVersion().getVersion().clear();
        {
            auto theRlock = p9conn->getReadLock();
            auto theWlock = p9conn->getWriteLock();
		    msize = jyq::min<int>(getOFcall().getVersion().size(), maximum::Msg);
            p9conn->alloc(msize);
        }
        getOFcall().getVersion().setSize(msize);
		break;
	case FType::TAttach:
		if(error) {
            destroyfid(*p9conn, fid->getId());
        }
        getIFcall().getTattach().reset();
		break;
	case FType::TOpen:
	case FType::TCreate:
		if(!error) {
			getOFcall().getRopen().setIoUnit(p9conn->getRMsg().size() - 24);
            fid->setIoUnit(getOFcall().getRopen().getIoUnit());
            fid->setOmode(getIFcall().getTopen().getMode());
			fid->setQid(getOFcall().getRopen().getQid());
		}
        getIFcall().getTcreate().reset();
		break;
	case FType::TWalk:
		if(error || getOFcall().getRwalk().size() < getIFcall().getTwalk().size()) {
			if(getIFcall().getFid() != getIFcall().getTwalk().getNewFid() && newfid) {
				destroyfid(*p9conn, newfid->getId());
            }
			if(!error && getOFcall().getRwalk().empty()) {
				error = Enofile.c_str();
            }
		}else{
            if (getOFcall().getRwalk().empty()) {
                newfid->setQid(fid->getQid());
            } else {
                newfid->setQid(getOFcall().getRwalk().getWqid()[getOFcall().getRwalk().size()-1]);
            }
		}
        getIFcall().getTwalk().getWname().fill("");
		break;
	case FType::TWrite:
        getIFcall().getTWrite().reset();
		break;
	case FType::TRemove:
		if(fid) {
			destroyfid(*p9conn, fid->getId());
        }
		break;
	case FType::TClunk:
		if(fid) {
			destroyfid(*p9conn, fid->getId());
        }
		break;
	case FType::TFlush:
        if (oldreq = p9conn->retrieveTag(getIFcall().getTflush().getOldTag()); oldreq) {
            oldreq->respond(Eintr);
        }
		break;
	case FType::TWStat:
		//Stat::free(&getIFcall().getTwstat().getStat());
		break;
	case FType::TRead:
	case FType::TStat:
		break;		
	/* Still to be implemented: auth */
	default:
		if(!error) {
            throw Exception("Respond called on unsupported fcall type!");
        }
		break;
	}

    getOFcall().setTag(getIFcall().getTag());

    if (!error) {
        getOFcall().setType(FType(((uint8_t)getIFcall().getType()) + 1));
    } else {
        getOFcall().setType(FType::RError);
		getOFcall().getError().setEname((char*)error);
	}

	if(printfcall) {
		printfcall(&getOFcall());
    }

    p9conn->removeTag(getIFcall().getTag());

    if (p9conn->getConn()) {

        auto theLock = p9conn->getWriteLock();
        msize = p9conn->getWMsg().pack(getOFcall());
        if (p9conn->sendmsg() != msize) {
			hangup(p9conn->getConn());
        }
	}

	switch(getOFcall().getType()) {
	case FType::RStat:
        getOFcall().getRstat().purgeStat();
		break;
	case FType::RRead:
        getOFcall().getRRead().reset();
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
    auto p9conn = c->unpackAux<Conn9*>();
    p9conn->setConn(nullptr);
    ReqList collection;
    if (p9conn->referenceCountGreaterThan(1)) {
        p9conn->fidExec<ReqList&>([](auto context, Fid::Map::iterator arg) {
                ++arg->second.getConn();
                context.emplace_back();
                context.back().getIFcall().setType(FType::TClunk);
                context.back().getIFcall().setNoTag();
                context.back().getIFcall().setFid(arg->second.getId());
                context.back().fid = &arg->second;
                context.back().setConn(&arg->second.getConn());
                }, collection);
        p9conn->tagExec<ReqList>([](auto context, Conn9::TagMap::iterator arg) {
                    arg->second.getConn()->operator++();
                    context.emplace_back();
                    context.back().getIFcall().setType(FType::TFlush);
                    context.back().getIFcall().setNoTag();
                    context.back().getIFcall().getTflush().setOldTag(arg->second.getIFcall().getTag());
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
        p9conn.setSrv(unpackAux<Srv9*>());
        p9conn.alloc(1024);
        srv.listen(fd, &p9conn, handlefcall, cleanupconn);
    }
}

void
Conn9::alloc(uint n) {
    _rmsg.alloc(n);
    _wmsg.alloc(n);
}
} // end namespace jyq
