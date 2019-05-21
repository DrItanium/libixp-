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
            const char* getName() const noexcept { return _name; }
            char* getName() noexcept { return _name; }
            const char* getUid() const noexcept { return _uid; }
            char* getUid() noexcept { return _uid; }
            const char* getGid() const noexcept { return _gid; }
            char* getGid() noexcept { return _gid; }
            const char* getMuid() const noexcept { return _muid; }
            char* getMuid() noexcept { return _muid; }
            uint16_t size() noexcept;
            void setMode(uint32_t value) noexcept { _mode = value; }
            void setAtime(uint32_t value) noexcept { _atime = value; }
            void setMtime(uint32_t value) noexcept { _mtime = value; }
            void setLength(uint64_t value) noexcept { _length = value; }
            void setName(char* value) noexcept { _name = value; }
            void setUid(char* value) noexcept { _uid = value; }
            void setGid(char* value) noexcept { _gid = value; }
            void setMuid(char* value) noexcept { _muid = value; }
            void setType(uint16_t value) noexcept { _type = value; }
            void setDev(uint32_t value) noexcept { _dev = value; }
        private:
            uint16_t	_type;
            uint32_t	_dev;
            Qid         _qid;
            uint32_t	_mode;
            uint32_t	_atime;
            uint32_t	_mtime;
            uint64_t	_length;
            char*	_name;
            char*	_uid;
            char*	_gid;
            char*	_muid;
    };
} // end namespace jyq
#endif // end LIBJYQ_STAT_H__
