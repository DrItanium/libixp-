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
Msg::message(char *data, uint length, Mode mode) {
	Msg m;

	m.data = data;
	m.pos = data;
	m.end = data + length;
	m.size = length;
    m.setMode(mode);
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
Stat::free(Stat *s) {
	::free(s->name);
	::free(s->uid);
	::free(s->gid);
	::free(s->muid);
	s->name = s->uid = s->gid = s->muid = nullptr;
}

void
Fcall::free(Fcall *fcall) {
    switch(fcall->getType()) {
    case FType::RStat:
		::free(fcall->rstat.stat);
		fcall->rstat.stat = nullptr;
		break;
    case FType::RRead:
		::free(fcall->rread.data);
		fcall->rread.data = nullptr;
		break;
    case FType::RVersion:
		::free(fcall->version.version);
		fcall->version.version = nullptr;
		break;
    case FType::RError:
		::free(fcall->error.ename);
		fcall->error.ename = nullptr;
		break;
    default:
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
Fcall::packUnpack(Msg& msg) noexcept {
	msg.pu8((uint8_t*)&hdr.type);
	msg.pu16(&hdr.tag);

	switch (getType()) {
	case FType::TVersion:
	case FType::RVersion:
		msg.pu32(&version.getSizeReference());
		msg.pstring(&version.version);
		break;
	case FType::TAuth:
		msg.pu32(&tauth.afid);
		msg.pstring(&tauth.uname);
		msg.pstring(&tauth.aname);
		break;
	case FType::RAuth:
		msg.pqid(&rauth.aqid);
		break;
	case FType::RAttach:
		msg.pqid(&rattach.qid);
		break;
	case FType::TAttach:
		msg.pu32(&hdr.fid);
		msg.pu32(&tattach.afid);
		msg.pstring(&tattach.uname);
		msg.pstring(&tattach.aname);
		break;
	case FType::RError:
		msg.pstring(&error.ename);
		break;
	case FType::TFlush:
		msg.pu16(&tflush.oldtag);
		break;
	case FType::TWalk:
		msg.pu32(&hdr.fid);
		msg.pu32(&twalk.newfid);
		msg.pstrings(&twalk.getSizeReference(), twalk.wname, nelem(twalk.wname));
		break;
	case FType::RWalk:
		msg.pqids(&rwalk.getSizeReference(), rwalk.wqid, nelem(rwalk.wqid));
		break;
	case FType::TOpen:
		msg.pu32(&hdr.fid);
		msg.pu8(&topen.mode);
		break;
	case FType::ROpen:
	case FType::RCreate:
		msg.pqid(&ropen.qid);
		msg.pu32(&ropen.iounit);
		break;
	case FType::TCreate:
		msg.pu32(&hdr.fid);
		msg.pstring(&tcreate.name);
		msg.pu32(&tcreate.perm);
		msg.pu8(&tcreate.mode);
		break;
	case FType::TRead:
		msg.pu32(&hdr.fid);
		msg.pu64(&tread.offset);
		msg.pu32(&tread.getSizeReference());
		break;
	case FType::RRead:
		msg.pu32(&rread.getSizeReference());
		msg.pdata(&rread.data, rread.size());
		break;
	case FType::TWrite:
		msg.pu32(&hdr.fid);
		msg.pu64(&twrite.offset);
		msg.pu32(&twrite.getSizeReference());
		msg.pdata(&twrite.data, twrite.size());
		break;
	case FType::RWrite:
		msg.pu32(&rwrite.getSizeReference());
		break;
	case FType::TClunk:
	case FType::TRemove:
	case FType::TStat:
		msg.pu32(&hdr.fid);
		break;
    case FType::RStat:
		msg.pu16(&rstat.getSizeReference());
		msg.pdata((char**)&rstat.stat, rstat.size());
		break;
    case FType::TWStat: {
		uint16_t size;
		msg.pu32(&hdr.fid);
		msg.pu16(&size);
        msg.packUnpack(&twstat.stat);
		break;
		}
    default:
        break;
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
    msg->setMode(Msg::Mode::Pack);
    msg->pfcall(fcall);

	if(msg->pos > msg->end)
		return 0;

	msg->end = msg->pos;
	size = msg->end - msg->data;

	msg->pos = msg->data;
    msg->pu32(&size);

	msg->pos = msg->data;
	return size;
}

uint
msg2fcall(Msg *msg, Fcall *fcall) {
	msg->pos = msg->data + SDWord;
    msg->setMode(Msg::Mode::Unpack);
    msg->pfcall(fcall);

	if(msg->pos > msg->end)
		return 0;

	return msg->pos - msg->data;
}

} // end namespace ixp
