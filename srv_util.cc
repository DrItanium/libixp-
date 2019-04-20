/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "ixp_local.h"

namespace ixp {
using FileIdU = void*;

static char
	Enofile[] = "file not found";
} // end namespace ixp

#include "ixp_srvutil.h"

namespace ixp {
struct Queue {
	Queue*	link;
	char*		dat;
	long		len;
};

constexpr auto QID(int64_t t, int64_t i) noexcept {
    return int64_t((t & 0xFF)<<32) | int64_t(i & 0xFFFF'FFFF);
}

static FileId*	free_fileid;

/**
 * Function: srv_getfile
 * Type: FileId
 *
 * Obtain an empty, reference counted FileId struct.
 *
 * See also:
 *	F<srv_clonefiles>, F<srv_freefile>
 */
FileId*
srv_getfile(void) {
	FileId *file;
	int i;

	if(!free_fileid) {
		i = 15;
		file = (decltype(file))ixp::emallocz(i * sizeof *file);
		for(; i; i--) {
			file->next = free_fileid;
			free_fileid = file++;
		}
	}
	file = free_fileid;
	free_fileid = file->next;
	file->p = nullptr;
	file->volatil = 0;
	file->nref = 1;
	file->next = nullptr;
	file->pending = false;
	return file;
}

/**
 * Function: srv_freefile
 *
 * Decrease the reference count of the given FileId,
 * and push it onto the free list when it reaches 0;
 *
 * See also:
 *	F<srv_getfile>
 */
void
srv_freefile(FileId *fileid) {
	if(--fileid->nref)
		return;
	free(fileid->tab.name);
	fileid->next = free_fileid;
	free_fileid = fileid;
}

/**
 * Function: srv_clonefiles
 *
 * Increase the reference count of every FileId linked
 * to P<fileid>.
 *
 * See also:
 *	F<srv_getfile>
 */
FileId*
srv_clonefiles(FileId *fileid) {
	FileId *r;

	r = srv_getfile();
	memcpy(r, fileid, sizeof *r);
	r->tab.name = ixp::estrdup(r->tab.name);
	r->nref = 1;
	for(fileid=fileid->next; fileid; fileid=fileid->next)
		assert(fileid->nref++);
	return r;
}

/**
 * Function: srv_readbuf
 * Function: srv_writebuf
 *
 * Utility functions for handling TRead and TWrite requests for
 * files backed by in-memory buffers. For both functions, P<buf>
 * points to a buffer and P<len> specifies the length of the
 * buffer. In the case of srv_writebuf, these values add a
 * level of pointer indirection, and updates the values if they
 * change.
 *
 * If P<max> has a value other than 0, srv_writebuf will
 * truncate any writes to that point in the buffer. Otherwise,
 * P<*buf> is assumed to be malloc(3) allocated, and is
 * reallocated to fit the new data as necessary. The buffer is
 * is always left nul-terminated.
 *
 * Bugs:
 *	srv_writebuf always truncates its buffer to the end
 *	of the most recent write.
 */

void
srv_readbuf(Req9 *req, char *buf, uint len) {

	if(req->ifcall.io.offset >= len)
		return;

	len -= req->ifcall.io.offset;
	if(len > req->ifcall.io.count)
		len = req->ifcall.io.count;
	req->ofcall.io.data = (decltype(req->ofcall.io.data))ixp::emalloc(len);
	memcpy(req->ofcall.io.data, buf + req->ifcall.io.offset, len);
	req->ofcall.io.count = len;
}

void
srv_writebuf(Req9 *req, char **buf, uint *len, uint max) {
	FileId *file;
	char *p;
	uint offset, count;

	file = std::any_cast<decltype(file)>(req->fid->aux);

	offset = req->ifcall.io.offset;
	if(file->tab.perm & DMAPPEND)
		offset = *len;

	if(offset > *len || req->ifcall.io.count == 0) {
		req->ofcall.io.count = 0;
		return;
	}

	count = req->ifcall.io.count;
	if(max && (offset + count > max))
		count = max - offset;

	*len = offset + count;
	if(max == 0)
		*buf = (char*)ixp::erealloc(*buf, *len + 1);
	p = *buf;

	memcpy(p+offset, req->ifcall.io.data, count);
	req->ofcall.io.count = count;
	p[offset+count] = '\0';
}

/**
 * Function: srv_data2cstring
 *
 * Ensure that the data member of P<req> is null terminated,
 * removing any new line from its end.
 *
 * See also:
 *	S<Req9>
 */
void
srv_data2cstring(Req9 *req) {
	char *p, *q;
	uint i;

	i = req->ifcall.io.count;
	p = req->ifcall.io.data;
	if(i && p[i - 1] == '\n')
		i--;
	q = (decltype(q))memchr(p, '\0', i);
	if(q)
		i = q - p;

	p = (decltype(p))ixp::erealloc(req->ifcall.io.data, i+1);
	p[i] = '\0';
	req->ifcall.io.data = p;
}

/**
 * Function: srv_writectl
 *
 * This utility function is meant to simplify the writing of
 * pseudo files to which single-lined commands are written.
 * In order to use this function, the P<aux> member of
 * P<req>->fid must be nul or an S<FileId>.  Each line of the
 * written data is stripped of its trailing newline,
 * nul-terminated, and stored in an S<Msg>. For each line
 * thus prepared, P<fn> is called with the Msg pointer and
 * the the P<p> member of the FileId.
 */
char*
srv_writectl(Req9 *req, char* (*fn)(void*, Msg*)) {
	char *err, *s, *p, c;
	FileId *file;
	Msg msg;

	file = std::any_cast<decltype(file)>(req->fid->aux);

	srv_data2cstring(req);
	s = req->ifcall.io.data;

	err = nullptr;
	c = *s;
	while(c != '\0') {
		while(*s == '\n')
			s++;
		p = s;
		while(*p != '\0' && *p != '\n')
			p++;
		c = *p;
		*p = '\0';

		msg = message(s, p-s, 0);
		s = fn(file->p, &msg);
		if(s)
			err = s;
		s = p + 1;
	}
	return err;
}

/**
 * Function: pending_write
 * Function: pending_print
 * Function: pending_vprint
 * Function: pending_pushfid
 * Function: pending_clunk
 * Function: pending_flush
 * Function: pending_respond
 * Type: Pending
 *
 * These functions aid in writing virtual files used for
 * broadcasting events or writing data when it becomes
 * available. When a file to be used with these functions is
 * opened, pending_pushfid should be called with its
 * S<Fid> as an argument. This sets the Fid's P<pending>
 * member to true.  Thereafter, for each file with its
 * P<pending> member set, pending_respond should be called
 * for each TRead request, pending_clunk for each TClunk
 * request, and pending_flush for each TFlush request.
 *
 * pending_write queues the data in P<dat> of length P<ndat>
 * to be written to each currently pending fid in P<pending>. If
 * there is a read request pending for a given fid, the data is
 * written immediately. Otherwise, it is written the next time
 * pending_respond is called. Likewise, if there is data
 * queued when pending_respond is called, it is written
 * immediately, otherwise the request is queued.
 *
 * pending_print and pending_vprint call pending_write
 * after formatting their arguments with V<vsmprint>.
 *
 * The Pending data structure is opaque and should be
 * initialized zeroed before using these functions for the first
 * time.
 *
 * Returns:
 *	pending_clunk returns true if P<pending> has any
 *	more pending Fids.
 */

void
pending_respond(Req9 *req) {
	PendingLink *p;
	RequestLink *req_link;
	Queue *queue;

	auto file = std::any_cast<FileId*>(req->fid->aux);
	assert(file->pending);
	p = (decltype(p))file->p;
	if(p->queue) {
		queue = p->queue;
		p->queue = queue->link;
		req->ofcall.io.data = queue->dat;
		req->ofcall.io.count = queue->len;
		if(req->aux.has_value()) {
            req_link = std::any_cast<decltype(req_link)>(req->aux);
			req_link->next->prev = req_link->prev;
			req_link->prev->next = req_link->next;
			free(req_link);
		}
		respond(req, nullptr);
		free(queue);
	}else {
		req_link = (decltype(req_link))ixp::emallocz(sizeof *req_link);
		req_link->req = req;
		req_link->next = &p->pending->req;
		req_link->prev = req_link->next->prev;
		req_link->next->prev = req_link;
		req_link->prev->next = req_link;
		req->aux = req_link;
	}
}

void
pending_write(Pending *pending, const char *dat, long ndat) {
	RequestLink req_link;
	Queue **qp, *queue;
	PendingLink *pp;
	RequestLink *rp;

	if(ndat == 0)
		return;

    if (!pending->req.next) {
		pending->req.next = &pending->req;
		pending->req.prev = &pending->req;
		pending->fids.prev = &pending->fids;
		pending->fids.next = &pending->fids;
	}

	for(pp=pending->fids.next; pp != &pending->fids; pp=pp->next) {
		for(qp=&pp->queue; *qp; qp=&qp[0]->link)
			;
		queue = (decltype(queue))ixp::emallocz(sizeof *queue);
		queue->dat = (decltype(queue->dat))ixp::emalloc(ndat);
		memcpy(queue->dat, dat, ndat);
		queue->len = ndat;
		*qp = queue;
	}

	req_link.next = &req_link;
	req_link.prev = &req_link;
	if(pending->req.next != &pending->req) {
		req_link.next = pending->req.next;
		req_link.prev = pending->req.prev;
		pending->req.prev = &pending->req;
		pending->req.next = &pending->req;
	}
	req_link.prev->next = &req_link;
	req_link.next->prev = &req_link;

	while((rp = req_link.next) != &req_link)
		pending_respond(rp->req);
}

int
pending_vprint(Pending *pending, const char *fmt, va_list ap) {
	char *dat;
	int res;

	dat = vsmprint(fmt, ap);
	res = strlen(dat);
	pending_write(pending, dat, res);
	free(dat);
	return res;
}

int
pending_print(Pending *pending, const char *fmt, ...) {
	va_list ap;
	int res;

	va_start(ap, fmt);
	res = pending_vprint(pending, fmt, ap);
	va_end(ap);
	return res;
}

void
pending_pushfid(Pending *pending, Fid *fid) {
	PendingLink *pend_link;

    if (!pending->req.next) {
		pending->req.next = &pending->req;
		pending->req.prev = &pending->req;
		pending->fids.prev = &pending->fids;
		pending->fids.next = &pending->fids;
	}

	auto file = std::any_cast<FileId*>(fid->aux);
	pend_link = (decltype(pend_link))ixp::emallocz(sizeof *pend_link);
	pend_link->fid = fid;
	pend_link->pending = pending;
	pend_link->next = &pending->fids;
	pend_link->prev = pend_link->next->prev;
	pend_link->next->prev = pend_link;
	pend_link->prev->next = pend_link;
	file->pending = true;
	file->p = pend_link;
}

static void
_pending_flush(Req9 *req) {
	RequestLink *req_link;

	auto file = std::any_cast<FileId*>(req->fid->aux);
	if(file->pending) {
		req_link = std::any_cast<decltype(req_link)>(req->aux);
		if(req_link) {
			req_link->prev->next = req_link->next;
			req_link->next->prev = req_link->prev;
			free(req_link);
		}
	}
}

void
pending_flush(Req9 *req) {

	_pending_flush(req->oldreq);
}

bool
pending_clunk(Req9 *req) {
	Pending *pending;
	PendingLink *pend_link;
	RequestLink *req_link;
	Req9 *r;
	Queue *queue;
	bool more;

	auto file = std::any_cast<FileId*>(req->fid->aux);
	pend_link = (decltype(pend_link))file->p;

	pending = pend_link->pending;
	for(req_link=pending->req.next; req_link != &pending->req;) {
		r = req_link->req;
		req_link = req_link->next;
		if(r->fid == pend_link->fid) {
			_pending_flush(r);
			respond(r, "interrupted");
		}
	}

	pend_link->prev->next = pend_link->next;
	pend_link->next->prev = pend_link->prev;

	while((queue = pend_link->queue)) {
		pend_link->queue = queue->link;
		free(queue->dat);
		free(queue);
	}
	more = (pend_link->pending->fids.next == &pend_link->pending->fids);
	free(pend_link);
	respond(req, nullptr);
	return more;
}

/**
 * Function: srv_walkandclone
 * Function: srv_readdir
 * Function: srv_verifyfile
 * Type: LookupFn
 *
 * These convenience functions simplify the writing of basic and
 * static file servers. They use a generic file lookup function
 * to simplify the process of walking, cloning, and returning
 * directory listings. Given the S<FileId> of a directory and a
 * filename name should return a new FileId (allocated via
 * F<srv_getfile>) for the matching directory entry, or null
 * if there is no match. If the passed name is null, P<lookup>
 * should return a linked list of FileIds, one for each child
 * directory entry.
 *
 * srv_walkandclone handles the moderately complex process
 * of walking from a directory entry and cloning fids, and calls
 * F<respond>. It should be called in response to a TWalk
 * request.
 *
 * srv_readdir should be called to handle read requests on
 * directories. It prepares a stat for each child of the
 * directory, taking into account the requested offset, and
 * calls F<respond>. The P<dostat> parameter must be a
 * function which fills the passed S<Stat> pointer based on
 * the contents of the passed FileId.
 *
 * srv_verifyfile returns whether a file still exists in the
 * filesystem, and should be used by filesystems that invalidate
 * files once they have been deleted.
 *
 * See also:
 *	S<FileId>, S<getfile>, S<freefile>
 */
bool
srv_verifyfile(FileId *file, LookupFn lookup) {
	FileId *tfile;
	int ret;

	if(!file->next)
		return true;

	ret = false;
	if(srv_verifyfile(file->next, lookup)) {
		tfile = lookup(file->next, file->tab.name);
		if(tfile) {
			if(!tfile->volatil || tfile->p == file->p)
				ret = true;
			srv_freefile(tfile);
		}
	}
	return ret;
}

void
srv_readdir(Req9 *req, LookupFn lookup, void (*dostat)(Stat*, FileId*)) {
	Msg msg;
	FileId *tfile;
	Stat stat;
	char *buf;
	ulong size, n;
	uint64_t offset;

	auto file = std::any_cast<FileId*>(req->fid->aux);

	size = req->ifcall.io.count;
	if(size > req->fid->iounit)
		size = req->fid->iounit;
	buf = (decltype(buf))ixp::emallocz(size);
	msg = message(buf, size, MsgPack);

	file = lookup(file, nullptr);
	tfile = file;
	/* Note: The first file is ".", so we skip it. */
	offset = 0;
	for(file=file->next; file; file=file->next) {
		dostat(&stat, file);
		n = sizeof_stat(&stat);
		if(offset >= req->ifcall.io.offset) {
			if(size < n)
				break;
			pstat(&msg, &stat);
			size -= n;
		}
		offset += n;
	}
	while((file = tfile)) {
		tfile=tfile->next;
		srv_freefile(file);
	}
	req->ofcall.io.count = msg.pos - msg.data;
	req->ofcall.io.data = msg.data;
	respond(req, nullptr);
}

void
srv_walkandclone(Req9 *req, LookupFn lookup) {
	FileId *tfile;
	int i;

	auto file = (FileId*)srv_clonefiles(std::any_cast<FileId*>(req->fid->aux));
	for(i=0; i < req->ifcall.twalk.nwname; i++) {
		if(!strcmp(req->ifcall.twalk.wname[i], "..")) {
			if(file->next) {
				tfile = file;
				file = file->next;
				srv_freefile(tfile);
			}
		}else{
			tfile = lookup(file, req->ifcall.twalk.wname[i]);
			if(!tfile)
				break;
			assert(!tfile->next);
			if(strcmp(req->ifcall.twalk.wname[i], ".")) {
				tfile->next = file;
				file = tfile;
			}
		}
		req->ofcall.rwalk.wqid[i].type = file->tab.qtype;
		req->ofcall.rwalk.wqid[i].path = QID(file->tab.type, file->id);
	}
	/* There should be a way to do this on freefid() */
	if(i < req->ifcall.twalk.nwname) {
		while((tfile = file)) {
			file=file->next;
			srv_freefile(tfile);
		}
		respond(req, Enofile);
		return;
	}
	/* Remove refs for req->fid if no new fid */
	if(req->ifcall.hdr.fid == req->ifcall.twalk.newfid) {
		tfile = std::any_cast<decltype(tfile)>(req->fid->aux);
		req->fid->aux = file;
		while((file = tfile)) {
			tfile = tfile->next;
			srv_freefile(file);
		}
	}else
		req->newfid->aux = file;
	req->ofcall.rwalk.nwqid = i;
	respond(req, nullptr);
}

} // end namespace ixp
