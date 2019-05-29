/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_STAT_H__
#define LIBJYQ_STAT_H__
#include "types.h"
#include "qid.h"
namespace jyq {
    struct Msg;
    /* stat structure */
    struct Stat {
        public:
            ~Stat();
            void packUnpack(Msg& msg) noexcept;
            constexpr auto getType() const noexcept { return _type; }
            constexpr auto getDev() const noexcept { return _dev; }
            constexpr auto getMode() const noexcept { return _mode; }
            constexpr auto getAtime() const noexcept { return _atime; }
            constexpr auto getMtime() const noexcept { return _mtime; }
            constexpr auto getLength() const noexcept { return _length; }
            Qid& getQid() noexcept { return _qid; }
            const Qid& getQid() const noexcept { return _qid; }
            const std::string& getName() const noexcept { return _name; }
            std::string& getName() noexcept { return _name; }
            const std::string& getUid() const noexcept { return _uid; }
            std::string& getUid() noexcept { return _uid; }
            const std::string& getGid() const noexcept { return _gid; }
            std::string& getGid() noexcept { return _gid; }
            const std::string& getMuid() const noexcept { return _muid; }
            std::string& getMuid() noexcept { return _muid; }
            uint16_t size() noexcept;
            void setMode(uint32_t value) noexcept { _mode = value; }
            void setAtime(uint32_t value) noexcept { _atime = value; }
            void setMtime(uint32_t value) noexcept { _mtime = value; }
            void setLength(uint64_t value) noexcept { _length = value; }
            void setName(const std::string& value) noexcept { _name = value; }
            void setUid(const std::string& value) noexcept { _uid = value; }
            void setGid(const std::string& value) noexcept { _gid = value; }
            void setMuid(const std::string& value) noexcept { _muid = value; }
            void setType(uint16_t value) noexcept { _type = value; }
            void setDev(uint32_t value) noexcept { _dev = value; }
            void reset() noexcept { 
                _name.clear();
                _uid.clear();
                _gid.clear();
                _muid.clear();
            }
        private:
            uint16_t	_type;
            uint32_t	_dev;
            Qid         _qid;
            uint32_t	_mode;
            uint32_t	_atime;
            uint32_t	_mtime;
            uint64_t	_length;
            std::string _name;
            std::string _uid;
            std::string _gid;
            std::string _muid;
    };
} // end namespace jyq
#endif // end LIBJYQ_STAT_H__
