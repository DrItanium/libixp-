/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "jyq.h"

namespace jyq {
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
Conn*
Server::listen(int fd, const std::any& aux,
        std::function<void(Conn*)> read,
        std::function<void(Conn*)> close) {
    auto c = new Conn;
	c->fd = fd;
	c->aux = aux;
	c->srv = this;
	c->read = read;
	c->close = close;
	c->next = conn;
    c->closed = false;
	conn = c;
	return c;
}

void 
Server::lock() {
    concurrency::threadModel->lock(&lk);
}

void
Server::unlock() {
    concurrency::threadModel->unlock(&lk);
}

bool
Server::canlock() {
    return concurrency::threadModel->canlock(&lk);
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

void
hangup(Conn *c) {
	Conn **tc;

	auto s = c->srv;
	for(tc=&s->conn; *tc; tc=&(*tc)->next)
		if(*tc == c) break;
	assert(*tc == c);

	*tc = c->next;
	c->closed = true;
	if(c->close)
		c->close(c);
	else
		shutdown(c->fd, SHUT_RDWR);

	::close(c->fd);
    delete c;
}

void
Server::close() {
	Conn *next = nullptr;

	for(auto c = conn; c; c = next) {
		next = c->next;
		hangup(c);
	}
}

void
Server::prepareSelect() {
	FD_ZERO(&this->rd);
	for(auto c = this->conn; c; c = c->next)
		if(c->read) {
			if(this->maxfd < c->fd)
				this->maxfd = c->fd;
			FD_SET(c->fd, &this->rd);
		}
}

void
Server::handleConns() {
	Conn *n;
	for(auto c = this->conn; c; c = n) {
		n = c->next;
		if(FD_ISSET(c->fd, &this->rd)) {
			c->read(c);
        }
	}
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

	running = true;
	concurrency::threadModel->initmutex(&lk);
	while(running) {
		timeval* tvp = nullptr;
		if (long timeout = nexttimer(); timeout > 0) {
			tv.tv_sec = timeout/1000;
			tv.tv_usec = timeout%1000 * 1000;
			tvp = &tv;
		}

		if(preselect) {
			preselect(this);
        }

		if(!running) {
			break;
        }

        prepareSelect();
		if (int r = concurrency::threadModel->select(maxfd + 1, &rd, 0, 0, tvp); r < 0) {
			if(errno == EINTR)
				continue;
			return true;
		}
        handleConns();
	}
	return false;
}

} // end namespace jyq
