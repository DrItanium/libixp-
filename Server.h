/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_CONN_H__
#define LIBJYQ_CONN_H__
#include <any>
#include <functional>
#include <list>
#include <memory>
#include "types.h"
#include "Conn.h"
#include "thread.h"

namespace jyq {
    struct Server {
        std::list<std::shared_ptr<Conn>> conns;
        Mutex	lk;
        Timer*	timer;
        std::function<void(Server*)> _preselect;
        std::any   aux;
        bool	running;
        int		maxfd;
        fd_set		rd;
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
        void preselect() {
            if (_preselect) {
                _preselect(this);
            }
        }
        private:
            void prepareSelect();
            void handleConns();
    };
} // end namespace jyq
#endif // end LIBJYQ_CONN_H__
