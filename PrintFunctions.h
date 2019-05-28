#ifndef LIBJYQ_PRINTFUNCTIONS_H__
#define LIBJYQ_PRINTFUNCTIONS_H__
/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#include <functional>
#include <sstream>
#include <stdexcept>
#include "types.h"
namespace jyq {
    extern std::function<int(char*, int, const char*, va_list)> vsnprint;
    extern std::function<std::string(const std::string&, va_list)> vsmprint;

    char*	errbuf();
    template<typename ... Args>
    void wErrorString(Args&& ... args) {
        std::ostringstream ss;
        print(ss, args...);
        auto str = ss.str();
        throw Exception(str);
    }
} // end namespace jyq


#endif // end LIBJYQ_PRINTFUNCTIONS_H__
