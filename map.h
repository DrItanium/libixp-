#ifndef LIBJYQ_MAP_H__
#define LIBJYQ_MAP_H__
/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */

#include <functional>
#include "types.h"
#include "thread.h"


namespace jyq {
    struct MapEnt;
    using Map = Map;
    struct Map {
        MapEnt**	bucket;
        int		nhash;

        RWLock	lock;
        void	free(std::function<void(void*)>);
        void	exec(std::function<void(void*, void*)>, void*);
        void	init(MapEnt**, int);
        bool	insert(ulong, void*, bool);
        void*	get(ulong);
        void*	rm(ulong);
    };
} // end namespace jyq

#endif // end LIBJYQ_MAP_H__
