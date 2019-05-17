/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_QID_H__
#define LIBJYQ_QID_H__
#include "types.h"
namespace jyq {
    struct Msg;
    struct Qid {
        constexpr auto getType() const noexcept { return _type; }
        constexpr auto getVersion() const noexcept { return _version; }
        constexpr auto getPath() const noexcept { return _path; }
        constexpr auto getDirType() const noexcept { return _dirType; }
        void setType(uint8_t value) noexcept { _type = value; }
        void setVersion(uint32_t value) noexcept { _version = value; }
        void setPath(uint64_t value) noexcept { _path = value; }
        void setDirType(uint8_t dtype) noexcept { _dirType = dtype; }
        void packUnpack(Msg& msg);
        private:
            uint8_t		_type;
            uint32_t    _version;
            uint64_t    _path;
            uint8_t     _dirType;
    };
} // end namespace jyq
#endif // end LIBJYQ_QID_H__
