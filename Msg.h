/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_MSG_H__
#define LIBJYQ_MSG_H__

#include "types.h"
#include "qid.h"
#include "stat.h"

namespace jyq {
    struct Qid;
    union Fcall;
    struct Msg : public ContainsSizeParameter<uint> {
        enum class Mode {
            Pack,
            Unpack,
        };
        char*	data; /* Begining of buffer. */
        char*	pos;  /* Current position in buffer. */
        char*	end;  /* End of message. */ 
        void pu8(uint8_t*);
        void pu16(uint16_t*);
        void pu32(uint32_t*);
        void pu64(uint64_t*);
        void pdata(char**, uint);
        void pstring(char**);
        void pstrings(uint16_t*, char**, uint);
        void pqids(uint16_t*, Qid*, uint);
        void pqid(Qid* value) { packUnpack(value); }
        void pqid(Qid& value) { packUnpack(value); }
        void pstat(Stat* value) { packUnpack(value); }
        void pstat(Stat& value) { packUnpack(value); }
        void pfcall(Fcall* value) { packUnpack(value); }
        void pfcall(Fcall& value) { packUnpack(value); }
        static Msg message(char*, uint len, Mode mode);
        template<typename T>
        void packUnpack(T& value) noexcept {
            using K = std::decay_t<T>;
            static_assert(!std::is_same_v<K, Msg>, "This would cause an infinite loop!");
            if constexpr (std::is_same_v<K, uint8_t>) {
                pu8(&value);
            } else if constexpr (std::is_same_v<K, uint16_t>) {
                pu16(&value);
            } else if constexpr (std::is_same_v<K, uint32_t>) {
                pu32(&value);
            } else if constexpr (std::is_same_v<K, uint64_t>) {
                pu64(&value);
            } else {
                value.packUnpack(*this);
            }
        }
        template<typename T>
        void packUnpack(T* value) noexcept {
            using K = std::decay_t<T>;
            static_assert(!std::is_same_v<K, Msg>, "This would cause an infinite loop!");
            if constexpr (std::is_same_v<K, uint8_t>) {
                pu8(value);
            } else if constexpr (std::is_same_v<K, uint16_t>) {
                pu16(value);
            } else if constexpr (std::is_same_v<K, uint32_t>) {
                pu32(value);
            } else if constexpr (std::is_same_v<K, uint64_t>) {
                pu64(value);
            } else {
                value->packUnpack(*this);
            }
        }
        template<typename ... Args>
        void packUnpackMany(Args&& ... fields) noexcept {
            (packUnpack(std::forward<Args>(fields)), ...);
        }
        constexpr bool unpackRequested() const noexcept {
            return _mode == Mode::Unpack;
        }
        constexpr bool packRequested() const noexcept {
            return _mode == Mode::Pack;
        }
        constexpr Mode getMode() const noexcept { 
            return _mode;
        }
        void setMode(Mode mode) noexcept {
            this->_mode = mode;
        }
        private:
           void puint(uint, uint32_t*);
           Mode _mode; /* MsgPack or MsgUnpack. */
    };
} // end namespace jyq
#endif // end LIBJYQ_MSG_H__
