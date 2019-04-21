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
#include "ixp_local.h"

#define nelem(ary) (sizeof(ary) / sizeof(*ary))
namespace ixp {
constexpr auto RootFid = 1;

namespace {
CFid*
getfid(Client *c) {
	CFid *f;

	concurrency::threadModel->lock(&c->lk);
	f = c->freefid;
    if (f) {
		c->freefid = f->next;
    } else {
		f = (CFid*)emallocz(sizeof *f);
		f->client = c;
		f->fid = ++c->lastfid;
		concurrency::threadModel->initmutex(&f->iolock);
	}
	f->next = nullptr;
	f->open = 0;
	concurrency::threadModel->unlock(&c->lk);
	return f;
}

void
putfid(CFid *f) {

	auto c = f->client;
	concurrency::threadModel->lock(&c->lk);
	if(f->fid == c->lastfid) {
		c->lastfid--;
		concurrency::threadModel->mdestroy(&f->iolock);
		free(f);
	} else {
		f->next = c->freefid;
		c->freefid = f;
	}
	concurrency::threadModel->unlock(&c->lk);
}

bool 
dofcall(Client *c, Fcall *fcall) {

    auto ret = c->muxrpc(fcall);
    if (!ret) {
		return false;
    }
	if(ret->hdr.type == FType::RError) {
		werrstr("%s", ret->error.ename);
		goto fail;
	}
    if (auto hdrVal = uint8_t(ret->hdr.type), fhdrVal = uint8_t(fcall->getType()); hdrVal != (fhdrVal^1)) {
        std::cout << "hdrVal: " << hdrVal << std::endl;
		werrstr("received mismatched fcall");
		goto fail;
	}
	memcpy(fcall, ret, sizeof *fcall);
	free(ret);
	return true;
fail:
	Fcall::free(fcall);
	free(ret);
	return false;
}
void
allocmsg(Client *c, int n) {
	c->rmsg.size = n;
	c->wmsg.size = n;
	c->rmsg.data = (char*)erealloc(c->rmsg.data, n);
	c->wmsg.data = (char*)erealloc(c->wmsg.data, n);
}
CFid*
walk(Client *c, const char *path) {
	CFid *f;
	char *p;
	Fcall fcall(FType::TWalk);
	int n;

	p = estrdup(path);
	n = tokenize(fcall.twalk.wname, nelem(fcall.twalk.wname), p, '/');
	f = getfid(c);
    fcall.setFid(RootFid);

    fcall.twalk.setSize(n);
	fcall.twalk.newfid = f->fid;
	if(!dofcall(c, &fcall))
		goto fail;
	if(fcall.rwalk.size() < n) {
		werrstr("File does not exist");
		if(fcall.rwalk.empty())
			werrstr("Protocol botch");
		goto fail;
	}

	f->qid = fcall.rwalk.wqid[n-1];

	Fcall::free(&fcall);
	free(p);
	return f;
fail:
	putfid(f);
	free(p);
	return nullptr;
}

CFid*
walkdir(Client *c, char *path, const char **rest) {
	char *p;

	p = path + strlen(path) - 1;
	assert(p >= path);
	while(*p == '/')
		*p-- = '\0';

	while((p > path) && (*p != '/'))
		p--;
	if(*p != '/') {
		werrstr("bad path");
		return nullptr;
	}

	*p++ = '\0';
	*rest = p;
	return walk(c, path);
}

void
initfid(CFid *f, Fcall *fcall) {
	f->open = 1;
	f->offset = 0;
	f->iounit = fcall->ropen.iounit;
	if(f->iounit == 0 || fcall->ropen.iounit > f->client->msize-24)
		f->iounit =  f->client->msize-24;
	f->qid = fcall->ropen.qid;
}
Stat*
_stat(Client *c, ulong fid) {
	Msg msg;
	Fcall fcall(FType::TStat, fid);
	Stat *stat;
	if(!dofcall(c, &fcall))
		return nullptr;

	msg = Msg::message((char*)fcall.rstat.stat, fcall.rstat.size(), Msg::Mode::Unpack);

	stat = (Stat*)emalloc(sizeof *stat);
    msg.pstat(stat);
	Fcall::free(&fcall);
	if(msg.pos > msg.end) {
		free(stat);
		stat = nullptr;
	}
	return stat;
}

long
_pread(CFid *f, char *buf, long count, int64_t offset) {
	Fcall fcall;

	auto len = 0l;
	while(len < count) {
        auto n = min<int>(count-len, f->iounit);
        fcall.setTypeAndFid(FType::TRead, f->fid);
		fcall.tread.offset = offset;
        fcall.tread.setSize(n);
		if(!dofcall(f->client, &fcall))
			return -1;
		if(fcall.rread.size()> n)
			return -1;

		memcpy(buf+len, fcall.rread.data, fcall.rread.size());
		offset += fcall.rread.size();
		len += fcall.rread.size();

		Fcall::free(&fcall);
		if(fcall.rread.size() < n)
			break;
	}
	return len;
}

long
_pwrite(CFid *f, const void *buf, long count, int64_t offset) {
	Fcall fcall;
	int n, len;

	len = 0;
	do {
		n = min<int>(count-len, f->iounit);
        fcall.setTypeAndFid(FType::TWrite, f->fid);
		fcall.twrite.offset = offset;
		fcall.twrite.data = (char*)buf + len;
        fcall.twrite.setSize(n);
		if(!dofcall(f->client, &fcall))
			return -1;

		offset += fcall.rwrite.size();
		len += fcall.rwrite.size();

		Fcall::free(&fcall);
		if(fcall.rwrite.size() < n)
			break;
	} while(len < count);
	return len;
}
} // end namespace

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
	Fcall fcall;

