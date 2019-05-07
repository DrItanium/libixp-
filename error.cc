/* C++ Implementation copyright (c)2019 Joshua Scoggins */
/* Public Domain --Kris Maglione */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PrintFunctions.h"
#include "thread.h"


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


/**
 * Function: errbuf
 * Function: errstr
 * Function: rerrstr
 * Function: werrstr
 * Variable: vsnprint
 *
 * Params:
 *	buf:  The buffer to read and/or fill.
 *	nbuf: The size of the buffer.
 *	fmt:  A format string with which to write the errstr.
 *	...:  Arguments to P<fmt>.
 *
 * These functions simulate Plan 9's errstr functionality.
 * They replace errno in libjyq. Note that these functions
 * are not internationalized.
 *
 * F<errbuf> returns the errstr buffer for the current
 * thread. F<rerrstr> fills P<buf> with the data from
 * the current thread's error buffer, while F<errstr>
 * exchanges P<buf>'s contents with those of the current
 * thread's error buffer. F<werrstr> formats the given
 * format string, P<fmt>, via V<vsnprint> and writes it to
 * the error buffer.
 *
 * V<vsnprint> may be set to a function which will format
 * its arguments write the result to the P<nbuf> length buffer
 * V<buf>. The default value is F<vsnprintf>. The function must
 * format '%s' as a nul-terminated string and may not consume
 * any arguments not indicated by a %-prefixed format specifier,
 * but may otherwise behave in any manner chosen by the user.
 *
 * See also:
 *	V<vsmprint>
 */
char*
errbuf() {
	auto errbuf = concurrency::threadModel->errbuf();
	if(errno == EINTR) {
		strncpy(errbuf, "interrupted", ErrorMax);
    } else if(errno != EPLAN9) {
		strncpy(errbuf, strerror(errno), ErrorMax);
    }
	return errbuf;
}

void
errstr(char *buf, int nbuf) {
	char tmp[ErrorMax];

	strncpy(tmp, buf, sizeof tmp);
	rerrstr(buf, nbuf);
	strncpy(concurrency::threadModel->errbuf(), tmp, ErrorMax);
	errno = EPLAN9;
}

void
rerrstr(char *buf, int nbuf) {
	strncpy(buf, errbuf(), nbuf);
}

void
werrstr(const char *fmt, ...) {
	char tmp[ErrorMax];
	va_list ap;

	va_start(ap, fmt);
	vsnprint(tmp, sizeof tmp, fmt, ap);
	va_end(ap);
	strncpy(concurrency::threadModel->errbuf(), tmp, ErrorMax);
	errno = EPLAN9;
}

void
werrstr(const std::string& msg) {
    msg.copy(concurrency::threadModel->errbuf(), ErrorMax);
    errno = EPLAN9;
}

Exception::~Exception() noexcept { }

const char*
Exception::what() const noexcept {
    return _message.c_str();
}

} // end namespace jyq
