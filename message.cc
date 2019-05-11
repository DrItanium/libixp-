/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "Msg.h"
#include "jyq.h"
#include "argv.h"

namespace jyq {
constexpr auto SByte = 1;
constexpr auto SWord = 2;
constexpr auto SDWord = 4;
constexpr auto SQWord = 8;

auto computeStringSize(const char* str) {
    return SWord + strlen(str);
}
auto computeStringSize(const std::string& str) {
    return SWord + str.length();
}
constexpr auto SQid = SByte + SDWord + SQWord;

/**
 * Type: Msg
 * Type: MsgMode
 * Function: message
 *
 * The Msg struct represents a binary message, and is used
 * extensively by libjyq for converting messages to and from
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
    return Msg(data, length, mode);
}
Msg::Msg(char* _data, uint _length, Mode _mode) {

	data = _data;
	pos = _data;
	end = _data + _length;
    setSize(_length);
    setMode(_mode);
}

Msg::Msg() : data(nullptr), pos(nullptr), end(nullptr), _mode(Mode::Pack) { 
    setSize(0);
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
        ::free(fcall->rstat.getStat());
        fcall->rstat.setStat(nullptr);
		break;
    case FType::RRead:
		::free(fcall->rread.getData());
        fcall->rread.setData(nullptr);
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
        + computeStringSize(name)
        + computeStringSize(uid)
        + computeStringSize(gid)
        + computeStringSize(muid);
}
void
FHdr::packUnpack(Msg& msg) 
{
    msg.pu8((uint8_t*)&type);
    msg.pu16(&tag);
}
void
FHdr::packUnpackFid(Msg& msg) {
    msg.pu32(&fid);
}
void
FVersion::packUnpack(Msg& msg) {
    msg.pu32(&getSizeReference());
    msg.pstring(&version);
}
void
FAttach::packUnpack(Msg& msg) {
    if (getType() == FType::TAttach) {
        packUnpackFid(msg);
    }
    msg.pu32(&afid);
    msg.pstring(&uname);
    msg.pstring(&aname);
}

void
FRAuth::packUnpack(Msg& msg) {
    msg.pqid(&_aqid);
}
void
FROpen::packUnpack(Msg& msg) {
    msg.pqid(&_qid);
    if (getType() != FType::RAttach) {
        msg.pu32(&_iounit);
    }
}
void
FTWalk::packUnpack(Msg& msg) {
    packUnpackFid(msg);
    msg.pu32(&_newfid);
    msg.pstrings(&getSizeReference(), wname, nelem(wname));
}
void
FTFlush::packUnpack(Msg& msg) {
    msg.pu16(&oldtag);
}
void
FError::packUnpack(Msg& msg) {
    msg.pstring(&ename);
}
void 
FRWalk::packUnpack(Msg& msg) {
    msg.pqids(&getSizeReference(), wqid, nelem(wqid));
}
void
FTCreate::packUnpack(Msg& msg) {
    packUnpackFid(msg);
    if (getType() == FType::TCreate) {
        msg.pstring(&_name);
        msg.pu32(&_perm);
    }
    msg.pu8(&_mode);
}
void
FIO::packUnpack(Msg& msg) {
    auto type = getType();
    if (type == FType::TRead || type == FType::TWrite) {
        packUnpackFid(msg);
        msg.pu64(&_offset);
    }
    msg.pu32(&getSizeReference());
    if (type == FType::RRead || type == FType::TWrite) {
        msg.pdata(&_data, size());
    }
}
void
FRStat::packUnpack(Msg& msg) {
    msg.pu16(&getSizeReference());
    msg.pdata((char**)&_stat, size());
}
void
FTWStat::packUnpack(Msg& msg) {
    uint16_t size;
    packUnpackFid(msg);
	msg.pu16(&size);
    msg.packUnpack(&_stat);
}
void
Fcall::packUnpack(Msg& msg) noexcept {
    hdr.packUnpack(msg);

	switch (getType()) {
	case FType::TVersion:
	case FType::RVersion:
        version.packUnpack(msg);
		break;
	case FType::TAuth:
        tauth.packUnpack(msg);
		break;
	case FType::RAuth:
        rauth.packUnpack(msg);
		break;
	case FType::RAttach:
        rattach.packUnpack(msg);
		break;
	case FType::TAttach:
        tattach.packUnpack(msg);
		break;
	case FType::RError:
        error.packUnpack(msg);
		break;
	case FType::TFlush:
        tflush.packUnpack(msg);
		break;
	case FType::TWalk:
        twalk.packUnpack(msg);
		break;
	case FType::RWalk:
        rwalk.packUnpack(msg);
		break;
	case FType::TOpen:
        topen.packUnpack(msg);
		break;
	case FType::ROpen:
	case FType::RCreate:
        ropen.packUnpack(msg);
		break;
	case FType::TCreate:
        tcreate.packUnpack(msg);
		break;
	case FType::TRead:
	case FType::RRead:
	case FType::TWrite:
	case FType::RWrite:
        io.packUnpack(msg);
		break;
	case FType::TClunk:
	case FType::TRemove:
	case FType::TStat:
        hdr.packUnpackFid(msg);
		break;
    case FType::RStat:
        rstat.packUnpack(msg);
		break;
    case FType::TWStat: 
        twstat.packUnpack(msg);
		break;
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
	msg->end = msg->data + msg->size();
	msg->pos = msg->data + SDWord;
    msg->setMode(Msg::Mode::Pack);
    msg->pfcall(fcall);

	if(msg->pos > msg->end)
		return 0;

	msg->end = msg->pos;
	uint32_t size = msg->end - msg->data;

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

} // end namespace jyq
