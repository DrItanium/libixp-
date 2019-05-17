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
            auto getFunction() noexcept { return _fn; }
            void setFunction(std::function<void(long, const std::any&)> value) noexcept { _fn = value; }
            void call(long a, const std::any& b) {
                if (_fn) {
                    _fn(a, b);
                }
            }
            void operator()(long a, const std::any& b) { call(a, b); }
        private:
            Timer*		_link;
            uint64_t	_msec;
            long		_id;
            std::function<void(long, const std::any&)> _fn;
        public:
            std::any	aux;
    };
    uint64_t msec();
} // end namespace jyq

#endif // end LIBJYQ_TIMER_H__
