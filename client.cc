/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <assert.h>
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
    concurrency::Locker<Mutex> theLock(_lk);
    if (_freefid.empty()) {
        auto ptr = std::make_shared<CFid>();
        ptr->fid = ++_lastfid;
        return ptr;
    } else {
        std::shared_ptr<CFid> front(_freefid.front()); // make a copy?
        _freefid.pop_front();
        return front;
    }
}

void
Client::putfid(std::shared_ptr<CFid> f) {
    concurrency::Locker<Mutex> theLock(_lk);
    if (f->fid == _lastfid) {
		_lastfid--;
	} else {
        _freefid.emplace_front(f);
	}
}
void
Msg::alloc(uint n) {
    setSize(n);
    if (data) {
        delete[] data;
    }
    data = new char[n];
    end = data + n;
    pos = data;
}
void
Client::allocmsg(int n) {
    _rmsg.alloc(n);
    _wmsg.alloc(n);
}
namespace {

void
initfid(std::shared_ptr<CFid> f, Fcall *fcall, decltype(CFid::iounit) iounit) {
	f->open = 1;
	f->offset = 0;
    f->iounit = iounit;
    f->qid = fcall->ropen.getQid();
}
std::shared_ptr<Stat>
_stat(ulong fid, std::function<std::shared_ptr<Fcall>(Fcall&)> dofcall) {
	Fcall fcall(FType::TStat, fid);
    if (auto result = dofcall(fcall); !result) {
        return nullptr;
    } else {

        Msg msg((char*)result->rstat.getStat(), result->rstat.size(), Msg::Mode::Unpack);
        auto stat = std::make_shared<Stat>();
        msg.pstat(*stat);
        if(msg.pos > msg.end) {
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
        auto n = min<int>(count-len, f->iounit);
        fcall.setTypeAndFid(FType::TRead, f->fid);
        fcall.tread.setOffset(offset);
        fcall.tread.setSize(n);
        if (auto result = dofcall(fcall); !result) {
            return -1;
        } else if (result->rread.size() > n) {
            return -1;
        } else {
            memcpy(buf+len, result->rread.getData(), result->rread.size());
            offset += result->rread.size();
            len += result->rread.size();

            if(result->rread.size() < n) {
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
		n = min<int>(count-len, f->iounit);
        fcall.setTypeAndFid(FType::TWrite, f->fid);
        fcall.twrite.setOffset(offset);
        fcall.twrite.setData((char*)buf + len);
        fcall.twrite.setSize(n);
        if (auto result = dofcall(fcall); !result) {
            return -1;
        } else {

            offset += result->rwrite.size();
            len += result->rwrite.size();

            if(result->rwrite.size() < n) {
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
        wErrorString(ret->error.getEname());
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
	char *p;

	p = path + strlen(path) - 1;
	assert(p >= path);
	while(*p == '/')
		*p-- = '\0';

	while((p > path) && (*p != '/'))
		p--;
	if(*p != '/') {
        wErrorString("bad path");
		return nullptr;
	}

	*p++ = '\0';
	*rest = p;
    return this->walk(path);
}
std::shared_ptr<CFid>
Client::walk(const char *path) {
    Fcall fcall(FType::TWalk);
    if (auto separation = tokenize(path, '/'); separation.size() > maximum::Welem) {
        throw Exception("Path: '", path, "' is split into more than ", int(maximum::Welem), " components!");
    } else {
        int n = separation.size();
        int count = 0;
        for (auto& sep : separation) {
            fcall.twalk.getWname()[count] = sep.data();
            ++count;
        }
        auto f = getFid();
        fcall.setFid(RootFid);

        fcall.twalk.setSize(n);
        fcall.twalk.setNewFid(f->fid);
        if (dofcall(fcall) == 0) {
            putfid(f);
            return nullptr;
        }
        if(fcall.rwalk.size() < n) {
            wErrorString("File does not exist");
            if(fcall.rwalk.empty()) {
                wErrorString("Protocol botch");
            }
            putfid(f);
            return nullptr;
        }

        f->qid = fcall.rwalk.getWqid()[n-1]; // gross... so gross, this is taken from teh c code...so gross

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
        fcall.setTypeAndFid(FType::TRemove, f->fid);
        auto ret = dofcall(fcall);
        putfid(f);

        return (bool)ret;
    }
}


Client::~Client() {
    fd.shutdown(SHUT_RDWR);
    fd.close();

    free(wait);

    if (_rmsg.data) {
        delete [] _rmsg.data;
    }
    if (_wmsg.data) {
        delete [] _wmsg.data;
    }
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
	Fcall fcall;

    fcall.setType(FType::TVersion);
    auto c = new Client(fd);
    c->allocmsg(256);
	c->_lastfid = RootFid;
	/* Override tag matching on TVersion */
	c->_mintag = NoTag;
	c->_maxtag = NoTag+1;

    fcall.version.setSize(maximum::Msg);
	fcall.version.setVersion((char*)Version);

	if(!c->dofcall(fcall)) {
        delete c;
		return nullptr;
	}

	if(strcmp(fcall.version.getVersion(), Version)
	|| fcall.version.size() > maximum::Msg) {
		wErrorString("bad 9P version response");
        delete c;
		return nullptr;
	}

	c->_mintag = 0;
	c->_maxtag = 255;
	c->_msize = fcall.version.size();

	c->allocmsg(fcall.version.size());
    fcall.reset();

    fcall.setTypeAndFid(FType::TAttach, RootFid);
    fcall.tattach.setAfid(NoFid);
	fcall.tattach.setUname(getenv("USER"));
	fcall.tattach.setAname((char*)"");
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

    auto f = walkdir(tpath.data(), &path);
    if (!f) {
        return f;
    }

    fcall.setTypeAndFid(FType::TCreate, f->fid);
    fcall.tcreate.setName((char*)(uintptr_t)path);
    fcall.tcreate.setPerm(perm);
    fcall.tcreate.setMode(mode);

	if(!dofcall(fcall)) {
        clunk(f);
        return nullptr;
	}

    auto count = fcall.ropen.getIoUnit();
    if (count == 0 || (fcall.ropen.getIoUnit() > (_msize-24))) {
        count = _msize-24;
    }
	initfid(f, &fcall, count);
	f->mode = mode;
	return f;
}

std::shared_ptr<CFid>
Client::open(const char *path, uint8_t mode) {
	Fcall fcall;

	auto f = walk(path);
    if (!f) {
		return nullptr;
    }

    fcall.setTypeAndFid(FType::TOpen, f->fid);
    fcall.topen.setMode(mode);

	if(!dofcall(fcall)) {
		clunk(f);
		return nullptr;
	}

    auto count = fcall.ropen.getIoUnit();
    if (count == 0 || (fcall.ropen.getIoUnit() > (_msize-24))) {
        count = _msize-24;
    }
	initfid(f, &fcall, count);
	f->mode = mode;

	return f;
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
	    auto stat = _stat(f->fid, [this](auto& fc) { return dofcall(fc); });
        clunk(f);
	    return stat;
    }
}

std::shared_ptr<Stat>
CFid::fstat(DoFcallFunc c) {
	return _stat(fid, c);
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
    concurrency::Locker<Mutex> theLock(iolock);
	int n = _pread(this, (char*)buf, count, offset, dofcall);
	if(n > 0) {
		offset += n;
    }
	return n;
}

long
CFid::pread(void *buf, long count, int64_t offset, DoFcallFunc fn) {
    concurrency::Locker<Mutex> theLock(iolock);
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
    concurrency::Locker<Mutex> theLock(iolock);
	auto n = _pwrite(this, buf, count, offset, fn);
	if(n > 0) {
		offset += n;
    }
	return n;
}

long
CFid::pwrite(const void *buf, long count, int64_t offset, DoFcallFunc fn) {
    concurrency::Locker<Mutex> theLock(iolock);
	return _pwrite(this, buf, count, offset, fn);
}

/**
 * Function: print
 * Function: vprint
 * Variable: vsmprint
 *
 * Params:
 *      fid:  An open CFid to which to write the result.
 *	fmt:  The string with which to format the data.
 *	args: A va_list holding the arguments to the format
 *	      string.
 *	...:  The arguments to the format string.
 *
 * These functions act like the standard formatted IO
 * functions. They write the result of the formatting to the
 * file pointed to by C<fid>.
 *
 * V<vsmprint> may be set to a function which will
 * format its arguments and return a nul-terminated string
 * allocated by malloc(3). The default formats its arguments as
 * printf(3).
 *
 * Returns:
 *	These functions return the number of bytes written.
 *	There is currently no way to detect failure.
 * See also:
 *	F<mount>, F<open>, printf(3)
 */

//int
//CFid::vprint(const char *fmt, va_list args) {
//	if (auto buf = vsmprint(fmt, args); !buf) {
//        return -1;
//    } else {
//        auto n = write(buf, strlen(buf));
//        free(buf);
//        return n;
//    }
//}

bool
CFid::performClunk(DoFcallFunc c) {
	Fcall fcall(FType::TClunk, fid);
    return bool(c(fcall));
}

bool 
CFid::clunk(DoFcallFunc fn) {
    return performClunk(fn);
}

Client::Client(int _fd) : fd(_fd), sleep(*this) { }
Client::Client(const Connection& c) : fd(c), sleep(*this) { 
    sleep.setNext(&sleep);
    sleep.setPrevious(&sleep);
	_tagrend.mutex = &_lk;
}
} // end namespace jyq

