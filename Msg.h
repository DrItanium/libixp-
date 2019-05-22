/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_MSG_H__
#define LIBJYQ_MSG_H__

#include <functional>
#include "types.h"
#include "qid.h"
#include "stat.h"

namespace jyq {
    struct Qid;
    union Fcall;
    struct Msg : public ContainsSizeParameter<uint> {
        public:
            using Parent = ContainsSizeParameter<uint>;
            enum class Mode {
                Pack,
                Unpack,
            };
            char* getEnd() noexcept { return _end; }
            const char* getEnd() const noexcept { return _end; }
            void setEnd(char* value) noexcept { _end = value; }
            char* getPos() noexcept { return _pos; }
            const char* getPos() const noexcept { return _pos; }
            void setPos(char* value) noexcept { _pos = value; }
            template<typename T>
            void advancePosition(T amount) noexcept { 
                _pos += amount;
            }
            char* getData() noexcept { return _data; }
            const char* getData() const noexcept { return _data; }
            void setData(char* value) noexcept;
            void pointToFront() noexcept { _pos = _data; }
        private:
            char*	_data; /* Begining of buffer. */
            char*	_pos;  /* Current position in buffer. */
            char*	_end;  /* End of message. */ 
        public:
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
        Msg();
        Msg(char*, uint, Mode);
        ~Msg();
        using Action = std::function<void(Msg&)>;
        void packUnpack(Action pack, Action unpack);
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
        public:
            void alloc(uint n);
            uint pack(Fcall& value);
            uint unpack(Fcall& value);
        private:
           enum class NumberSize : uint {
                SByte = 1,
                SWord = 2,
                SDWord = 4,
           };
           template<NumberSize size>
           void puint(uint32_t* val) {
               if ((_pos + uint(size)) <= _end) {
                   auto pos = (uint8_t*)_pos;
                   switch(getMode()) {
                       case Msg::Mode::Pack: 
                           [pos, val]() {
                               int v = *val;
                               auto do1 = [pos, v]() { pos[0] = v; };
                               auto do2 = [pos, v, do1]() { 
                                   pos[1] = v>>8; 
                                   do1();
                               };
                               auto do4 = [pos, v, do2]() {
                                   pos[3] = v>>24;
                                   pos[2] = v>>16;
                                   do2();
                               };

                               switch(size) {
                                   case NumberSize::SDWord:
                                       do4();
                                       break;
                                   case NumberSize::SWord:
                                       do2();
                                       break;
                                   case NumberSize::SByte:
                                       do1();
                                       break;
                               }
                           }();
                           break;
                       case Msg::Mode::Unpack: 
                           *val = [pos]() {
                               auto v = 0;
                               switch(size) {
                                   case NumberSize::SDWord:
                                       v |= pos[3]<<24;
                                       v |= pos[2]<<16;
                                   case NumberSize::SWord:
                                       v |= pos[1]<<8;
                                   case NumberSize::SByte:
                                       v |= pos[0];
                                       break;
                               }
                               return v;
                           }();
                           break;
                   }
               }
               _pos += uint(size);
           }
           Mode _mode; /* MsgPack or MsgUnpack. */
    };
} // end namespace jyq
#endif // end LIBJYQ_MSG_H__
