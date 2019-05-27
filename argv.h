#ifndef LIBJYQ_ARGV_H__
#define LIBJYQ_ARGV_H__
/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */

#include "types.h"
#include <map>
#include <functional>
#include <list>

extern char* argv0;
#define ARGBEGIN \
    int _argtmp=0, _inargv=0; char *_argv=nullptr; \
if(!argv0) {argv0=*argv; argv++, argc--;} \
_inargv=1; \
while(argc && argv[0][0] == '-') { \
    _argv=&argv[0][1]; argv++; argc--; \
    if(_argv[0] == '-' && _argv[1] == '\0') \
    break; \
    while(*_argv) switch(*_argv++)
#define ARGEND }_inargv=0

#define EARGF(f) ((_inargv && *_argv) ? \
        (_argtmp=strlen(_argv), _argv+=_argtmp, _argv-_argtmp) \
        : ((argc > 0) ? \
            (--argc, ++argv, *(argv-1)) \
            : ((f), (char*)0)))
#define ARGF() EARGF(0)
#define nelem(ary) (sizeof(ary) / sizeof(*ary))
namespace jyq {
    class Argument {
        public:
            Argument(const std::string& value) : _key(value) { }
            ~Argument() = default;
            const std::string& getKey() const noexcept { return _key; }
            bool isOption() const noexcept {
                return (_key.length() > 1 && _key.front() == '-');
            }
            std::string asKey() const noexcept {
                return isOption() ? _key.substr(1) : "";
            }
        private:
            std::string _key;

    };
    inline std::list<Argument> convert(int argc, char** argv) {
        std::list<Argument> out;
        for (int i = 0; i < argc; ++i) {
            out.emplace_back(argv[i]);
        }
        return out;
    }

} // end namespace jyq
#endif // end LIBJYQ_ARGV_H__
