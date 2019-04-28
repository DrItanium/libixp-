/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_FCALL_H__
#define LIBJYQ_FCALL_H__
#include <string>
#include <array>
#include <functional>
#include "types.h"
#include "qid.h"
#include "stat.h"
#include "Msg.h"
namespace jyq {
    struct FHdr {
        FType type;
        uint16_t	tag;
        uint32_t	fid;
    };
    struct QueryHeader {
        QueryHeader() = default;
        ~QueryHeader() = default;
        FHdr hdr;
        constexpr auto getType() const noexcept { return hdr.type; }
        constexpr auto getFid() const noexcept { return hdr.fid; }
        constexpr auto getTag() const noexcept { return hdr.tag; }
    };
    struct FVersion : public QueryHeader, public ContainsSizeParameter<uint32_t> {
        char*		version;
    };
    struct FTFlush : public QueryHeader {
        uint16_t	oldtag;
    };
    struct FError : public QueryHeader {
        char*		ename;
    };
    struct FROpen : public QueryHeader {
        Qid		qid; /* +Rattach */
        uint32_t	iounit;
    };
    struct FRAuth : public QueryHeader {
        Qid		aqid;
    };
    struct FAttach : public QueryHeader {
        uint32_t	afid;
        char*		uname;
        char*		aname;
    };
    struct FAttachCxx : public QueryHeader {
        uint32_t     afid;
        std::string  uname;
        std::string  aname;
    };
    struct FTCreate : public QueryHeader {
        uint32_t	perm;
        char*		name;
        uint8_t		mode; /* +Topen */
    };
    struct FTCreateCxx : public QueryHeader {
        uint32_t     perm;
        std::string  name;
        uint8_t      mode; /* +Topen */
    };
    struct FTWalk : public QueryHeader, public ContainsSizeParameter<uint16_t>  {
        uint32_t	newfid;
        char*		wname[maximum::Welem];
    };
    struct FTWalkCxx : public QueryHeader, public ContainsSizeParameter<uint16_t>  {
        uint32_t	newfid;
        std::array<std::string, maximum::Welem> wname;
    };
    struct FRWalk : public QueryHeader, public ContainsSizeParameter<uint16_t> {
        Qid		wqid[maximum::Welem];
    };
    struct FRWalkCxx : public QueryHeader, public ContainsSizeParameter<uint16_t> {
        std::array<Qid, maximum::Welem> wqid;
    };
    struct FIO : public QueryHeader, public ContainsSizeParameter<uint32_t> {
        uint64_t	offset; /* Tread, Twrite */
        char*		data; /* Twrite, Rread */
    };
    struct FRStat : public QueryHeader, public ContainsSizeParameter<uint16_t> {
        uint8_t*	stat;
    };
    struct FTWStat : public QueryHeader {
        Stat		stat;
    };

    /**
     * Type: Fcall
     * Type: FType
     * Type: FAttach
     * Type: FError
     * Type: FHdr
     * Type: FIO
     * Type: FRAuth
     * Type: FROpen
     * Type: FRStat
     * Type: FRWalk
     * Type: FTCreate
     * Type: FTFlush
     * Type: FTWStat
     * Type: FTWalk
     * Type: FVersion
     *
     * The Fcall structure represents a 9P protocol message. The
     * P<hdr> element is common to all Fcall types, and may be used to
     * determine the type and tag of the message. The Fcall type is
     * used heavily in server applications, where it both presents a
     * request to handler functions and returns a response to the
     * client.
     *
     * Each member of the Fcall structure represents a certain
     * message type, which can be discerned from the P<hdr.type> field.
     * This value corresponds to one of the FType constants. Types
     * with significant overlap use the same structures, thus TRead and
     * RWrite are both represented by FIO and can be accessed via the
     * P<io> member as well as P<tread> and P<rwrite> respectively.
     *
     * See also:
     *	T<Srv9>, T<Req9>
     */
    union Fcall {
        FHdr     hdr;
        FVersion version, tversion,
                 rversion;
        FTFlush  tflush;
        FROpen   ropen, rcreate,
                 rattach;
        FError   error;
        FRAuth   rauth;
        FAttach  tattach, tauth;
        FTCreate tcreate, topen;
        FTWalk   twalk;
        FRWalk   rwalk;
        FTWStat  twstat;
        FRStat   rstat;
        FIO      twrite, rwrite, 
                 tread, rread,
                 io;
        void packUnpack(Msg& msg) noexcept;
        static void free(Fcall*);
        FType getType() const noexcept { return hdr.type; }
        decltype(FHdr::fid) getFid() const noexcept { return hdr.fid; }
        void setType(FType type) noexcept { hdr.type = type; }
        void setFid(decltype(FHdr::fid) value) noexcept { hdr.fid = value; }
        void setTypeAndFid(FType type, decltype(FHdr::fid) value) noexcept {
            setType(type);
            setFid(value);
        }
        void setTag(decltype(FHdr::tag) value) noexcept { hdr.tag = value; }
        void setNoTag() noexcept { setTag(NoTag); }
        Fcall() = default;
        Fcall(FType type) {
            setType(type);
        }
        Fcall(FType type, decltype(FHdr::fid) value) : Fcall(type) {
            setFid(value);
        }
    };
    using DoFcallFunc = std::function<bool(Fcall*)>;
    uint	msg2fcall(Msg*, Fcall*);
    uint	fcall2msg(Msg*, Fcall*);
} // end namespace jyq
#endif // end LIBJYQ_FCALL_H__
