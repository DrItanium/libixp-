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
message(char *data, uint length, uint mode) {
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
freestat(Stat *s) {
	free(s->name);
	free(s->uid);
	free(s->gid);
	free(s->muid);
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
sizeof_stat(Stat *stat) {
	return SWord /* size */
		+ SWord /* type */
		+ SDWord /* dev */
		+ SQid /* qid */
		+ (3 * SDWord) /* mode, atime, mtime */
		+ SQWord /* length */
		+ SString(stat->name)
		+ SString(stat->uid)
		+ SString(stat->gid)
		+ SString(stat->muid);
}

void
pfcall(Msg *msg, Fcall *fcall) {
	pu8(msg, &fcall->hdr.type);
	pu16(msg, &fcall->hdr.tag);

	switch (fcall->hdr.type) {
	case TVersion:
	case RVersion:
		pu32(msg, &fcall->version.msize);
		pstring(msg, &fcall->version.version);
		break;
	case TAuth:
		pu32(msg, &fcall->tauth.afid);
		pstring(msg, &fcall->tauth.uname);
		pstring(msg, &fcall->tauth.aname);
		break;
	case RAuth:
		pqid(msg, &fcall->rauth.aqid);
		break;
	case RAttach:
		pqid(msg, &fcall->rattach.qid);
		break;
	case TAttach:
		pu32(msg, &fcall->hdr.fid);
		pu32(msg, &fcall->tattach.afid);
		pstring(msg, &fcall->tattach.uname);
		pstring(msg, &fcall->tattach.aname);
		break;
	case RError:
		pstring(msg, &fcall->error.ename);
		break;
	case TFlush:
		pu16(msg, &fcall->tflush.oldtag);
		break;
	case TWalk:
		pu32(msg, &fcall->hdr.fid);
		pu32(msg, &fcall->twalk.newfid);
		pstrings(msg, &fcall->twalk.nwname, fcall->twalk.wname, nelem(fcall->twalk.wname));
		break;
	case RWalk:
		pqids(msg, &fcall->rwalk.nwqid, fcall->rwalk.wqid, nelem(fcall->rwalk.wqid));
		break;
	case TOpen:
		pu32(msg, &fcall->hdr.fid);
		pu8(msg, &fcall->topen.mode);
		break;
	case ROpen:
	case RCreate:
		pqid(msg, &fcall->ropen.qid);
		pu32(msg, &fcall->ropen.iounit);
		break;
	case TCreate:
		pu32(msg, &fcall->hdr.fid);
		pstring(msg, &fcall->tcreate.name);
		pu32(msg, &fcall->tcreate.perm);
		pu8(msg, &fcall->tcreate.mode);
		break;
	case TRead:
		pu32(msg, &fcall->hdr.fid);
		pu64(msg, &fcall->tread.offset);
		pu32(msg, &fcall->tread.count);
		break;
	case RRead:
		pu32(msg, &fcall->rread.count);
		pdata(msg, &fcall->rread.data, fcall->rread.count);
		break;
	case TWrite:
		pu32(msg, &fcall->hdr.fid);
		pu64(msg, &fcall->twrite.offset);
		pu32(msg, &fcall->twrite.count);
		pdata(msg, &fcall->twrite.data, fcall->twrite.count);
		break;
	case RWrite:
		pu32(msg, &fcall->rwrite.count);
		break;
	case TClunk:
	case TRemove:
	case TStat:
		pu32(msg, &fcall->hdr.fid);
		break;
	case RStat:
		pu16(msg, &fcall->rstat.nstat);
		pdata(msg, (char**)&fcall->rstat.stat, fcall->rstat.nstat);
		break;
	case TWStat: {
		uint16_t size;
		pu32(msg, &fcall->hdr.fid);
		pu16(msg, &size);
		pstat(msg, &fcall->twstat.stat);
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
