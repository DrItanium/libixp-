#ifndef LIBJYQ_TIMER_H__
#define LIBJYQ_TIMER_H__
/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */

#include <functional>
#include <any>
#include "types.h"


namespace jyq {
    struct Timer {
        Timer*		link;
        uint64_t	msec;
        long		id;
        std::function<void(long, const std::any&)> fn;
        std::any	aux;
    };
    uint64_t msec();
} // end namespace jyq

#endif // end LIBJYQ_TIMER_H__
