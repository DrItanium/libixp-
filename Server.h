/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_SERVER_H__
#define LIBJYQ_SERVER_H__
#include <any>
#include <functional>
#include <list>
#include <memory>
#include "types.h"
#include "Conn.h"
#include "thread.h"
#include "timer.h"

namespace jyq {
    struct Server : public HasAux {
        public:
            using Locker = concurrency::Locker<Server>;
        public:
            std::shared_ptr<Conn> listen(int, const std::any&,
                    std::function<void(Conn*)> read,
                    std::function<void(Conn*)> close);
            bool serverloop();
            void close();
            bool unsettimer(long);
            long settimer(long, std::function<void(long, const std::any&)>, const std::any& aux);
            long nexttimer();
            void lock();
            void unlock();
            bool canlock();
            inline void preselect() {
                if (_preselect) {
                    _preselect(this);
                }
            }
            void setPreselect(std::function<void(Server*)> value) noexcept { _preselect = value; }
            auto getPreselect() const noexcept { return _preselect; }
            constexpr auto isRunning() const noexcept { return _running; }
            void setIsRunning(bool value = true) noexcept { _running = value; }
            constexpr auto getMaxFd() const noexcept { return _maxfd; }
            void setMaxFd(int value) noexcept { _maxfd = value; }
            auto getRd() noexcept { return _rd; }
            void setRd(fd_set value) noexcept { _rd = value; }
            auto getTimer() noexcept { return _timer; }
            const auto getTimer() const noexcept { return _timer; }
            void setTimer(Timer* value) noexcept { _timer = value; }
        public:
            std::list<std::shared_ptr<Conn>> conns;
            Mutex	lk;
        private:
            Timer*	_timer;
            std::function<void(Server*)> _preselect;
            bool	_running;
            int		_maxfd;
            fd_set		_rd;
        private:
            void prepareSelect();
            void handleConns();
    };
} // end namespace jyq
#endif // end LIBJYQ_SERVER_H__
