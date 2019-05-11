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
        void packUnpack(Msg& msg);
        void packUnpackFid(Msg& msg);
        constexpr auto getType() const noexcept { return type; }
        constexpr auto getTag() const noexcept { return tag; }
        constexpr auto getFid() const noexcept { return fid; }
        FHdr() = default;
        ~FHdr() = default;
    };
    struct FVersion : public FHdr, public ContainsSizeParameter<uint32_t> {
        char*		version;
        void packUnpack(Msg& msg);
    };
    struct FTFlush : public FHdr {
        uint16_t	oldtag;
        constexpr auto getOldTag() const noexcept { return oldtag; }
        void packUnpack(Msg& msg);
    };
    struct FError : public FHdr {
        char*		ename;
        void packUnpack(Msg& msg);
    };
    struct FROpen : public FHdr {
        Qid		qid; /* +Rattach */
        uint32_t	iounit;
        constexpr auto getIoUnit() const noexcept { return iounit; }
        void packUnpack(Msg& msg);
    };
    struct FRAuth : public FHdr {
        Qid		aqid;
        void packUnpack(Msg& msg);
    };
    struct FAttach : public FHdr {
        uint32_t	afid;
        char*		uname;
        char*		aname;
        void packUnpack(Msg& msg);
        constexpr auto getAfid() const noexcept { return afid; }
    };
    struct FTCreate : public FHdr {
        uint32_t	perm;
        char*		name;
        uint8_t		mode; /* +Topen */
        constexpr auto getPerm() const noexcept { return perm; }
        constexpr auto getMode() const noexcept { return mode; }
        void packUnpack(Msg& msg);
    };
    struct FTWalk : public FHdr, public ContainsSizeParameter<uint16_t>  {
        char*		wname[maximum::Welem];
        constexpr auto getNewFid() const noexcept { return _newfid; }
        void setNewFid(uint32_t value) noexcept { _newfid = value; }
        void packUnpack(Msg& msg);
        private:
            uint32_t _newfid;
                
    };
    struct FRWalk : public FHdr, public ContainsSizeParameter<uint16_t> {
        Qid		wqid[maximum::Welem];
        void packUnpack(Msg& msg);
    };
    struct FIO : public FHdr, public ContainsSizeParameter<uint32_t> {
        void packUnpack(Msg& msg);
        constexpr auto getOffset() const noexcept { return _offset; }
        void setOffset(uint64_t value) noexcept { _offset = value; }
        char* getData() const noexcept { return _data; }
        void setData(char* value) noexcept { _data = value; }
        private: 
            uint64_t  _offset; /* Tread, Twrite */
            char*     _data; /* Twrite, Rread */
    };
    struct FRStat : public FHdr, public ContainsSizeParameter<uint16_t> {
        void packUnpack(Msg& msg);
        uint8_t* getStat() const noexcept { return _stat; }
        void setStat(uint8_t* value) noexcept { _stat = value; }
        private:
            uint8_t* _stat;
    };
    struct FTWStat : public FHdr {
        void packUnpack(Msg& msg);
        Stat& getStat() noexcept { return _stat; }
        const Stat& getStat() const noexcept { return _stat; }
        void setStat(const Stat& stat) noexcept { _stat = stat; }
        private:
            Stat _stat;
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
        static void free(Fcall*);
        constexpr FType getType() const noexcept { return hdr.type; }
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
        void packUnpack(Msg& msg) noexcept;
    };
    using DoFcallFunc = std::function<bool(Fcall*)>;
    uint	msg2fcall(Msg*, Fcall*);
    uint	fcall2msg(Msg*, Fcall*);
} // end namespace jyq
#endif // end LIBJYQ_FCALL_H__
