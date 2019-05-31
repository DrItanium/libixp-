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



Fid::Fid(uint32_t f, std::shared_ptr<Conn9> c) : _fid(f), _omode(-1), _conn(c) {
}

static Fid* 
createfid(Fid::Map& map, int fid, std::shared_ptr<Conn9> p9conn) {
    if (auto result = map.emplace(std::make_pair(fid, Fid(fid, p9conn))); result.second) {
        return &result.first->second;
    } else {
        return nullptr;
    }
}
Fid::~Fid() {
    if (auto srv = this->getConn()->getSrv(); srv) {
       if (srv->freefid) {
           srv->freefid(this);
       }
    }
    //decref_p9conn(&this->getConn()); // this should not be done like this
}

void
Conn::handleFcall() {
	Fcall fcall;

    auto p9conn = this->unpackAux<std::shared_ptr<Conn9>>();
    auto rlock = p9conn->getReadLock();
    if (this->recvmsg(p9conn->getRMsg()) == 0) {
        rlock.unlock();
        // hangup(this); // NOPE!
        return;
    }
    if (p9conn->getRMsg().unpack(fcall) == 0) {
        rlock.unlock();
        // hangup(this); // NOPE!
        return;
    }
    rlock.unlock();

    //p9conn->operator++();
    Req9 req;
    req.setConn(p9conn);
    req.setSrv(p9conn->getSrv());
    req.setIFcall(fcall);
    p9conn->setConn(this);

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

	if(printfcall) {
		printfcall(&getIFcall());
    }
    getIFcall().visit([this, srv = _conn->getSrv()](auto&& value) {
                using K = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<K, FTWStat>) {
                    if (_fid = _conn->retrieveFid(value.getFid()); !_fid) {
                        respond(Enofid);
                    } else if(auto& twstat = value.getStat(); ~twstat.getType()) {
                        respond("wstat of type");
                    } else if(~twstat.getDev()) {
                        respond("wstat of dev");
                    } else if(auto& theQid = twstat.getQid(); ~theQid.getType() || (ulong)~theQid.getVersion() || ~theQid.getPath()) {
                        respond("wstat of qid");
                    } else if(!twstat.getMuid().empty()) {
                        respond("wstat of muid");
                    } else if(~twstat.getMode() && ((twstat.getMode()&(uint32_t)(DMode::DIR))>>24) != (_fid->getQid().getType()&uint8_t(QType::DIR))) {
                        respond("wstat on DMDIR bit");
                    } else if(!srv->wstat) {
                        respond(Enofunc);
                    } else {
                        srv->wstat(this);
                    }
                } else if constexpr (std::is_same_v<K, FVersion>) {
                    switch (value.getType()) {
                        case FType::TVersion:
                            {
                                static std::string str9p("9P");
                                static std::string str9p2000("9P2000");
                                static std::string strUnknown("unknown");
                                auto& iversion = getIFcall().getVersion();
                                std::string ver(iversion.getVersion());
                                auto& oversion = getOFcall().getVersion();
                                if (ver == str9p) {
                                    oversion.setVersion(str9p.data());
                                } else if(ver == str9p2000) {
                                    oversion.setVersion(str9p2000.data());
                                } else {
                                    oversion.setVersion(strUnknown.data());
                                }
                                oversion.setSize(iversion.size());
                                respond(nullptr);
                            }
                            break;
                        default:
                            respond(Enofunc);
                            break;
                    }
                } else if constexpr (std::is_same_v<K, FAttach>) {
                    switch (value.getType()) {
                        case FType::TAttach: 
                            {
                                auto newfid = createfid(_conn->getFidMap(), value.getFid(), _conn);
                                _fid = newfid;
                                if (!_fid) {
                                    respond(Edupfid);
                                } else {
                                    /* attach is a required function */
                                    srv->attach(this);
                                }
                            }
                            break;
                        default:
                            respond(Enofunc);
                            break;
                    }
                } else if constexpr (std::is_same_v<K, FTFlush>) {
                    if (_oldreq = _conn->retrieveTag(getIFcall().getTflush().getOldTag()); !_oldreq) {
                        respond(Enotag);
                    } else {
                        if(!srv->flush) {
                            respond(Enofunc);
                            return;
                        }
                        srv->flush(this);
                    }
        
                } else if constexpr (std::is_same_v<K, FTCreate>) {
                    switch (value.getType()) {
                        case FType::TOpen:
                            if (_fid = _conn->retrieveFid(getIFcall().getFid()); !_fid) {
                                respond(Enofid);
                            } else if ((_fid->getQid().getType()&uint8_t(QType::DIR)) && (getIFcall().getTopen().getMode()|uint8_t(OMode::RCLOSE)) != (uint8_t(OMode::READ)|uint8_t(OMode::RCLOSE))) {
                                respond(Eisdir);
                            } else if (getOFcall().getRopen().setQid(_fid->getQid()); !_conn->getSrv()->open) {
                                respond(Enofunc);
                            } else {
                                srv->open(this);
                            }
                            break;
                        case FType::TCreate:
                            if (_fid = _conn->retrieveFid(getIFcall().getFid()); !_fid) {
                                respond(Enofid);
                            } else if (_fid->getOmode() != -1) {
                                respond(Eopen);
                            } else if(!(_fid->getQid().getType()&uint8_t(QType::DIR))) {
                                respond(Enotdir);
                            } else if(!_conn->getSrv()->create) {
                                respond(Enofunc);
                            } else {
                                srv->create(this);
                            }
                            break;
                        default:
                            respond(Enofunc);
                            break;
                    }

                } else if constexpr (std::is_same_v<K, FFullHeader>) {
                    switch (value.getType()) {
                        case FType::TStat:
                            if (_fid = _conn->retrieveFid(getIFcall().getFid()); !_fid) {
                                respond(Enofid);
                            } else if (!srv->stat) {
                                respond(Enofunc);
                            } else {
                                srv->stat(this);
                            }
                            break;
                        case FType::TRemove:
                            if (_fid = _conn->retrieveFid(getIFcall().getFid()); !_fid) {
                                respond(Enofid);
                            } else if (!srv->remove) {
                                respond(Enofunc);
                            } else {
                                srv->remove(this);
                            }
                            break;
                        case FType::TClunk:
                            if (_fid = _conn->retrieveFid(getIFcall().getFid()); !_fid) {
                                respond(Enofid);
                            } else {
                                if(!srv->clunk) {
                                    respond(nullptr);
                                    return;
                                }
                                srv->clunk(this);
                            }
                            break;
                        default:
                            respond(Enofunc);
                            break;
                    }

                } else if constexpr (std::is_same_v<K, FIO>) {
                    switch (value.getType()) {
                        case FType::TWrite: 
                            if (_fid = _conn->retrieveFid(getIFcall().getFid()); !_fid) {
                                respond(Enofid);
                            } else if(auto omode = _fid->getOmode(); (omode&3) != (uint8_t(OMode::WRITE)) && (omode&3) != (uint8_t(OMode::RDWR))) {
                                respond("write on fid not opened for writing");
                            } else if(!srv->write) {
                                respond(Enofunc);
                            } else {
                                srv->write(this);
                            }
                            break;
                        case FType::TRead:
                            if (_fid = _conn->retrieveFid(getIFcall().getFid()); !_fid) {
                                respond(Enofid);
                            } else if (auto omode = _fid->getOmode(); omode == -1 || omode == uint8_t(OMode::WRITE)) {
                                respond(Enoread);
                            } else if (!srv->read) {
                                respond(Enofunc); 
                            } else {
                                srv->read(this);
                            }
                            break;
                        default:
                            respond(Enofunc);
                            break;
                    }
                } else if constexpr (std::is_same_v<K, FTWalk>) {
                    if (_fid = _conn->retrieveFid(getIFcall().getFid()); !_fid) {
                        respond(Enofid);
                        return;
                    }
                    if(_fid->getOmode() != -1) {
                        respond("cannot walk from an open fid");
                        return;
                    }
                    auto& itwalk = getIFcall().getTwalk();
                    if(itwalk.size() && !(_fid->getQid().getType()&uint8_t(QType::DIR))) {
                        respond(Enotdir);
                        return;
                    }
                    if((itwalk.getFid() != itwalk.getNewFid())) {
                        if (_newfid = createfid(_conn->getFidMap(), itwalk.getNewFid(), _conn); !_newfid) {
                            respond(Edupfid);
                            return;
                        }
                    } else {
                        _newfid = _fid;
                    }
                    if(!srv->walk) {
                        respond(Enofunc);
                    } else {
                        srv->walk(this);
                    }
                } else {
                    respond(Enofunc);
                }
            });
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

	auto p9conn = _conn;
    getIFcall().visit([this, &error, &p9conn](auto&& value) {
            using K = std::decay_t<decltype(value)>;
            // Still to be implemented: auth 
            if constexpr (std::is_same_v<K, FVersion>) {
                int msize = 0;
                if (error) {
                    throw Exception("error is defined!");
                }
                value.getVersion().clear();
                {
                    auto theRlock = p9conn->getReadLock();
                    auto theWlock = p9conn->getWriteLock();
                    msize = jyq::min<int>(getOFcall().getVersion().size(), maximum::Msg);
                    p9conn->alloc(msize);
                }
                getOFcall().getVersion().setSize(msize);
            } else if constexpr (std::is_same_v<K, FAttach>) {
                if(error) {
                    p9conn->removeFid(_fid->getId());
                }
                value.reset();
            } else if constexpr (std::is_same_v<K, FTCreate>) {
	    	    if(!error) {
	    	    	getOFcall().getRopen().setIoUnit(p9conn->getRMsg().size() - 24);
                    _fid->setIoUnit(getOFcall().getRopen().getIoUnit());
                    _fid->setOmode(value.getMode());
	    	    	_fid->setQid(getOFcall().getRopen().getQid());
	    	    }
                value.reset();
            } else if constexpr (std::is_same_v<K, FTWalk>) {
                if(error || getOFcall().getRwalk().size() < value.size()) {
                    if(value.getFid() != value.getNewFid() && _newfid) {
                        p9conn->removeFid(_newfid->getId());
                    }
                    if(!error && getOFcall().getRwalk().empty()) {
                        error = Enofile.c_str();
                    }
                }else{
                    if (getOFcall().getRwalk().empty()) {
                        _newfid->setQid(_fid->getQid());
                    } else {
                        _newfid->setQid(getOFcall().getRwalk().getWqid()[getOFcall().getRwalk().size()-1]);
                    }
                }
                value.getWname().fill("");
            } else if constexpr (std::is_same_v<K, FIO>) {
                switch (value.getType()) {
                    case FType::TWrite:
                        value.reset();
                        break;
                    case FType::TRead:
                    default:
                        break;
                }
            } else if constexpr (std::is_same_v<K, FFullHeader>) {
                switch (value.getType()) {
                    case FType::TRemove:
                    case FType::TClunk:
                        if (_fid) {
                            p9conn->removeFid(_fid->getId());
                        }
                        break;
                    case FType::TStat:
                    default:
                        break;
                }
            } else if constexpr (std::is_same_v<K, FTFlush>) {
                if (_oldreq= p9conn->retrieveTag(getIFcall().getTflush().getOldTag()); _oldreq) {
                    _oldreq->respond(Eintr);
                }
            } else if constexpr (std::is_same_v<K, FTWStat>) {
                value.getStat().reset();
                // Still to be implemented: auth
            } else {
                if (!error) {
                    throw Exception("Respond called on unsupported fcall type!");
                }
            }
            });

    getOFcall().setTag(getIFcall().getTag());

    if (!error) {
        getOFcall().reset(FType(((uint8_t)getIFcall().getType()) + 1));
    } else {
        getOFcall().reset(FType::RError);
		getOFcall().getError().setEname(error);
	}

	if(printfcall) {
		printfcall(&getOFcall());
    }

    p9conn->removeTag(getIFcall().getTag());

    if (p9conn->getConn()) {

        auto theLock = p9conn->getWriteLock();
        auto msize = p9conn->getWMsg().pack(getOFcall());
        if (p9conn->sendmsg() != msize) {
			//hangup(p9conn->getConn());
            //hmmm, how to describe that we did a hangup?
        }
	}
    getOFcall().visit([](auto&& value) {
                using K = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<K, FRStat>) {
                    value.purgeStat();
                } else if constexpr (std::is_same_v<K, FIO>) {
                    if (value.getType() == FType::RRead) {
                        value.reset();
                    }
                } else {
                    // do nothing
                }
            });
	//decref_p9conn(p9conn);
}

