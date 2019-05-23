/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_CONN_H__
#define LIBJYQ_CONN_H__
#include <any>
#include <functional>
#include "types.h"
#include "socket.h"
namespace jyq {
    struct Server;
    struct Conn : public HasAux {
        using Func = std::function<void(Conn*)>;
        Conn(Server& srv, int fd, std::any a, Func read, Func close);
        ~Conn();

        void    serve9conn();
        /**
         * Returns a reference to the connection underlying this conn object
         * @return the connection object stored in this class
         */
        Connection& getConnection() noexcept { return _fd; }
        constexpr auto isClosed() const noexcept { return _closed; }
        Func getReadFunc() const noexcept { return _read; }
        Func getCloseFunc() const noexcept { return _close; }
        void setReadFunc(Func value) noexcept { _read = value; }
        void setCloseFunc(Func value) noexcept { _close = value; }
        void setClosed(bool value = true) noexcept { _closed = value; }
        uint recvmsg(Msg& msg) { return getConnection().recvmsg(msg); }
        uint sendmsg(Msg& msg) { return getConnection().sendmsg(msg); }

        public:
            Server&	srv;
        private:
            Func _read, _close;
            bool _closed = false;
            Connection _fd;

    };
    void	hangup(Conn*);
} // end namespace jyq
#endif // end LIBJYQ_CONN_H__
