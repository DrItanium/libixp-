/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_MSG_H__
#define LIBJYQ_MSG_H__

#include <functional>
#include <cstring>
#include "types.h"
#include "qid.h"
#include "stat.h"

namespace jyq {
    struct Qid;
    struct Fcall;
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
/**
 * Function: pdata
 *
 * Packs or unpacks a raw character buffer of size P<len>.
 *
 * If P<msg>->mode is Msg::Pack, buffer pointed to by P<data> is
 * packed into the buffer at P<msg>->pos. If P<msg>->mode is
 * Msg::Unpack, the address pointed to by P<s> is loaded with a
 * malloc(3) allocated buffer with the contents of the buffer at
 * P<msg>->pos.  In either case, P<msg>->pos is advanced by the
 * number of bytes read or written. If the action would advance
 * P<msg>->pos beyond P<msg>->end, P<msg>->pos is still advanced
 * but no other action is taken.
 *
 * See also:
 *	T<Msg>, F<pstring>
 */
        void pdata(char**, uint);
        void pdata(std::string&, uint);
        void pdata(std::vector<uint8_t>&, uint);
        void pstring(char**);
        void pstring(std::string&);
/**
 * Function: pstrings
 *
 * Packs or unpacks an array of UTF-8 encoded strings. The packed
 * representation consists of a 16-bit element count followed by
 * an array of strings as packed by F<pstring>. The unpacked
 * representation is an array of nul-terminated character arrays.
 *
 * If P<msg>->mode is Msg::Pack, P<*num> strings in the array
 * pointed to by P<strings> are packed into the buffer at
 * P<msg>->pos. If P<msg>->mode is Msg::Unpack, P<*num> is loaded
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
        template<uint max>
        void pstrings(uint16_t& num, std::array<char*, max>& strings) {
            char *s = nullptr;
            uint size = 0;
            uint16_t len = 0;

            pu16(&num);
            if(num > max) {
                _pos = _end+1;
                return;
            }

            if (unpackRequested()) {
                s = _pos;
                size = 0;
                for (auto i = 0; i < num; ++i) {
                    pu16(&len);
                    _pos += len;
                    size += len;
                    if(_pos > _end) {
                        return;
                    }
                }
                _pos = s;
                size += num;
                s = new char[size];
            }

            for(auto i = 0; i < num; ++i) {
                if (packRequested()) {
                    len = strlen(strings[i]);
                }
                pu16(&len);

                if (unpackRequested()) {
                    memcpy(s, _pos, len);
                    strings[i] = s;
                    s += len;
                    _pos += len;
                    *s++ = '\0';
                } else {
                    pdata(&strings[i], len);
                }
            }
        }
        template<uint max>
        void pstrings(uint16_t& num, std::array<std::string, max>& strings) {
            pu16(&num);
            if(num > max) {
                _pos = _end+1;
                return;
            }


            for(auto i = 0; i < num; ++i) {
                uint16_t len = 0;
                if (packRequested()) {
                    len = strings[i].length();
                }
                pu16(&len);

                if (unpackRequested()) {
                    strings[i] = std::string(_pos, len);
                } else {
                    pstring(strings[i]);
                }
            }
        }
        template<uint max>
        void pqids(uint16_t& num, std::array<Qid, max>& qid) {
            pu16(&num);
            if(num > max) {
                _pos = _end + 1;
                return;
            }

            for(auto i = 0; i < num; i++) {
                pqid(&qid[i]);
            }
        }
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
                               auto do1 = [&v, pos]() {
                                   v |= pos[0];
                               };
                               auto do2 = [&v, pos, do1]() {
                                   v |= pos[1]<<8;
                                   do1();
                               };
                               auto do4 = [&v, pos, do2]() {
                                   v |= pos[3]<<24;
                                   v |= pos[2]<<16;
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
