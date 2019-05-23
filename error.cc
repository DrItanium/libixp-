/* C++ Implementation copyright (c)2019 Joshua Scoggins */
/* Public Domain --Kris Maglione */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "PrintFunctions.h"


namespace jyq {
namespace {
int
_vsnprint(char *buf, int nbuf, const char *fmt, va_list ap) {
	return vsnprintf(buf, nbuf, fmt, ap);
}

std::string
_vsmprint(const std::string& fmt, va_list ap) {
	va_list al;
	va_copy(al, ap);
	auto n = vsnprintf(nullptr, 0, fmt.c_str(), al);
	va_end(al);
	if(auto buf = std::make_unique<char[]>(++n); buf) {
		vsnprintf(buf.get(), n, fmt.c_str(), ap);
        return std::string(buf.get());
    } else {
        return "";
    }
}
/* Approach to errno handling taken from Plan 9 Port. */
constexpr auto EPLAN9 = 0x19283745;
} // end namespace

    std::function<int(char*,int,const char*, va_list)> vsnprint = _vsnprint;
    std::function<std::string(const std::string&, va_list)> vsmprint = _vsmprint;


char*
errbuf() {
    static char* buf = nullptr;
    if (!buf) {
        buf = new char[ErrorMax+1];
        buf[ErrorMax] = '\0';
    }
	return buf;
}

void
werrstr(const std::string& msg) {
    throw Exception(msg);
}

Exception::~Exception() noexcept { }

const char*
Exception::what() const noexcept {
    return _message.c_str();
}

} // end namespace jyq
