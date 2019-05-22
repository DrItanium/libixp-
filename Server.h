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
        public:
            std::list<std::shared_ptr<Conn>> conns;
            Mutex	lk;
            Timer*	timer;
        private:
            std::function<void(Server*)> _preselect;
            bool	_running;
        public:
            int		maxfd;
            fd_set		rd;
        private:
            void prepareSelect();
            void handleConns();
    };
} // end namespace jyq
#endif // end LIBJYQ_SERVER_H__
