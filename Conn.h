/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_STAT_H__
#define LIBJYQ_STAT_H__
#include <any>
#include <functional>
#include "types.h"
namespace jyq {
    struct Server;
    struct Conn {
        ~Conn();
        Server*	srv;
        std::any	aux;	/* Arbitrary pointer, to be used by handlers. */
        int		fd;	/* The file descriptor of the connection. */
        std::function<void(Conn*)> read, close;
        bool		closed;	/* Non-zero when P<fd> has been closed. */

        void    serve9conn();
    };
    void	hangup(Conn*);
} // end namespace jyq
#endif // end LIBJYQ_STAT_H__
