#ifndef LIBJYQ_UTIL_H__
#define LIBJYQ_UTIL_H__
/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#include "types.h"

namespace jyq {
    void*	emalloc(uint);
    void*	emallocz(uint);
    void	eprint(const char*, ...);
    void*	erealloc(void*, uint);
    char*	estrdup(const char*);
    char*	getNamespace();
    char*	smprint(const char*, ...);
    uint	tokenize(char**, uint len, char*, char);
    std::list<std::string> tokenize(const std::string& str, char delim); 
} // end namespace jyq

#endif // end LIBJYQ_UTIL_H__