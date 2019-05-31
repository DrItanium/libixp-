/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <ctype.h>
#include <cstdarg>
#include <cstdbool>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include "Msg.h"
#include "jyq.h"
#include "jyq_srvutil.h"

namespace jyq {
static std::string Enofile("file not found");

constexpr auto computeQIDValue(int64_t t, int64_t i) noexcept {
    return int64_t((t & 0xFF)<<32) | int64_t(i & 0xFFFF'FFFF);
}

/**
 * Function: srv_getfile
 * Type: FileId
 *
 * Obtain an empty, reference counted FileId struct.
 *
 * See also:
 *	F<srv_clonefiles>, F<srv_freefile>
 */
FileId
srv_getfile() {
    FileId file = std::make_shared<RawFileId>();
    file->getContents().p = nullptr;
    file->getContents()._volatile = false;
    file->getContents().nref = 1;
    file->setNext(nullptr);
    file->getContents().pending = false;
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
srv_freefile(FileId *) {
    // this function is irrelevant now
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
FileId
srv_clonefiles(FileId& fileid) {

    FileId r = std::make_shared<RawFileId>(fileid->getContents());
    r->getContents().nref = 1;
    for (auto curr = r->getNext(); curr; curr = curr->getNext()) {
        curr->getContents().nref++;
        if (curr == 0) {
            throw Exception("Reference count overflow!");
        }
    }
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
#if 0
	if(req->getIFcall().getIO().getOffset() >= len)
		return;

	len -= req->getIFcall().getIO().getOffset();
	if(len > req->getIFcall().getIO().size())
		len = req->getIFcall().getIO().size();
    req->getOFcall().getIO().setData(new char[len]);
	memcpy(req->getOFcall().getIO().getData(), buf + req->getIFcall().getIO().getOffset(), len);
	req->getOFcall().getIO().setSize(len);
#endif
}

void
srv_writebuf(Req9 *req, char **buf, uint *len, uint max) {
#if 0
	char *p;
	uint offset, count;

    auto file = req->fid->unpackAux<FileId>();

	offset = req->getIFcall().getIO().getOffset();
	if(file->getContents().tab.perm & uint32_t(DMode::APPEND))
		offset = *len;

	if(offset > *len || req->getIFcall().getIO().empty()) {
        req->getOFcall().getIO().setSize(0);
		return;
	}

	count = req->getIFcall().getIO().size();
	if(max && (offset + count > max))
		count = max - offset;

	*len = offset + count;
	if(max == 0) {
		*buf = (char*)jyq::erealloc(*buf, *len + 1);
    }
	p = *buf;

	memcpy(p+offset, req->getIFcall().getIO().getData(), count);
	req->getOFcall().getIO().setSize(count);
	p[offset+count] = '\0';
#endif
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
    // I believe this function is irrelevant now since the data in fio is already 
    // a std::string
#if 0
    uint i = req->getIFcall().getIO().size();
	auto p = req->getIFcall().getIO().getData();
	if(i && p[i - 1] == '\n') {
		i--;
    }
	char* q = (char*)memchr(p, '\0', i);
	if(q) {
		i = q - p;
    }

	p = (decltype(p))jyq::erealloc(req->getIFcall().getIO().getData(), i+1);
	p[i] = '\0';
	req->getIFcall().getIO().setData(p);
#endif
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
srv_writectl(Req9 *req, std::function<char*(void*, Msg*)> fn) {
    // TODO reimplement this
    //auto file = req->fid->unpackAux<FileId>();

	//srv_data2cstring(req);
    //auto s = req->getIFcall().getIO().getData();
	char* err = nullptr;
#if 0
	auto c = *s;
	while(c != '\0') {
		while(*s == '\n')
			s++;
		auto p = s;
		while(*p != '\0' && *p != '\n')
			p++;
		c = *p;
		*p = '\0';

        Msg msg(s, p-s, Msg::Mode::Pack);
		s = fn(file->getContents().p, &msg);
		if(s)
			err = s;
		s = p + 1;
	}
#endif
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
    auto file = req->getFid()->unpackAux<FileId>();
    if (!file->getContents().pending) {
        throw Exception("Given file's contents not marked as pending!");
    }

	auto p = (PendingLink*)(file->getContents().p);

    if (auto& queue = (*p)->getContents().queue; !queue.empty()) {
        std::string front(queue.front());
        queue.pop_front();
        auto buf = new char[front.size()+1];
        buf[front.size()] = '\0';
        front.copy(buf, front.size());
        req->getOFcall().getIO().setData(buf);
		req->getOFcall().getIO().setSize(front.length() + 1);
		if(req->getAux().has_value()) {
            auto req_link = req->unpackAux<RequestLink>();
            req_link->unlink();
		}
		req->respond(nullptr);
	} else {
        RequestLink reqLink = std::make_shared<RawRequestLink>();
        reqLink->setNext((*p)->getContents().pending->req);
        reqLink->setPrevious(reqLink->getNext()->getPrevious());
        reqLink->getNext()->setPrevious(reqLink);
        reqLink->getPrevious()->setNext(reqLink);
		req->getAux() = reqLink;
	}
}

void
Pending::write(const std::string& dat) {
    if (dat.empty()) {
        return;
    }
    write(dat.c_str(), dat.length());
}
void
Pending::write(const char *dat, long ndat) {
	RequestLink req_link;

	if(ndat == 0) {
		return;
    }

    if (!this->req->getNext()) {
        this->req->setNext(this->req);
        this->req->setPrevious(this->req);
        this->fids->setPrevious(this->fids);
        this->fids->setNext(this->fids);
	}
    for (PendingLink pp = this->fids->getNext(); pp != this->fids; pp = pp->getNext()) {
        std::string entry(dat, ndat);
        pp->getContents().queue.emplace_back(entry);
	}

    RequestLink reqLink = std::make_shared<RawRequestLink>();
    reqLink->setNext(reqLink);
    reqLink->setPrevious(reqLink);
    if (this->req->getNext() != this->req) {
        reqLink->setNext(this->req->getNext());
        reqLink->setPrevious(this->req->getPrevious());
        this->req->setPrevious(this->req);
        this->req->setNext(this->req);
	}
    reqLink->getPrevious()->setNext(reqLink);
    reqLink->getNext()->setPrevious(reqLink);

    for (auto rp = reqLink->getNext(); rp != reqLink; ) {
        pending_respond(rp->getContents().req);
    }
}


void
Pending::pushfid(Fid *fid) {

    if (!this->req->getNext()) {

        RawRequestLink::circularLink(this->req);
        RawPendingLink::circularLink(this->fids);
	}

    auto file = fid->unpackAux<FileId>();
    PendingLink pendLink = std::make_shared<RawPendingLink>();
    pendLink->getContents().fid = fid;
    pendLink->getContents().pending = this;
    pendLink->setNext(this->fids);
    pendLink->setPrevious(pendLink->getNext()->getPrevious());
    pendLink->getNext()->setPrevious(pendLink);
    pendLink->getPrevious()->setNext(pendLink);
    file->getContents().pending = true;
    file->getContents().p = &pendLink;
}

static void
_pending_flush(Req9 *req) {
    auto file = req->getFid()->unpackAux<FileId>();
	if(file->getContents().pending) {
        if (auto req_link = req->unpackAux<RequestLink>(); req_link) {
            req_link->unlink();
        }
	}
}

void
pending_flush(Req9 *req) {

	_pending_flush(req->oldreq);
}

bool
pending_clunk(Req9 *req) {

    auto file = req->getFid()->unpackAux<FileId>();
    auto pendLink = std::any_cast<PendingLink>(file->getContents().p);

    auto pending = pendLink->getContents().pending;
    Req9* r = nullptr;
	for(auto reqLink =pending->req->getNext(); reqLink != pending->req;) {
        r = reqLink->getContents().req;
        reqLink = reqLink->getNext();
        if (r->getFid() == pendLink->getContents().fid) {
			_pending_flush(r);
			r->respond("interrupted");
		}
	}

    pendLink->unlink();

    pendLink->getContents().queue.clear();
    auto more = (pendLink->getContents().pending->fids->getNext() == 
                 pendLink->getContents().pending->fids);
    r->respond(nullptr);
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
srv_verifyfile(FileId& file, LookupFn lookup) {
    if (!file->hasNext()) {
        return true;
    }

	auto ret = false;
	if(auto next = file->getNext(); srv_verifyfile(next, lookup)) {
		if (auto tfile = lookup(file->getNext(), file->getContents().tab.name); tfile) {
            if (!tfile->getContents()._volatile || tfile->getContents().p == file->getContents().p) {
                ret = true;
            }
			srv_freefile(tfile);
		}
	}
	return ret;
}

void
srv_readdir(Req9 *req, LookupFn lookup, std::function<void(Stat*, FileId&)> dostat) {
	Stat stat;

    auto file = req->getFid()->unpackAux<FileId>();

	ulong size = req->getIFcall().getIO().size();
	if(size > req->getFid()->getIoUnit()) {
		size = req->getFid()->getIoUnit();
    }
    auto buf = new char[size];
    Msg msg(buf, size, Msg::Mode::Pack);

	file = lookup(file, "");
	auto tfile = file;
	/* Note: The first file is ".", so we skip it. */
	uint64_t offset = 0;
	for(file=file->getNext(); file; file=file->getNext()) {
		dostat(&stat, file);
        ulong n = stat.size();
		if(offset >= req->getIFcall().getIO().getOffset()) {
			if(size < n)
				break;
            msg.pstat(&stat);
			size -= n;
		}
		offset += n;
	}
	while((file = tfile)) {
		tfile=tfile->getNext();
		srv_freefile(file);
	}
	req->getOFcall().getIO().setSize(msg.getPos() - msg.getData());
    req->getOFcall().getIO().setData(msg.getData());
    req->respond(nullptr);
}

void
srv_walkandclone(Req9 *req, LookupFn lookup) {
	FileId tfile;
	int i;

    auto fid = req->getFid()->unpackAux<FileId>();
	auto file = srv_clonefiles(fid);
	for(i=0; i < req->getIFcall().getTwalk().size(); i++) {
        if (req->getIFcall().getTwalk().getWname()[i] == "..") {
			if(file->hasNext()) {
				tfile = file;
				file = file->getNext();
				srv_freefile(tfile);
			}
		}else{
			tfile = lookup(file, req->getIFcall().getTwalk().getWname()[i]);
			if(!tfile)
				break;
            if (tfile->hasNext()) {
			    //assert(!tfile->hasNext());
                throw Exception("tfile has next!");
            }
            if (req->getIFcall().getTwalk().getWname()[i] != ".") {
				tfile->setNext(file);
				file = tfile;
			}
		}
		req->getOFcall().getRwalk().getWqid()[i].setType(file->getContents().tab.qtype);
		req->getOFcall().getRwalk().getWqid()[i].setPath(computeQIDValue(file->getContents().tab.type, file->getContents().id));
	}
	/* There should be a way to do this on freefid() */
	if(i < req->getIFcall().getTwalk().size()) {
		while((tfile = file)) {
			file=file->getNext();
			srv_freefile(tfile);
		}
        req->respond(Enofile);
		return;
	}
	/* Remove refs for req->fid if no new fid */
	if(req->getIFcall().getFid() == req->getIFcall().getTwalk().getNewFid()) {
        tfile = req->getFid()->unpackAux<decltype(tfile)>();
		req->getFid()->setAux(file);
		while((file = tfile)) {
			tfile = tfile->getNext();
			srv_freefile(file);
		}
	} else {
        req->getNewFid()->setAux(file);
    }
    req->getOFcall().getRwalk().setSize(i);
    req->respond(nullptr);
}

} // end namespace jyq
