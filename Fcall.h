/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_FCALL_H__
#define LIBJYQ_FCALL_H__
#include <string>
#include <array>
#include <functional>
#include <memory>
#include "types.h"
#include "qid.h"
#include "stat.h"
#include "Msg.h"
namespace jyq {
    class FHdr {
        public:
            void packUnpack(Msg& msg);
            void packUnpackFid(Msg& msg);
            constexpr auto getType() const noexcept { return _type; }
            constexpr auto getTag() const noexcept { return _tag; }
            constexpr auto getFid() const noexcept { return _fid; }
            void setType(FType value) noexcept { _type = value; }
            void setTag(uint16_t value) noexcept { _tag = value; }
            void setFid(uint32_t value) noexcept { _fid = value; }
            FHdr() = default;
            ~FHdr() = default;
        private:
            FType _type;
            uint16_t _tag;
            uint32_t _fid;

    };
    class FVersion : public FHdr, public ContainsSizeParameter<uint32_t> {
        public:
            using FHdr::FHdr;
            void packUnpack(Msg& msg);
            char* getVersion() noexcept { return _version; }
            const char* getVersion() const noexcept { return _version; }
            void setVersion(char* value) noexcept { _version = value; }
        private:
            char* _version;
    };
    class FTFlush : public FHdr {
        public:
            using FHdr::FHdr;
            constexpr auto getOldTag() const noexcept { return _oldtag; }
            void setOldTag(uint16_t oldtag) noexcept { _oldtag = oldtag; }
            void packUnpack(Msg& msg);
        private:
            uint16_t _oldtag;
    };
    class FError : public FHdr {
        public:
            void packUnpack(Msg& msg);
            char* getEname() noexcept { return _ename; }
            const char* getEname() const noexcept { return _ename; }
            void setEname(char* value) noexcept { _ename = value; }
        private:
            char* _ename;
    };
    class FROpen : public FHdr {
        public:
            constexpr auto getIoUnit() const noexcept { return _iounit; }
            void setIoUnit(uint32_t value) noexcept { _iounit = value; }
            void packUnpack(Msg& msg);
            const Qid& getQid() const noexcept { return _qid; }
            Qid& getQid() noexcept { return _qid; }
            void setQid(const Qid& value) noexcept { _qid = value; }
        private: 
            Qid		_qid; /* +Rattach */
            uint32_t	_iounit;
    };
    class FRAuth : public FHdr {
        public:
            const Qid& getAQid() const noexcept { return _aqid; }
            Qid& getAQid() noexcept { return _aqid; }
            void setAQid(const Qid& aq) noexcept { _aqid = aq; }
            void packUnpack(Msg& msg);
        private:
            Qid		_aqid;
    };
    class FAttach : public FHdr {
        public:
            constexpr auto getAfid() const noexcept { return _afid; }
            char* getUname() noexcept { return _uname; }
            const char* getUname() const noexcept { return _uname; }
            char* getAname() noexcept { return _aname; }
            const char* getAname() const noexcept { return _aname; }
            void setAfid(uint32_t value) noexcept { _afid = value; }
            void setUname(char* value) noexcept { _uname = value; }
            void setAname(char* value) noexcept { _aname = value; }
            void packUnpack(Msg& msg);
        private:
            uint32_t	_afid;
            char*		_uname;
            char*		_aname;
    };
    class FTCreate : public FHdr {
        public:
            constexpr auto getPerm() const noexcept { return _perm; }
            constexpr auto getMode() const noexcept { return _mode; }
            const char* getName() const noexcept { return _name; }
            char* getName() noexcept { return _name; }
            void setPerm(uint32_t perm) noexcept { _perm = perm; }
            void setMode(uint8_t value) noexcept { _mode = value; }
            void setName(char* value) noexcept { _name = value; }
            void packUnpack(Msg& msg);
        private:
            uint32_t	_perm;
            char*		_name;
            uint8_t		_mode; /* +Topen */
    };
    class FTWalk : public FHdr, public ContainsSizeParameter<uint16_t>  {
        public:
            constexpr auto getNewFid() const noexcept { return _newfid; }
            void setNewFid(uint32_t value) noexcept { _newfid = value; }
            char** getWname() noexcept { return _wname; }
            constexpr auto getMaximumWnameCount() const noexcept { return maximum::Welem; }
            void packUnpack(Msg& msg);
        private:
            uint32_t _newfid;
            char*    _wname[maximum::Welem];
                
    };
    class FRWalk : public FHdr, public ContainsSizeParameter<uint16_t> {
        public:
            void packUnpack(Msg& msg);
            Qid* getWqid() noexcept { return _wqid; }
            constexpr auto getWqidMaximum() const noexcept { return maximum::Welem; }
        private:
            Qid		_wqid[maximum::Welem];
    };
    class FIO : public FHdr, public ContainsSizeParameter<uint32_t> {
        public:
            constexpr auto getOffset() const noexcept { return _offset; }
            const char* getData() const noexcept { return _data; }
            char* getData() noexcept { return _data; }
            void setOffset(uint64_t value) noexcept { _offset = value; }
            void setData(char* value) noexcept { _data = value; }
            void packUnpack(Msg& msg);
        private: 
            uint64_t  _offset; /* Tread, Twrite */
            char*     _data; /* Twrite, Rread */
    };
    class FRStat : public FHdr, public ContainsSizeParameter<uint16_t> {
        public:
            const uint8_t* getStat() const noexcept { return _stat; }
            uint8_t* getStat() noexcept { return _stat; }
            void setStat(uint8_t* value) noexcept { _stat = value; }
            void packUnpack(Msg& msg);
            void purgeStat() noexcept;
        private:
            uint8_t* _stat;
    };
    class FTWStat : public FHdr {
        public:
            Stat& getStat() noexcept { return _stat; }
            const Stat& getStat() const noexcept { return _stat; }
            void setStat(const Stat& stat) noexcept { _stat = stat; }
            void packUnpack(Msg& msg);
        private:
            Stat _stat;
    };
    struct Msg;
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
        FHdr     _hdr;
        FVersion _version;
        FTFlush  _tflush;
        FROpen   _ropen;
        FError   _error;
        FRAuth   _rauth;
        FAttach  _tattach;
        FTCreate _tcreate;
        FTWalk   _twalk;
        FRWalk   _rwalk;
        FTWStat  _twstat;
        FRStat   _rstat;
        FIO      _io;
        const auto& getHeader() const noexcept { return _hdr; }
        auto& getHeader() noexcept { return _hdr; }
        auto& getVersion() noexcept { return _version; }
        auto& getTversion() noexcept { return _version; }
        auto& getRversion() noexcept { return _version; }
        auto& getRopen() noexcept { return _ropen; }
        auto& getRcreate() noexcept { return _ropen; }
        auto& getRattach() noexcept { return _ropen; }
        auto& getTattach() noexcept { return _tattach; }
        auto& getTauth() noexcept { return _tattach; }
        auto& getTcreate() noexcept { return _tcreate; }
        auto& getTopen() noexcept { return _tcreate; }
        auto& getTWrite() noexcept { return _io; }
        auto& getRWrite() noexcept { return _io; }
        auto& getTRead() noexcept { return _io; }
        auto& getRRead() noexcept { return _io; }
        auto& getIO() noexcept { return _io; }
        auto& getTwalk() noexcept { return _twalk; }
        auto& getRwalk() noexcept { return _rwalk; }
        auto& getError() noexcept { return _error; }
        auto& getTwstat() noexcept { return _twstat; }
        auto& getRstat() noexcept { return _rstat; }
        //static void free(Fcall*);
        constexpr auto getType() const noexcept { return _hdr.getType(); }
        constexpr auto getFid() const noexcept { return _hdr.getFid(); }
        constexpr auto getTag() const noexcept { return _hdr.getTag(); }
        void setType(FType type) noexcept { getHeader().setType(type); }
        void setFid(uint32_t value) noexcept { getHeader().setFid(value); }
        void setTag(uint16_t value) noexcept { getHeader().setTag(value); }
        void setNoTag() noexcept { setTag(NoTag); }
        void setTypeAndFid(FType t, uint32_t fid) noexcept {
            setType(t);
            setFid(fid);
        }
        Fcall() = default;
        Fcall(FType type) {
            setType(type);
        }
        Fcall(FType type, uint32_t fid) : Fcall(type) {
            setFid(fid);
        }
        void reset();
        ~Fcall();
        void packUnpack(Msg& msg) noexcept;
    };
    using DoFcallFunc = std::function<std::shared_ptr<Fcall>(Fcall&)>;
} // end namespace jyq
#endif // end LIBJYQ_FCALL_H__
