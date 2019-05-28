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
Msg::Msg(char* _data, uint _length, Mode _mode) : Parent(_length), _data(_data), _pos(_data), _end(_data + _length), _mode(_mode) { }

Msg::Msg() : Parent(0), _data(nullptr), _pos(nullptr), _end(nullptr), _mode(Mode::Pack) { }


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
Stat::~Stat() {
}

Fcall::~Fcall() {
}

void
FRStat::purgeStat() noexcept {
    _stat.clear();
}


uint16_t
Stat::size() noexcept {
	return SWord /* size */
		+ SWord /* type */
		+ SDWord /* dev */
		+ SQid /* qid */
		+ (3 * SDWord) /* mode, atime, mtime */
		+ SQWord /* length */
        + computeStringSize(_name)
        + computeStringSize(_uid)
        + computeStringSize(_gid)
        + computeStringSize(_muid);
}
void
FHdr::packUnpack(Msg& msg) 
{
    msg.pu8((uint8_t*)&_type);
    msg.pu16(&_tag);
}
void
FHdr::packUnpackFid(Msg& msg) {
    msg.pu32(&_fid);
}
void
FVersion::packUnpack(Msg& msg) {
    FHdr::packUnpack(msg);
    msg.pu32(&getSizeReference());
    msg.pstring(_version);
}
void
FAttach::packUnpack(Msg& msg) {
    FHdr::packUnpack(msg);
    if (getType() == FType::TAttach) {
        packUnpackFid(msg);
    }
    msg.pu32(&_afid);
    msg.pstring(_uname);
    msg.pstring(_aname);
}

void
FRAuth::packUnpack(Msg& msg) {
    FHdr::packUnpack(msg);
    msg.pqid(&_aqid);
}
void
FROpen::packUnpack(Msg& msg) {
    FHdr::packUnpack(msg);
    msg.pqid(&_qid);
    if (getType() != FType::RAttach) {
        msg.pu32(&_iounit);
    }
}
void
FTWalk::packUnpack(Msg& msg) {
    FHdr::packUnpack(msg);
    packUnpackFid(msg);
    msg.pu32(&_newfid);
    msg.pstrings<maximum::Welem>(getSizeReference(), _wname);
}
void
FTFlush::packUnpack(Msg& msg) {
    FHdr::packUnpack(msg);
    msg.pu16(&_oldtag);
}
void
FError::packUnpack(Msg& msg) {
    FHdr::packUnpack(msg);
    msg.pstring(_ename);
}
void 
FRWalk::packUnpack(Msg& msg) {
    FHdr::packUnpack(msg);
    msg.pqids<maximum::Welem>(getSizeReference(), _wqid);
}
void
FTCreate::packUnpack(Msg& msg) {
    FHdr::packUnpack(msg);
    packUnpackFid(msg);
    if (getType() == FType::TCreate) {
        msg.pstring(_name);
        msg.pu32(&_perm);
    }
    msg.pu8(&_mode);
}
void
FIO::packUnpack(Msg& msg) {
    FHdr::packUnpack(msg);
    auto type = getType();
    if (type == FType::TRead || type == FType::TWrite) {
        packUnpackFid(msg);
        msg.pu64(&_offset);
    }
    msg.pu32(&getSizeReference());
    if (type == FType::RRead || type == FType::TWrite) {
        msg.pdata(_data, size());
    }
}
void
FRStat::packUnpack(Msg& msg) {
    FHdr::packUnpack(msg);
    msg.pu16(&getSizeReference());
    msg.pdata(_stat, size());
}
void
FTWStat::packUnpack(Msg& msg) {
    FHdr::packUnpack(msg);
    packUnpackFid(msg);
    uint16_t size;
	msg.pu16(&size);
    msg.packUnpack(&_stat);
}

Fcall::VariantStorage
Fcall::constructBlankStorage(const FHdr& hdr) {
    Fcall::VariantStorage storage;
    switch (hdr.getType()) {
        case FType::TVersion:
        case FType::RVersion:
            storage.emplace<FVersion>();
            break;
        default:
            throw Exception("Undefined or unimplemented type specified!");
    }

    return storage;
}

void
Fcall::packUnpack(Msg& msg) {
    // if we are in pack mode then we should definitely stop here
    if (!_storage) {
        if (msg.packRequested()) {
            throw Exception("Backing storage is empty!");
        } else if (msg.unpackRequested()) {
            auto startPoint = msg.getPos();
            FHdr tmp;
            tmp.packUnpack(msg);
            msg.setPos(startPoint); // go back to where we started
            _storage = constructBlankStorage(tmp);
        } else {
            throw Exception("Neither pack or unpack requested!");
        }
    }
    // if we get here then there is something to do
    std::visit([&msg](auto&& value) { value.packUnpack(msg); }, *_storage);
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
Msg::unpack(Fcall& val) {
	_pos = _data + SDWord;
    setMode(Msg::Mode::Unpack);
    pfcall(val);

	if(_pos > _end) {
		return 0;
    }

	return _pos - _data;
}

uint
Msg::pack(Fcall& val) {
    //return fcall2msg(this, &val);
	_end = _data + size();
	_pos = _data + SDWord;
    setMode(Msg::Mode::Pack);
    pfcall(val);

	if(_pos > _end) {
		return 0;
    }

	_end = _pos;
	uint32_t size = _end - _data;

	_pos = _data;
    pu32(&size);

    _pos = _data;
	return size;
}

Msg::~Msg() {
    if (_data) {
        delete [] _data;
    }
    _data = nullptr;
    _pos = nullptr;
    _end = nullptr;
}

void
Msg::setData(char* value) noexcept {
    if (_data) {
        // make sure we reclaim things
        delete [] _data;
    }
    _data = value;
}

} // end namespace jyq
