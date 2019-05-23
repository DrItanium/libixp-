/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * C++ Implementation copyright (c)2019 Joshua Scoggins *
 * See LICENSE file for license details.
 */
#include <cerrno>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "Msg.h"
#include "Server.h"
#include "stat.h"
#include "Fcall.h"
#include "Conn.h"

namespace jyq {
Conn::Conn(Server& s, int theFd, std::any a, Conn::Func r, Conn::Func c) : HasAux(a),
    srv(s), _read(r), _close(c), _closed(false), _fd(theFd) { }
/**
 * Function: listen
 * Type: Conn
 *
 * Params:
 *	fs:    The file descriptor on which to listen.
 *	aux:   A piece of data to store in the connection's
 *	       P<aux> member of the Conn data structure.
 *	read:  The function called when the connection has
 *	       data available to read.
 *	close: A cleanup function called when the
 *	       connection is closed.
 *
 * Starts the server P<srv> listening on P<fd>. The optional
 * P<read> and P<close> callbacks are called with the Conn
 * structure for the connection as their sole argument.
 *
 * Returns:
 *	Returns the connection's new Conn data structure.
 *
 * See also:
 *	F<serverloop>, F<serve9conn>, F<hangup>
 */
std::shared_ptr<Conn>
Server::listen(int fd, const std::any& aux,
        std::function<void(Conn*)> read,
        std::function<void(Conn*)> close) {
    _conns.emplace_back(std::make_shared<Conn>(*this, fd, aux, read, close));
    return _conns.back();
}

void 
Server::lock() {
    _lk.lock();
}

void
Server::unlock() {
    _lk.unlock();
}


/**
 * Function: hangup
 * Function: close
 *
 * hangup closes a connection, and stops the server
 * listening on it. It calls the connection's close
 * function, if it exists. close calls hangup
 * on all of the connections on which the server is
 * listening.
 *
 * See also:
 *	F<listen>, S<Server>, S<Conn>
 */

Conn::~Conn() {
    _closed = true;
    if (_close) {
        _close(this);
    } else {
        _fd.shutdown(SHUT_RDWR);
    }
    _fd.close();
}

void
Server::close() {
    _conns.clear();
}

void
Server::prepareSelect() {
	FD_ZERO(&this->_rd);
    for (auto& c : _conns) {
        if (c->getReadFunc()) {
            if (_maxfd < c->getConnection()) {
                _maxfd = c->getConnection();
            }
            FD_SET(c->getConnection(), &_rd);
        }
    }
}

void
Server::handleConns() {
    for (auto& c : _conns) {
        if (FD_ISSET(c->getConnection(), &_rd)) {
            c->getReadFunc()(c.get());
        }
    }
}

void
hangup(Conn *c) {
    delete c;
}

/**
 * Function: serverloop
 * Type: Server
 *
 * Enters the main loop of the server. Exits when
 * P<srv>->running becomes false, or when select(2) returns an
 * error other than EINTR.
 *
 * Returns:
 *	Returns false when the loop exits normally, and true when
 *	it exits on error. V<errno> or the return value of
 *	F<errbuf> may be inspected.
 * See also:
 *	F<listen>, F<settimer>
 */

bool
Server::serverloop() {
	timeval tv;

    setIsRunning();
	while(isRunning()) {
		timeval* tvp = nullptr;
		if (long timeout = nexttimer(); timeout > 0) {
			tv.tv_sec = timeout/1000;
			tv.tv_usec = timeout%1000 * 1000;
			tvp = &tv;
		}
        preselect();

		if(!isRunning()) {
			break;
        }

        prepareSelect();
		if (auto r = ::select(_maxfd + 1, &_rd, 0, 0, tvp); r < 0) {
			if(errno == EINTR) {
				continue;
            }
			return true;
		}
        handleConns();
	}
	return false;
}

} // end namespace jyq
