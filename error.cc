/* C++ Implementation copyright (c)2019 Joshua Scoggins */
/* Public Domain --Kris Maglione */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"

namespace jyq {

Exception::~Exception() noexcept { }

const char*
Exception::what() const noexcept {
    return _message.c_str();
}

} // end namespace jyq
