#ifndef LIBJYQ_TYPES_H__
#define LIBJYQ_TYPES_H__
/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */

#include <cstdint>
#include <cstdbool>
#include <iostream>
#include <sys/types.h>

namespace jyq {
    using uint = unsigned int;
    using ulong = unsigned long;
    namespace maximum {
        constexpr auto Version = 32;
        constexpr auto Msg = 8192;
        constexpr auto Error = 128;
        constexpr auto Cache = 32;
        constexpr auto Flen = 128;
        constexpr auto Ulen = 32;
        constexpr auto Welem = 16;
    } // end namespace maximum

    constexpr auto ErrorMax = maximum::Error;

    constexpr auto ApiVersion = 135;
    constexpr auto Version = "9P2000";
    constexpr auto NoTag = uint16_t(~0); 
    constexpr auto NoFid = ~0u;

    template<typename ... Args>
        void print(std::ostream& os, Args&& ... args) {
            (os << ... << args);
        }

    template<typename ... Args>
        void errorPrint(Args&& ... args) {
            jyq::print(std::cerr, args...);
        }

    template<typename ... Args>
        void fatalPrint(Args&& ... args) {
            errorPrint(args...);
            throw 1;
        }

    template<typename T>
    constexpr T min(T a, T b) noexcept {
        return a < b ? a : b;
    }
    template<typename T>
    constexpr T max(T a, T b) noexcept {
        return a > b ? a : b;
    }
    static_assert(min(1,2) == 1);
    static_assert(min(2,1) == 1);
    static_assert(max(1,2) == 2);
    static_assert(max(2,1) == 2);

    template<typename T>
    struct ContainsSizeParameter {
        ContainsSizeParameter() = default;
        ~ContainsSizeParameter() = default;
        constexpr T size() const noexcept { return _value; }
        constexpr bool empty() const noexcept { return size() == 0; }
        T& getSizeReference() noexcept { return _value; }
        void setSize(T value) noexcept { _value = value; }
        private:
            T _value;
    };
} // end namespace jyq

#endif // end LIBJYQ_TYPES_H__
