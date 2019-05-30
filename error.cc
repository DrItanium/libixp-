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
