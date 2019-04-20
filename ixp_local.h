#ifndef LIBIXP_LOCAL_H__
#define  LIBIXP_LOCAL_H__
#define IXP_NO_P9_
#include <ixp.h>
#include <stdbool.h>
#include <iostream>

#undef ulong
#define ulong _ixpulong
typedef unsigned long ulong;

#ifdef CPROTO
typedef char* va_list;
#endif
extern char* argv0;
#define ARGBEGIN \
    int _argtmp=0, _inargv=0; char *_argv=nullptr; \
if(!argv0) {argv0=*argv; argv++, argc--;} \
_inargv=1; USED(_inargv); \
while(argc && argv[0][0] == '-') { \
    _argv=&argv[0][1]; argv++; argc--; \
    if(_argv[0] == '-' && _argv[1] == '\0') \
    break; \
    while(*_argv) switch(*_argv++)
#define ARGEND }_inargv=0;USED(_argtmp, _argv, _inargv)

#define EARGF(f) ((_inargv && *_argv) ? \
        (_argtmp=strlen(_argv), _argv+=_argtmp, _argv-_argtmp) \
        : ((argc > 0) ? \
            (--argc, ++argv, _used(argc), *(argv-1)) \
            : ((f), (char*)0)))
#define ARGF() EARGF(_used(0))

#ifndef KENC
static inline void _used(long a, ...) { if(a){} }
# define USED(...) _used((long)__VA_ARGS__)
# define SET(x) (x = 0)
/* # define SET(x) USED(&x) GCC 4 is 'too smart' for this. */
#endif

#define nelem(ary) (sizeof(ary) / sizeof(*ary))

namespace ixp {
    struct MapEnt;
    using Map = Map;
    using Timer = Timer;


    struct Map {
        MapEnt**	bucket;
        int		nhash;

        RWLock	lock;
    };

    struct Timer {
        Timer*		link;
        uint64_t	msec;
        long		id;
        std::function<void(long, const std::any&)> fn;
        std::any	aux;
    };

    /* map.c */
    void	mapfree(Map*, std::function<void(void*)>);
    void	mapexec(Map*, std::function<void(void*, void*)>, void*);
    void	mapinit(Map*, MapEnt**, int);
    bool	mapinsert(Map*, ulong, void*, bool);
    void*	mapget(Map*, ulong);
    void*	maprm(Map*, ulong);

    /* rpc.c */
} // end namespace ixp

#endif
