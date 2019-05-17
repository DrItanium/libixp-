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
        public:
            constexpr auto getMsec() const noexcept { return _msec; }
            constexpr auto getId() const noexcept { return _id; }
            void setMsec(uint64_t value) noexcept { _msec = value; }
            void setId(long value) noexcept { _id = value; }
            Timer *& getLink() noexcept { return _link; }
            void setLink(Timer* other) noexcept { _link = other; }
        private:
            Timer*		_link;
            uint64_t	_msec;
            long		_id;
        public:
            std::function<void(long, const std::any&)> fn;
            std::any	aux;
    };
    uint64_t msec();
} // end namespace jyq

#endif // end LIBJYQ_TIMER_H__
