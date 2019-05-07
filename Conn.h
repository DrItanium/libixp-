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
    struct Conn {
        using Func = std::function<void(Conn*)>;
        Conn(Server& srv, int fd, std::any a, Func read, Func close);
        ~Conn();
        Server&	srv;
        std::any	aux;	/* Arbitrary pointer, to be used by handlers. */
        Connection _fd; /* The file descriptor of the connection. */
        Func read, close;
        bool		closed = false;	/* Non-zero when P<fd> has been closed. */

        void    serve9conn();
    };
    void	hangup(Conn*);
} // end namespace jyq
#endif // end LIBJYQ_CONN_H__
