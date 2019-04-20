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
void
puint(Msg *msg, uint size, uint32_t *val) {
	uint8_t *pos;
	int v;

	if(msg->pos + size <= msg->end) {
		pos = (uint8_t*)msg->pos;
        switch(msg->mode) {
            case MsgPack:
                v = *val;
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
            case MsgUnpack:
                v = 0;
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
                *val = v;
        }
	}
	msg->pos += size;
}
} // end namespace

/**
 * Function: pu8
 * Function: pu16
 * Function: pu32
 * Function: pu64
 *
 * These functions pack or unpack an unsigned integer of the
 * specified size.
 *
 * If P<msg>->mode is MsgPack, the value pointed to by P<val> is
 * packed into the buffer at P<msg>->pos. If P<msg>->mode is
 * MsgUnpack, the packed value at P<msg>->pos is loaded into the
 * location pointed to by P<val>. In both cases, P<msg>->pos is
 * advanced by the number of bytes read or written. If the call
 * would advance P<msg>->pos beyond P<msg>->end, P<msg>->pos is
 * advanced, but nothing is modified.
 *
 * See also:
 *	T<Msg>
 */
void
pu8(Msg *msg, uint8_t *val) {
	uint32_t v;

	v = *val;
	puint(msg, SByte, &v);
	*val = (uint8_t)v;
}
void
pu16(Msg *msg, uint16_t *val) {
	uint32_t v;

	v = *val;
	puint(msg, SWord, &v);
	*val = (uint16_t)v;
}
void
pu32(Msg *msg, uint32_t *val) {
	puint(msg, SDWord, val);
}
void
pu64(Msg *msg, uint64_t *val) {
	uint32_t vl, vb;

	vl = (uint)*val;
	vb = (uint)(*val>>32);
	puint(msg, SDWord, &vl);
	puint(msg, SDWord, &vb);
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
 * If P<msg>->mode is MsgPack, the string pointed to by P<s> is
 * packed into the buffer at P<msg>->pos. If P<msg>->mode is
 * MsgUnpack, the address pointed to by P<s> is loaded with a
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
pstring(Msg *msg, char **s) {
	uint16_t len;

	if(msg->mode == MsgPack)
		len = strlen(*s);
	pu16(msg, &len);

	if(msg->pos + len <= msg->end) {
		if(msg->mode == MsgUnpack) {
			*s = (char*)ixp::emalloc(len + 1);
			memcpy(*s, msg->pos, len);
			(*s)[len] = '\0';
		}else
			memcpy(msg->pos, *s, len);
	}
	msg->pos += len;
}

/**
 * Function: pstrings
 *
 * Packs or unpacks an array of UTF-8 encoded strings. The packed
 * representation consists of a 16-bit element count followed by
 * an array of strings as packed by F<pstring>. The unpacked
 * representation is an array of nul-terminated character arrays.
 *
 * If P<msg>->mode is MsgPack, P<*num> strings in the array
 * pointed to by P<strings> are packed into the buffer at
 * P<msg>->pos. If P<msg>->mode is MsgUnpack, P<*num> is loaded
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
pstrings(Msg *msg, uint16_t *num, char *strings[], uint max) {
	char *s;
	uint i, size;
	uint16_t len;

	pu16(msg, num);
	if(*num > max) {
		msg->pos = msg->end+1;
		return;
	}

	SET(s);
	if(msg->mode == MsgUnpack) {
		s = msg->pos;
		size = 0;
		for(i=0; i < *num; i++) {
			pu16(msg, &len);
			msg->pos += len;
			size += len;
			if(msg->pos > msg->end)
				return;
		}
		msg->pos = s;
		size += *num;
		s = (char*)ixp::emalloc(size);
	}

	for(i=0; i < *num; i++) {
		if(msg->mode == MsgPack)
			len = strlen(strings[i]);
		pu16(msg, &len);

		if(msg->mode == MsgUnpack) {
			memcpy(s, msg->pos, len);
			strings[i] = (char*)s;
			s += len;
			msg->pos += len;
			*s++ = '\0';
		}else
			pdata(msg, &strings[i], len);
	}
}

/**
 * Function: pdata
 *
 * Packs or unpacks a raw character buffer of size P<len>.
 *
 * If P<msg>->mode is MsgPack, buffer pointed to by P<data> is
 * packed into the buffer at P<msg>->pos. If P<msg>->mode is
 * MsgUnpack, the address pointed to by P<s> is loaded with a
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
pdata(Msg *msg, char **data, uint len) {
    if(msg->pos + len <= msg->end) {
        if(msg->mode == MsgUnpack) {
            *data = (char*)ixp::emalloc(len);
            memcpy(*data, msg->pos, len);
        } else {
            memcpy(msg->pos, *data, len);
        }
    }
	msg->pos += len;
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
pqid(Msg *msg, Qid *qid) {
	pu8(msg, &qid->type);
	pu32(msg, &qid->version);
	pu64(msg, &qid->path);
}

void
pqids(Msg *msg, uint16_t *num, Qid qid[], uint max) {

	pu16(msg, num);
	if(*num > max) {
		msg->pos = msg->end+1;
		return;
	}

	for(auto i = 0; i < *num; i++) {
		pqid(msg, &qid[i]);
    }
}

void
pstat(Msg *msg, Stat *stat) {
	uint16_t size;

	if(msg->mode == MsgPack) {
        size = stat->getSize() - 2;
    }

	pu16(msg, &size);
	pu16(msg, &stat->type);
	pu32(msg, &stat->dev);
	pqid(msg, &stat->qid);
	pu32(msg, &stat->mode);
	pu32(msg, &stat->atime);
	pu32(msg, &stat->mtime);
	pu64(msg, &stat->length);
	pstring(msg, &stat->name);
	pstring(msg, &stat->uid);
	pstring(msg, &stat->gid);
	pstring(msg, &stat->muid);
}
} // end namespace ixp
