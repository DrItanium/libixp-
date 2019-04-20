/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ixp_local.h"

namespace ixp {
namespace {
constexpr auto SByte = 1;
constexpr auto SWord = 2;
constexpr auto SDWord = 4;
constexpr auto SQWord = 8;

#define SString(s) (SWord + strlen(s))
constexpr auto SQid = SByte + SDWord + SQWord;
} // end namespace 

/**
 * Type: Msg
 * Type: MsgMode
 * Function: message
 *
 * The Msg struct represents a binary message, and is used
 * extensively by libixp for converting messages to and from
 * wire format. The location and size of a buffer are stored in
 * P<data> and P<size>, respectively. P<pos> points to the
 * location in the message currently being packed or unpacked,
 * while P<end> points to the end of the message. The packing
 * functions advance P<pos> as they go, always ensuring that
 * they don't read or write past P<end>.  When a message is
 * entirely packed or unpacked, P<pos> whould be less than or
 * equal to P<end>. Any other state indicates error.
 *
 * message is a convenience function to pack a construct an
 * Msg from a buffer of a given P<length> and a given
 * P<mode>. P<pos> and P<data> are set to P<data> and P<end> is
 * set to P<data> + P<length>.
 *
 * See also:
 *	F<pu8>, F<pu16>, F<pu32>, F<pu64>,
 *	F<pstring>, F<pstrings>
 */
Msg
Msg::message(char *data, uint length, uint mode) {
	Msg m;

	m.data = data;
	m.pos = data;
	m.end = data + length;
	m.size = length;
	m.mode = mode;
	return m;
}

/**
 * Function: freestat
 * Function: freefcall
 *
 * These functions free malloc(3) allocated data in the members
 * of the passed structures and set those members to nullptr. They
 * do not free the structures themselves.
 *
 * See also:
 *	S<Fcall>, S<Stat>
 */
void
Stat::freestat(Stat *s) {
	::free(s->name);
	::free(s->uid);
	::free(s->gid);
	::free(s->muid);
	s->name = s->uid = s->gid = s->muid = nullptr;
}

void
freefcall(Fcall *fcall) {
	switch(fcall->hdr.type) {
	case RStat:
		free(fcall->rstat.stat);
		fcall->rstat.stat = nullptr;
		break;
	case RRead:
		free(fcall->rread.data);
		fcall->rread.data = nullptr;
		break;
	case RVersion:
		free(fcall->version.version);
		fcall->version.version = nullptr;
		break;
	case RError:
		free(fcall->error.ename);
		fcall->error.ename = nullptr;
		break;
	}
}

uint16_t
Stat::size() noexcept {
	return SWord /* size */
		+ SWord /* type */
		+ SDWord /* dev */
		+ SQid /* qid */
		+ (3 * SDWord) /* mode, atime, mtime */
		+ SQWord /* length */
		+ SString(name)
		+ SString(uid)
		+ SString(gid)
		+ SString(muid);
}

void
Msg::pfcall(Fcall *fcall) {
	pu8(&fcall->hdr.type);
	pu16(&fcall->hdr.tag);

	switch (fcall->hdr.type) {
	case TVersion:
	case RVersion:
		pu32(&fcall->version.msize);
		pstring(&fcall->version.version);
		break;
	case TAuth:
		pu32(&fcall->tauth.afid);
		pstring(&fcall->tauth.uname);
		pstring(&fcall->tauth.aname);
		break;
	case RAuth:
		pqid(&fcall->rauth.aqid);
		break;
	case RAttach:
		pqid(&fcall->rattach.qid);
		break;
	case TAttach:
		pu32(&fcall->hdr.fid);
		pu32(&fcall->tattach.afid);
		pstring(&fcall->tattach.uname);
		pstring(&fcall->tattach.aname);
		break;
	case RError:
		pstring(&fcall->error.ename);
		break;
	case TFlush:
		pu16(&fcall->tflush.oldtag);
		break;
	case TWalk:
		pu32(&fcall->hdr.fid);
		pu32(&fcall->twalk.newfid);
		pstrings(&fcall->twalk.nwname, fcall->twalk.wname, nelem(fcall->twalk.wname));
		break;
	case RWalk:
		pqids(&fcall->rwalk.nwqid, fcall->rwalk.wqid, nelem(fcall->rwalk.wqid));
		break;
	case TOpen:
		pu32(&fcall->hdr.fid);
		pu8(&fcall->topen.mode);
		break;
	case ROpen:
	case RCreate:
		pqid(&fcall->ropen.qid);
		pu32(&fcall->ropen.iounit);
		break;
	case TCreate:
		pu32(&fcall->hdr.fid);
		pstring(&fcall->tcreate.name);
		pu32(&fcall->tcreate.perm);
		pu8(&fcall->tcreate.mode);
		break;
	case TRead:
		pu32(&fcall->hdr.fid);
		pu64(&fcall->tread.offset);
		pu32(&fcall->tread.count);
		break;
	case RRead:
		pu32(&fcall->rread.count);
		pdata(&fcall->rread.data, fcall->rread.count);
		break;
	case TWrite:
		pu32(&fcall->hdr.fid);
		pu64(&fcall->twrite.offset);
		pu32(&fcall->twrite.count);
		pdata(&fcall->twrite.data, fcall->twrite.count);
		break;
	case RWrite:
		pu32(&fcall->rwrite.count);
		break;
	case TClunk:
	case TRemove:
	case TStat:
		pu32(&fcall->hdr.fid);
		break;
	case RStat:
		pu16(&fcall->rstat.nstat);
		pdata((char**)&fcall->rstat.stat, fcall->rstat.nstat);
		break;
	case TWStat: {
		uint16_t size;
		pu32(&fcall->hdr.fid);
		pu16(&size);
		pstat(&fcall->twstat.stat);
		break;
		}
	}
}

/**
 * Function: fcall2msg
 * Function: msg2fcall
 *
 * These functions pack or unpack a 9P protocol message. The
 * message is set to the appropriate mode and its position is
 * set to the begining of its buffer.
 *
 * Returns:
 *	These functions return the size of the message on
 *	success and 0 on failure.
 * See also:
 *	F<Msg>, F<pfcall>
 */
uint
fcall2msg(Msg *msg, Fcall *fcall) {
	uint32_t size;

	msg->end = msg->data + msg->size;
	msg->pos = msg->data + SDWord;
	msg->mode = MsgPack;
	pfcall(msg, fcall);

	if(msg->pos > msg->end)
		return 0;

	msg->end = msg->pos;
	size = msg->end - msg->data;

	msg->pos = msg->data;
	pu32(msg, &size);

	msg->pos = msg->data;
	return size;
}

uint
msg2fcall(Msg *msg, Fcall *fcall) {
	msg->pos = msg->data + SDWord;
	msg->mode = MsgUnpack;
	pfcall(msg, fcall);

	if(msg->pos > msg->end)
		return 0;

	return msg->pos - msg->data;
}

} // end namespace ixp
