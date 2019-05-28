/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "Client.h"
#include "CFid.h"
#include "util.h"
#include "PrintFunctions.h"
#include "socket.h"

namespace jyq {
constexpr auto RootFid = 1;

void
Client::clunk(std::shared_ptr<CFid> ptr) {
    ptr->clunk([this](auto& value) { return dofcall(value); });
    putfid(ptr);
}
std::shared_ptr<CFid>
Client::getFid() {
    auto theLock = getLock();
    if (_freefid.empty()) {
        auto ptr = std::make_shared<CFid>();
        ptr->setFid(++_lastfid);
        return ptr;
    } else {
        std::shared_ptr<CFid> front(_freefid.front()); // make a copy?
        _freefid.pop_front();
        return front;
    }
}

void
Client::putfid(std::shared_ptr<CFid> f) {
    auto theLock = getLock();
    if (f->getFid() == _lastfid) {
		_lastfid--;
	} else {
        _freefid.emplace_front(f);
	}
}
void
Msg::alloc(uint n) {
    setSize(n);
    setData(new char[n]);
    _end = _data + n;
    _pos = _data;
}
void
Client::allocmsg(int n) {
    _rmsg.alloc(n);
    _wmsg.alloc(n);
}
namespace {

void
initfid(std::shared_ptr<CFid> f, Fcall *fcall, uint iounit) {
    f->setOpen(1);
    f->setOffset(0);
    f->setIoUnit(iounit);
    f->setQid(fcall->getRopen().getQid());
}
std::shared_ptr<Stat>
_stat(ulong fid, std::function<std::shared_ptr<Fcall>(Fcall&)> dofcall) {
	Fcall fcall(FType::TStat, fid);
    if (auto result = dofcall(fcall); !result) {
        return nullptr;
    } else {

        char* buf = new char[result->getRstat().getStat().size()];
        for (auto i = 0; i < result->getRstat().getStat().size(); ++i) {
            buf[i] = result->getRstat().getStat()[i];
        }
        Msg msg(buf, result->getRstat().size(), Msg::Mode::Unpack);
        auto stat = std::make_shared<Stat>();
        msg.pstat(*stat);
        if(msg.getPos() > msg.getEnd()) {
            return nullptr;
        } else {
            return stat;
        }
    }
}

long
_pread(CFid *f, char *buf, long count, int64_t offset, std::function<std::shared_ptr<Fcall>(Fcall&)> dofcall) {

	auto len = 0l;
	while(len < count) {
	    Fcall fcall;
        auto n = min<int>(count-len, f->getIoUnit());
        fcall.setTypeAndFid(FType::TRead, f->getFid());
        auto& tread = fcall.getTRead();
        tread.setOffset(offset);
        tread.setSize(n);
        if (auto result = dofcall(fcall); !result) {
            return -1;
        } else if (result->getRRead().size() > n) {
            return -1;
        } else {
            auto& rread = result->getRRead();
            memcpy(buf+len, rread.getData().data(), rread.size());
            offset += rread.size();
            len += rread.size();

            if(rread.size() < n) {
                break;
            }
        }
	}
	return len;
}

long
_pwrite(CFid *f, const void *buf, long count, int64_t offset, DoFcallFunc dofcall) {
	int n, len;

	len = 0;
	do {
        Fcall fcall;
		n = min<int>(count-len, f->getIoUnit());
        fcall.setTypeAndFid(FType::TWrite, f->getFid());
        auto& twrite = fcall.getTWrite();
        twrite.setOffset(offset);
        twrite.setData((char*)buf + len);
        twrite.setSize(n);
        if (auto result = dofcall(fcall); !result) {
            return -1;
        } else {
            auto& rwrite = result->getRWrite();
            offset += rwrite.size();
            len += rwrite.size();

            if(rwrite.size() < n) {
                break;
            }
        }
	} while(len < count);
	return len;
}
} // end namespace
std::shared_ptr<Fcall>
Client::dofcall(Fcall& fcall) {
    // this version of dofcall does not modify the result of 
    auto ret = muxrpc(fcall);

    if (!ret) {
        return nullptr;
    }
    if (ret->getType() == FType::RError) {
        wErrorString(ret->getError().getEname());
        return nullptr;
    }
    if (auto hdrVal = uint8_t(ret->getType()), fhdrVal = uint8_t(fcall.getType()); hdrVal != (fhdrVal^1)) {
        wErrorString("received mismatched fcall");
        return nullptr;
	}
    return ret;
}
std::shared_ptr<CFid>
Client::walkdir(char *path, const char **rest) {
    // TODO: replace with std::filesystem::path iterator, much better
	char* p = path + strlen(path) - 1;
    if (p < path) {
        throw Exception("p is not greater than or equal to path!");
    }
	while(*p == '/') {
		*p-- = '\0';
    }

	while((p > path) && (*p != '/')) {
		p--;
    }
	if(*p != '/') {
        throw Exception("bad path");
	}

	*p++ = '\0';
	*rest = p;
    return this->walk(path);
}
std::shared_ptr<CFid>
Client::walk(const std::string& path) {
    Fcall fcall(FType::TWalk);
    if (auto separation = tokenize(path, '/'); separation.size() > maximum::Welem) {
        throw Exception("Path: '", path, "' is split into more than ", int(maximum::Welem), " components!");
    } else {
        int n = separation.size();
        int count = 0;
        for (auto& sep : separation) {
            fcall.getTwalk().getWname()[count] = sep.data();
            ++count;
        }
        auto f = getFid();
        fcall.setFid(RootFid);

        fcall.getTwalk().setSize(n);
        fcall.getTwalk().setNewFid(f->getFid());
        if (dofcall(fcall) == 0) {
            putfid(f);
            return nullptr;
        }
        if(fcall.getRwalk().size() < n) {
            wErrorString("File does not exist");
            if(fcall.getRwalk().empty()) {
                wErrorString("Protocol botch");
            }
            putfid(f);
            return nullptr;
        }

        f->setQid(fcall.getRwalk().getWqid()[n-1]); // gross... so gross, this is taken from teh c code...so gross

        return f;
    }
}

/**
 * Function: remove
 *
 * Params:
 *	path: The path of the file to remove.
 *
 * Removes a file or directory from the remote server.
 *
 * Returns:
 *	remove returns 0 on failure, 1 on success.
 * See also:
 *	F<mount>
 */

bool
Client::remove(const char *path) {
    if (auto f = walk(path); !f) {
        return false;
    } else {
        Fcall fcall;
        fcall.setTypeAndFid(FType::TRemove, f->getFid());
        auto ret = dofcall(fcall);
        putfid(f);

        return (bool)ret;
    }
}


Client::~Client() {
    fd.shutdown(SHUT_RDWR);
    fd.close();
}


/**
 * Function: mount
 * Function: mountfd
 * Function: nsmount
 * Type: Client
 *
 * Params:
 *	fd:      A file descriptor which is already connected
 *	         to a 9P server.
 *	address: An address (in Plan 9 resource fomat) at
 *	         which to connect to a 9P server.
 *	name:    The name of a socket in the process's canonical
 *	         namespace directory.
 *
 * Initiate a 9P connection with the server at P<address>,
 * connected to on P<fd>, or under the process's namespace
 * directory as P<name>.
 *
 * Returns:
 *	A pointer to a new 9P client.
 * See also:
 *	F<open>, F<create>, F<remove>, F<unmount>
 */
Client*
Client::mountfd(const Connection& fd) {
    FVersion ver;
    ver.setType(FType::TVersion);
	Fcall fcall;
    auto c = new Client(fd);
    c->allocmsg(256);
	c->_lastfid = RootFid;
	/* Override tag matching on TVersion */
	c->_mintag = NoTag;
	c->_maxtag = NoTag+1;

    ver.setSize(maximum::Msg);
    ver.setVersion(Version);

    fcall._storage = ver;
	if(!c->dofcall(fcall)) {
        delete c;
		return nullptr;
	}

    if (fcall.getVersion().getVersion() != Version
	|| fcall.getVersion().size() > maximum::Msg) {
		wErrorString("bad 9P version response");
        delete c;
		return nullptr;
	}

	c->_mintag = 0;
	c->_maxtag = 255;
	c->_msize = fcall.getVersion().size();

	c->allocmsg(fcall.getVersion().size());
    //fcall.reset();
    FAttach contents;
    contents.setType(FType::TAttach);
    contents.setFid(RootFid);
    contents.setAfid(NoFid);
	contents.setUname(getenv("USER"));
    contents.setAname("");
    fcall._storage = contents;
	if(!c->dofcall(fcall)) {
        delete c;
		return nullptr;
	}

	return c;
}

Client*
Client::mount(const char *address) {
    if (Connection fd = Connection::dial(address); !fd.isLegal()) {
        return nullptr;
    } else {
        return mountfd(fd);
    }
}

Client*
Client::nsmount(const char *name) {
    std::string address(getNamespace());
	if(!address.empty()) {
        std::stringstream builder;
        builder << "unix!" << address << "/" << name;
        address = builder.str();
    }
    if (address.empty()) {
		return nullptr;
    } else {
        return mount(address);
    }
}


/**
 * Function: open
 * Function: create
 * Type: CFid
 * Type: OMode
 *
 * Params:
 *	path: The path of the file to open or create.
 *	perm: The permissions with which to create the new
 *	      file. These will be ANDed with those of the
 *	      parent directory by the server.
 *	mode: The file's open mode.
 *
 * open and create each open a file at P<path>.
 * P<mode> must include OREAD, OWRITE, or ORDWR, and may
 * include any of the modes specified in T<OMode>.
 * create, additionally, creates a file at P<path> if it
 * doesn't already exist.
 *
 * Returns:
 *	A pointer on which to operate on the newly
 *      opened file.
 *
 * See also:
 *	F<mount>, F<read>, F<write>, F<print>,
 *	F<fstat>, F<close>
 */

std::shared_ptr<CFid>
Client::create(const char *path, uint perm, uint8_t mode) {
	Fcall fcall;

    std::string tpath(path);
    if (auto f = walkdir(tpath.data(), &path); !f) {
        return f;
    } else {
        fcall.setTypeAndFid(FType::TCreate, f->getFid());
        fcall.getTcreate().setName((char*)(uintptr_t)path);
        fcall.getTcreate().setPerm(perm);
        fcall.getTcreate().setMode(mode);

        if(!dofcall(fcall)) {
            clunk(f);
            return nullptr;
        }

        auto count = fcall.getRopen().getIoUnit();
        if (count == 0 || (fcall.getRopen().getIoUnit() > (_msize-24))) {
            count = _msize-24;
        }
        initfid(f, &fcall, count);
        f->setMode(mode);
        return f;
    }
}

std::shared_ptr<CFid>
Client::open(const char *path, uint8_t mode) {

    if (auto f = walk(path); !f) {
        return nullptr;
    } else {
	    Fcall fcall;
        fcall.setTypeAndFid(FType::TOpen, f->getFid());
        fcall.getTopen().setMode(mode);

        if(!dofcall(fcall)) {
            clunk(f);
            return nullptr;
        }

        auto count = fcall.getRopen().getIoUnit();
        if (count == 0 || (fcall.getRopen().getIoUnit() > (_msize-24))) {
            count = _msize-24;
        }
        initfid(f, &fcall, count);
        f->setMode(mode);

        return f;
    }
}

/**
 * Function: close
 *
 * Closes the file pointed to by P<f> and frees its
 * associated data structures;
 *
 * Returns:
 *	Returns 1 on success, and zero on failure.
 * See also:
 *	F<mount>, F<open>
 */

bool
CFid::close(DoFcallFunc fn) {
    return clunk(fn);
}


/**
 * Function: stat
 * Function: fstat
 * Type: Stat
 * Type: Qid
 * Type: QType
 * Type: DMode
 *
 * Params:
 *	path: The path of the file to stat.
 *	fid:  An open file descriptor to stat.
 *
 * Stats the file at P<path> or pointed to by P<fid>.
 *
 * Returns:
 *	Returns an Stat structure, which must be freed by
 *	the caller with free(3).
 * See also:
 *	F<mount>, F<open>
 */

std::shared_ptr<Stat>
Client::stat(const char *path) {
	if (auto f = walk(path); !f) {
        return nullptr;
    } else {
	    auto stat = _stat(f->getFid(), [this](auto& fc) { return dofcall(fc); });
        clunk(f);
	    return stat;
    }
}

std::shared_ptr<Stat>
CFid::fstat(DoFcallFunc c) {
	return _stat(_fid, c);
}


/**
 * Function: read
 * Function: pread
 *
 * Params:
 *	buf:    A buffer in which to store the read data.
 *	count:  The number of bytes to read.
 *	offset: The offset at which to begin reading.
 *
 * read and pread each read P<count> bytes of data
 * from the file pointed to by P<fid>, into P<buf>. read
 * begins reading at its stored offset, and increments it by
 * the number of bytes read. pread reads beginning at
 * P<offset> and does not alter P<fid>'s stored offset.
 *
 * Returns:
 *	These functions return the number of bytes read on
 *	success and -1 on failure.
 * See also:
 *	F<mount>, F<open>, F<write>
 */

long
CFid::read(void *buf, long count, DoFcallFunc dofcall) {
    auto theLock = getIoLock();
	int n = _pread(this, (char*)buf, count, _offset, dofcall);
	if(n > 0) {
		_offset += n;
    }
	return n;
}

long
CFid::pread(void *buf, long count, int64_t offset, DoFcallFunc fn) {
    auto theLock = getIoLock();
	return _pread(this, (char*)buf, count, offset, fn);
}


/**
 * Function: write
 * Function: pwrite
 *
 * Params:
 *	buf:    A buffer holding the contents to store.
 *	count:  The number of bytes to store.
 *	offset: The offset at which to write the data.
 *
 * write and pwrite each write P<count> bytes of
 * data stored in P<buf> to the file pointed to by C<fid>.
 * write writes its data at its stored offset, and
 * increments it by P<count>. pwrite writes its data a
 * P<offset> and does not alter C<fid>'s stored offset.
 *
 * Returns:
 *	These functions return the number of bytes actually
 *	written. Any value less than P<count> must be considered
 *	a failure.
 * See also:
 *	F<mount>, F<open>, F<read>
 */

long
CFid::write(const void *buf, long count, DoFcallFunc fn) {
    auto theLock = getIoLock();
	auto n = _pwrite(this, buf, count, _offset, fn);
	if(n > 0) {
		_offset += n;
    }
	return n;
}

long
CFid::pwrite(const void *buf, long count, int64_t offset, DoFcallFunc fn) {
    auto theLock = getIoLock();
	return _pwrite(this, buf, count, offset, fn);
}

bool
CFid::performClunk(DoFcallFunc c) {
	Fcall fcall(FType::TClunk, _fid);
    return bool(c(fcall));
}

bool 
CFid::clunk(DoFcallFunc fn) {
    return performClunk(fn);
}

Client::Client(int _fd) : fd(_fd), sleep(std::make_shared<BareRpc>()) { }
Client::Client(const Connection& c) : fd(c), sleep(std::make_shared<BareRpc>()) { 
    sleep->circularLink(sleep);
}
} // end namespace jyq