void
Conn::cleanup() {
    using ReqList = std::list<Req9>;
    auto p9conn = this->unpackAux<std::shared_ptr<Conn9>>();
    p9conn->setConn(nullptr);
    ReqList collection;
    if (p9conn.use_count() > 1) {
        p9conn->fidExec<ReqList&>([](auto context, Fid::Map::iterator arg) {
                context.emplace_back();
                context.back().getIFcall().reset(FType::TClunk);
                context.back().getIFcall().setNoTag();
                context.back().getIFcall().setFid(arg->second.getId());
                context.back().setFid(&arg->second);
                context.back().setConn(arg->second.getConn());
                }, collection);
        p9conn->tagExec<ReqList>([](auto context, Conn9::TagMap::iterator arg) {
                    context.emplace_back();
                    context.back().getIFcall().reset(FType::TFlush);
                    context.back().getIFcall().setNoTag();
                    context.back().getIFcall().getTflush().setOldTag(arg->second.getIFcall().getTag());
                    context.back().setConn(arg->second.getConn());
                }, collection);
	}
    for (auto& req : collection) {
        req.handle();
    }
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
        auto p9conn = std::make_shared<Conn9>();
        //++p9conn;
        p9conn->setSrv(unpackAux<Srv9*>());
        p9conn->alloc(1024);
        srv.listen(fd, p9conn, &Conn::handleFcall, &Conn::cleanup);
    }
}

void
Conn9::alloc(uint n) {
    _rmsg.alloc(n);
    _wmsg.alloc(n);
}
} // end namespace jyq