    if (auto f = walk(this, path); !f) {
        return false;
    } else {
        fcall.setTypeAndFid(FType::TRemove, f->fid);
        auto ret = dofcall(this, &fcall);
        Fcall::free(&fcall);
        putfid(f);

        return ret;
    }
}


/**
 * Function: unmount
 *
 * Unmounts the client P<client> and frees its data structures.
 *
 * See also:
 *	F<mount>
 */
Client::~Client() {
    unmount(this);
}
void
Client::unmount(Client *client) {
	CFid *f;
    // TODO migrate this to the client destructor eventually

	shutdown(client->fd, SHUT_RDWR);
	::close(client->fd);

    client->muxfree();

	while((f = client->freefid)) {
		client->freefid = f->next;
		concurrency::threadModel->mdestroy(&f->iolock);
		free(f);
	}
	free(client->rmsg.data);
	free(client->wmsg.data);
    free(client);
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
Client::mountfd(int fd) {
	Fcall fcall;

    fcall.setType(FType::TVersion);
	auto c = (Client*)emallocz(sizeof(Client));
	c->fd = fd;
    c->muxinit();

	allocmsg(c, 256);
	c->lastfid = RootFid;
	/* Override tag matching on TVersion */
	c->mintag = NoTag;
	c->maxtag = NoTag+1;

    fcall.version.setSize(maximum::Msg);
	fcall.version.version = (char*)Version;

	if(!dofcall(c, &fcall)) {
		unmount(c);
		return nullptr;
	}

	if(strcmp(fcall.version.version, Version)
	|| fcall.version.size() > maximum::Msg) {
		werrstr("bad 9P version response");
		unmount(c);
		return nullptr;
	}

	c->mintag = 0;
	c->maxtag = 255;
	c->msize = fcall.version.size();

	allocmsg(c, fcall.version.size());
	Fcall::free(&fcall);

    fcall.setTypeAndFid(FType::TAttach, RootFid);
	fcall.tattach.afid = NoFid;
	fcall.tattach.uname = getenv("USER");
	fcall.tattach.aname = (char*)"";
	if(!dofcall(c, &fcall)) {
		unmount(c);
		return nullptr;
	}

	return c;
}

Client*
Client::mount(const char *address) {
	int fd;

	fd = dial(address);
	if(fd < 0)
		return nullptr;
	return mountfd(fd);
}

Client*
Client::nsmount(const char *name) {
	char *address;
	Client *c;

	address = getNamespace();
	if(address)
		address = smprint("unix!%s/%s", address, name);
    if (!address) 
		return nullptr;
	c = mount(address);
	free(address);
	return c;
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

CFid*
Client::create(const char *path, uint perm, uint8_t mode) {
	Fcall fcall;
	CFid *f;
	char *tpath;;

	tpath = estrdup(path);

	f = walkdir(this, tpath, &path);
    if (!f) 
		goto done;

    fcall.setTypeAndFid(FType::TCreate, f->fid);
	fcall.tcreate.name = (char*)(uintptr_t)path;
	fcall.tcreate.perm = perm;
	fcall.tcreate.mode = mode;

	if(!dofcall(this, &fcall)) {
        f->clunk();
		f = nullptr;
		goto done;
	}

	initfid(f, &fcall);
	f->mode = mode;

	Fcall::free(&fcall);

done:
	free(tpath);
	return f;
}

CFid*
Client::open(const char *path, uint8_t mode) {
	Fcall fcall;
	CFid *f;

	f = walk(this, path);
    if (!f) 
		return nullptr;

    fcall.setTypeAndFid(FType::TOpen, f->fid);
	fcall.topen.mode = mode;

	if(!dofcall(this, &fcall)) {
		f->clunk();
		return nullptr;
	}

	initfid(f, &fcall);
	f->mode = mode;

	Fcall::free(&fcall);
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
CFid::close() {
    return clunk();
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

Stat*
Client::stat(const char *path) {
	Stat *stat;
	CFid *f;

	f = walk(this, path);
    if (!f) 
		return nullptr;

	stat = _stat(this, f->fid);
    f->clunk();
	return stat;
}

Stat*
CFid::fstat() {
	return _stat(client, fid);
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
CFid::read(void *buf, long count) {
	int n;

	concurrency::threadModel->lock(&iolock);
	n = _pread(this, (char*)buf, count, offset);
	if(n > 0)
		offset += n;
	concurrency::threadModel->unlock(&iolock);
	return n;
}

long
CFid::pread(void *buf, long count, int64_t offset) {
	int n;

	concurrency::threadModel->lock(&iolock);
	n = _pread(this, (char*)buf, count, offset);
	concurrency::threadModel->unlock(&iolock);
	return n;
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
CFid::write(const void *buf, long count) {
	concurrency::threadModel->lock(&iolock);
	auto n = _pwrite(this, buf, count, offset);
	if(n > 0)
		offset += n;
	concurrency::threadModel->unlock(&iolock);
	return n;
}

long
CFid::pwrite(const void *buf, long count, int64_t offset) {
	int n;

	concurrency::threadModel->lock(&iolock);
	n = _pwrite(this, buf, count, offset);
	concurrency::threadModel->unlock(&iolock);
	return n;
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

int
CFid::vprint(const char *fmt, va_list args) {
	if (auto buf = vsmprint(fmt, args); !buf) {
        return -1;
    } else {
        auto n = write(buf, strlen(buf));
        free(buf);
        return n;
    }
}

int
CFid::print(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	auto n = vprint(fmt, ap);
	va_end(ap);

	return n;
}
bool 
CFid::clunk() {
	Fcall fcall(FType::TClunk, fid);
	auto c = client;
	auto ret = dofcall(c, &fcall);
	if(ret)
		putfid(this);
	Fcall::free(&fcall);
	return ret;
}
} // end namespace ixp

