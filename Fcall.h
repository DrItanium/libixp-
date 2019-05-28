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
#include <variant>
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
            FVersion() = default;
            ~FVersion() = default;
            void packUnpack(Msg& msg);
            std::string& getVersion() noexcept { return _version; }
            const std::string& getVersion() const noexcept { return _version; }
            void setVersion(const std::string& value) noexcept { _version = value; }
            void reset() { _version.clear(); }
        private:
            std::string _version;
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
            std::string& getEname() noexcept { return _ename; }
            const std::string& getEname() const noexcept { return _ename; }
            void setEname(const std::string& value) noexcept { _ename = value; }
            void reset() { _ename.clear(); }
        private:
            std::string _ename;
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
            std::string& getUname() noexcept { return _uname; }
            const std::string getUname() const noexcept { return _uname; }
            std::string getAname() noexcept { return _aname; }
            const std::string getAname() const noexcept { return _aname; }
            void setAfid(uint32_t value) noexcept { _afid = value; }
            void setUname(const std::string& value) noexcept { _uname = value; }
            void setAname(const std::string& value) noexcept { _aname = value; }
            void packUnpack(Msg& msg);
            void reset() {
                _uname.clear();
                _aname.clear();
            }
        private:
            uint32_t	_afid;
            std::string _uname;
            std::string _aname;
    };
    class FTCreate : public FHdr {
        public:
            constexpr auto getPerm() const noexcept { return _perm; }
            constexpr auto getMode() const noexcept { return _mode; }
            const std::string& getName() const noexcept { return _name; }
            std::string& getName() noexcept { return _name; }
            void setPerm(uint32_t perm) noexcept { _perm = perm; }
            void setMode(uint8_t value) noexcept { _mode = value; }
            void setName(const std::string& value) noexcept { _name = value; }
            void packUnpack(Msg& msg);
            void reset() {
                _name.clear();
            }
        private:
            uint32_t	_perm;
            std::string		_name;
            uint8_t		_mode; /* +Topen */
    };
    class FTWalk : public FHdr, public ContainsSizeParameter<uint16_t>  {
        public:
            constexpr auto getNewFid() const noexcept { return _newfid; }
            void setNewFid(uint32_t value) noexcept { _newfid = value; }
            auto& getWname() noexcept { return _wname; }
            constexpr auto getMaximumWnameCount() const noexcept { return maximum::Welem; }
            void packUnpack(Msg& msg);
        private:
            uint32_t _newfid;
            std::array<std::string, maximum::Welem> _wname;
                
    };
    class FRWalk : public FHdr, public ContainsSizeParameter<uint16_t> {
        public:
            void packUnpack(Msg& msg);
            auto& getWqid() noexcept { return _wqid; }
            constexpr auto getWqidMaximum() const noexcept { return maximum::Welem; }
        private:
            std::array<Qid, maximum::Welem> _wqid;
    };
    class FIO : public FHdr, public ContainsSizeParameter<uint32_t> {
        public:
            constexpr auto getOffset() const noexcept { return _offset; }
            const std::string& getData() const noexcept { return _data; }
            std::string& getData() noexcept { return _data; }
            void setOffset(uint64_t value) noexcept { _offset = value; }
            void setData(const std::string& value) noexcept { _data = value; }
            void packUnpack(Msg& msg);
            void reset() { _data.clear(); }
        private: 
            uint64_t  _offset; /* Tread, Twrite */
            std::string _data; /* Twrite, Rread */
    };
    class FRStat : public FHdr, public ContainsSizeParameter<uint16_t> {
        public:
            const std::vector<uint8_t>& getStat() const noexcept { return _stat; }
            std::vector<uint8_t>& getStat() noexcept { return _stat; }
            void packUnpack(Msg& msg);
            void purgeStat() noexcept;
        private:
            std::vector<uint8_t> _stat;
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
    struct Fcall {
        using VariantStorage = std::variant<FVersion, FTFlush, FROpen, FError, FRAuth, FAttach, FTCreate,
            FTWalk, FRWalk, FTWStat, FRStat, FIO>;
        std::optional<VariantStorage> _storage;
        /*
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
        */
        const FHdr& getHeader() const {
            if (_storage) {
                return std::visit([](auto&& value) -> const FHdr& { return value; }, *_storage);
            } else {
                throw Exception("backing storage contains no value!");
            }
        }
        FHdr& getHeader() {
            if (_storage) {
                return std::visit([](auto&& value) -> FHdr& { return value; }, *_storage);
            } else {
                throw Exception("backing storage contains no value!");
            }
        }
        template<typename DesiredType>
        auto& retrieveFromStorage() {
            if (_storage) {
                return std::get<DesiredType>(*_storage);
            } else {
                throw Exception("backing storage contains no value!");
            }
        }
        static VariantStorage constructBlankStorage(const FHdr& type);
        auto& getRauth()  { return retrieveFromStorage<FRAuth>(); }
        auto& getTflush()  { return retrieveFromStorage<FTFlush>(); }
        auto& getVersion()  { return retrieveFromStorage<FVersion>(); }
        auto& getTversion()  { return retrieveFromStorage<FVersion>(); }
        auto& getRversion()  { return retrieveFromStorage<FVersion>(); }
        auto& getRopen()  { return retrieveFromStorage<FROpen>(); }
        auto& getRcreate()  { return retrieveFromStorage<FROpen>(); }
        auto& getRattach()  { return retrieveFromStorage<FROpen>(); }
        auto& getTattach()  { return retrieveFromStorage<FAttach>(); }
        auto& getTauth()  { return retrieveFromStorage<FAttach>(); }
        auto& getTcreate()  { return retrieveFromStorage<FTCreate>(); }
        auto& getTopen()  { return retrieveFromStorage<FTCreate>(); }
        auto& getTWrite()  { return retrieveFromStorage<FIO>(); }
        auto& getRWrite()  { return retrieveFromStorage<FIO>(); }
        auto& getTRead()  { return retrieveFromStorage<FIO>(); }
        auto& getRRead()  { return retrieveFromStorage<FIO>(); }
        auto& getIO()  { return retrieveFromStorage<FIO>(); }
        auto& getTwalk()  { return retrieveFromStorage<FTWalk>(); }
        auto& getRwalk()  { return retrieveFromStorage<FRWalk>(); }
        auto& getError()  { return retrieveFromStorage<FError>(); }
        auto& getTwstat()  { return retrieveFromStorage<FTWStat>(); }
        auto& getRstat()  { return retrieveFromStorage<FRStat>(); }

        //static void free(Fcall*);
        auto getType() const { return getHeader().getType(); }
        auto getFid() const { return getHeader().getFid(); }
        auto getTag() const { return getHeader().getTag(); }
        void setType(FType type) { getHeader().setType(type); }
        void setFid(uint32_t value) { getHeader().setFid(value); }
        void setTag(uint16_t value) { getHeader().setTag(value); }
        void setNoTag() { setTag(NoTag); }
        void setTypeAndFid(FType t, uint32_t fid) {
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
        ~Fcall();
        void packUnpack(Msg& msg);
    };
    using DoFcallFunc = std::function<std::shared_ptr<Fcall>(Fcall&)>;
} // end namespace jyq
#endif // end LIBJYQ_FCALL_H__
