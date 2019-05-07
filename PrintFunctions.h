#ifndef LIBJYQ_PRINTFUNCTIONS_H__
#define LIBJYQ_PRINTFUNCTIONS_H__
/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#include <functional>
#include <sstream>
#include "types.h"
namespace jyq {
    union Fcall;
    extern std::function<int(char*, int, const char*, va_list)> vsnprint;
    extern std::function<std::string(const std::string&, va_list)> vsmprint;
    extern std::function<void(Fcall*)> printfcall;
    char*	errbuf();
    void	errstr(char*, int);
    void	rerrstr(char*, int);
    void	werrstr(const char*, ...);
    void    werrstr(const std::string& msg);
    template<typename ... Args>
    void wErrorString(Args&& ... args) {
        std::ostringstream ss;
        print(ss, std::forward(args)...);
        auto str = ss.str();
        werrstr(str);
    }
} // end namespace jyq


#endif // end LIBJYQ_PRINTFUNCTIONS_H__
