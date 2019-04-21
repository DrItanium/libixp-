/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ixp_local.h"



namespace ixp {
namespace {
constexpr auto SByte = 1;
constexpr auto SWord = 2;
constexpr auto SDWord = 4;
constexpr auto SQWord = 8;
} // end namespace

void
Msg::puint(uint size, uint32_t *val) {
	if ((this->pos + size) <= end) {
		auto pos = (uint8_t*)this->pos;
        auto performPack = [pos, size, val]() {
                int v = *val;
                switch(size) {
                    case SDWord:
                        pos[3] = v>>24;
                        pos[2] = v>>16;
                    case SWord:
                        pos[1] = v>>8;
                    case SByte:
                        pos[0] = v;
                        break;
                }
        };
        auto performUnpack = [pos, size]() {
            auto v = 0;
            switch(size) {
                case SDWord:
                    v |= pos[3]<<24;
                    v |= pos[2]<<16;
                case SWord:
                    v |= pos[1]<<8;
                case SByte:
                    v |= pos[0];
                    break;
            }
            return v;
        };
        switch(mode) {
            case Msg::Mode::Pack: 
                performPack(); 
                break;
            case Msg::Mode::Unpack: {
                *val = performUnpack();
                break;
            }
        }
	}
    pos += size;
}

/**
 * Function: pu8
 * Function: pu16
 * Function: pu32
 * Function: pu64
 *
 * These functions pack or unpack an unsigned integer of the
 * specified size.
 *
 * If P<msg>->mode is Msg::Pack, the value pointed to by P<val> is
 * packed into the buffer at P<msg>->pos. If P<msg>->mode is
 * Msg::Unpack, the packed value at P<msg>->pos is loaded into the
 * location pointed to by P<val>. In both cases, P<msg>->pos is
 * advanced by the number of bytes read or written. If the call
 * would advance P<msg>->pos beyond P<msg>->end, P<msg>->pos is
 * advanced, but nothing is modified.
 *
 * See also:
 *	T<Msg>
 */
void
Msg::pu8(uint8_t *val) {
	uint32_t v = *val;
	puint(SByte, &v);
	*val = (uint8_t)v;
}
void
Msg::pu16(uint16_t *val) {
	uint32_t v = *val;
	puint(SWord, &v);
	*val = (uint16_t)v;
}
void
Msg::pu32(uint32_t *val) {
	puint(SDWord, val);
}
void
Msg::pu64(uint64_t *val) {
	uint32_t vl = (uint)*val;
	uint32_t vb = (uint)(*val>>32);
	puint(SDWord, &vl);
	puint(SDWord, &vb);
	*val = vl | ((uint64_t)vb<<32);
}

/**
 * Function: pstring
 *
 * Packs or unpacks a UTF-8 encoded string. The packed
 * representation of the string consists of a 16-bit unsigned
 * integer followed by the contents of the string. The unpacked
 * representation is a nul-terminated character array.
 *
 * If P<msg>->mode is Msg::Pack, the string pointed to by P<s> is
 * packed into the buffer at P<msg>->pos. If P<msg>->mode is
 * Msg::Unpack, the address pointed to by P<s> is loaded with a
 * malloc(3) allocated, nul-terminated representation of the
 * string packed at P<msg>->pos. In either case, P<msg>->pos is
 * advanced by the number of bytes read or written. If the
 * action would advance P<msg>->pos beyond P<msg>->end,
 * P<msg>->pos is still advanced but no other action is taken.
 *
 * See also:
 *	T<Msg>, F<pstrings>, F<pdata>
 */
void
Msg::pstring(char **s) {
	uint16_t len;

    if (packRequested()) {
		len = strlen(*s);
    }
	pu16(&len);

	if((pos + len) <= end) {
        if (unpackRequested()) {
			*s = (char*)ixp::emalloc(len + 1);
			memcpy(*s, pos, len);
			(*s)[len] = '\0';
		} else {
			memcpy(pos, *s, len);
        }
	}
	pos += len;
}

/**
 * Function: pstrings
 *
 * Packs or unpacks an array of UTF-8 encoded strings. The packed
 * representation consists of a 16-bit element count followed by
 * an array of strings as packed by F<pstring>. The unpacked
 * representation is an array of nul-terminated character arrays.
 *
 * If P<msg>->mode is Msg::Pack, P<*num> strings in the array
 * pointed to by P<strings> are packed into the buffer at
 * P<msg>->pos. If P<msg>->mode is Msg::Unpack, P<*num> is loaded
 * with the number of strings unpacked, the array at
 * P<*strings> is loaded with pointers to the unpacked strings,
 * and P<(*strings)[0]> must be freed by the user. In either
 * case, P<msg>->pos is advanced by the number of bytes read or
 * written. If the action would advance P<msg>->pos beyond
 * P<msg>->end, P<msg>->pos is still advanced, but no other
 * action is taken. If P<*num> is greater than P<max>,
 * P<msg>->pos is set beyond P<msg>->end and no other action is
 * taken.
 * 
 * See also:
 *	P<Msg>, P<pstring>, P<pdata>
 */
void
Msg::pstrings(uint16_t *num, char *strings[], uint max) {
	char *s = nullptr;
	uint size = 0;
	uint16_t len = 0;

	pu16(num);
	if(*num > max) {
		pos = end+1;
		return;
	}

	SET(s);
    if (unpackRequested()) {
		s = pos;
		size = 0;
        for (auto i = 0; i < *num; ++i) {
			pu16(&len);
			pos += len;
			size += len;
			if(pos > end)
				return;
		}
		pos = s;
		size += *num;
		s = (char*)ixp::emalloc(size);
	}

	for(auto i = 0; i < *num; ++i) {
        if (packRequested()) {
			len = strlen(strings[i]);
        }
		pu16(&len);

        if (unpackRequested()) {
			memcpy(s, pos, len);
			strings[i] = (char*)s;
			s += len;
			pos += len;
			*s++ = '\0';
		} else {
			pdata(&strings[i], len);
        }
	}
}

/**
 * Function: pdata
 *
 * Packs or unpacks a raw character buffer of size P<len>.
 *
 * If P<msg>->mode is Msg::Pack, buffer pointed to by P<data> is
 * packed into the buffer at P<msg>->pos. If P<msg>->mode is
 * Msg::Unpack, the address pointed to by P<s> is loaded with a
 * malloc(3) allocated buffer with the contents of the buffer at
 * P<msg>->pos.  In either case, P<msg>->pos is advanced by the
 * number of bytes read or written. If the action would advance
 * P<msg>->pos beyond P<msg>->end, P<msg>->pos is still advanced
 * but no other action is taken.
 *
 * See also:
 *	T<Msg>, F<pstring>
 */
void
Msg::pdata(char **data, uint len) {
    if(pos + len <= end) {
        if (unpackRequested()) {
            *data = (char*)ixp::emalloc(len);
            memcpy(*data, pos, len);
        } else {
            memcpy(pos, *data, len);
        }
    }
	pos += len;
}

/**
 * Function: pfcall
 * Function: pqid
 * Function: pqids
 * Function: pstat
 * Function: sizeof_stat
 *
 * These convenience functions pack or unpack the contents of
 * libixp structures into their wire format. They behave as if
 * F<pu8>, F<pu16>, F<pu32>, F<pu64>, and
 * F<pstring> were called for each member of the structure
 * in question. pqid is to pqid as F<pstrings> is to
 * pstring.
 *
 * sizeof_stat returns the size of the packed represention
 * of P<stat>.
 *
 * See also:
 *	T<Msg>, F<pu8>, F<pu16>, F<pu32>,
 *	F<pu64>, F<pstring>, F<pstrings>
 */
void
Qid::packUnpack(Msg& msg) {
    msg.packUnpackMany(type, version, path);
	//msg.pu8(&type);
	//msg.pu32(&version);
	//msg.pu64(&path);
}

void
Msg::pqids(uint16_t *num, Qid qid[], uint max) {

	pu16(num);
	if(*num > max) {
        pos = end + 1;
		return;
	}

	for(auto i = 0; i < *num; i++) {
        pqid(&qid[i]);
    }
}

void
Stat::packUnpack(Msg& msg) noexcept {
    uint16_t totalSize = 0;
    if (msg.packRequested()) {
        totalSize = (size() - 2);
    }
    msg.pu16(&totalSize);
	msg.pu16(&type);
	msg.pu32(&dev);
	msg.pqid(&qid);
	msg.pu32(&mode);
	msg.pu32(&atime);
	msg.pu32(&mtime);
	msg.pu64(&length);
	msg.pstring(&name);
	msg.pstring(&uid);
	msg.pstring(&gid);
	msg.pstring(&muid);
}
} // end namespace ixp
