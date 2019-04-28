#ifndef LIBJYQ_TYPES_H__
#define LIBJYQ_TYPES_H__
/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */

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

} // end namespace jyq

#endif // end LIBJYQ_TYPES_H__
