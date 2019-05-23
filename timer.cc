/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#include <cstdlib>
#include <sys/time.h>
#include "Msg.h"
#include "jyq.h"
#include "timer.h"
#include "Server.h"


namespace jyq {
/**
 * Function: msec
 *
 * Returns the time since the Epoch in milliseconds.
 */
uint64_t
msec() {
	timeval tv;

	gettimeofday(&tv, 0);
	return (uint64_t)tv.tv_sec*1000 + (uint64_t)tv.tv_usec/1000;
}

/**
 * Function: settimer
 *
 * Params:
 *	msec: The timeout in milliseconds.
 *	fn:   The function to call after P<msec> milliseconds
 *	      have elapsed.
 *	aux:  An arbitrary argument to pass to P<fn> when it
 *	      is called.
 * 
 * Initializes a callback-based timer to be triggerred after
 * P<msec> milliseconds. The timer is passed its id number
 * and the value of P<aux>.
 *
 * Returns:
 *	Returns the new timer's unique id number.
 * See also:
 *	F<unsettimer>, F<serverloop>
 */
long
Server::settimer(long msecs, std::function<void(long, const std::any&)> fn, const std::any& aux) {
    /* 
     * This really needn't be threadsafe, as it has little use in
     * threaded programs, but it nonetheless is.
     */

    static long	lastid = 1;
	Timer **tp;
	Timer *t;
	uint64_t time;

	time = msec() + msecs;

    t = new Timer();
    auto locker = getLock();
    t->setId(lastid++);
    t->setMsec(time);
    t->setFunction(fn);
	t->aux = aux; // make a copy of the contents of the passed in std::aux

	for(tp=&_timer; *tp; tp=&tp[0]->getLink()) {
		if(tp[0]->getMsec() < time) {
			break;
        }
    }
    t->setLink(*tp);
	*tp = t;
	return t->getId();
}

/**
 * Function: unsettimer
 *
 * Params:
 *	id: The id number of the timer to void.
 *
 * Voids the timer identified by P<id>.
 *
 * Returns:
 *	Returns true if a timer was stopped, false
 *	otherwise.
 * See also:
 *	F<settimer>, F<serverloop>
 */
bool
Server::unsettimer(long id) {
	Timer **tp;
	Timer *t;
    auto locker = getLock();
    for(tp=&_timer; (t=*tp); tp=&t->getLink()) {
        if(t->getId() == id) {
            break;
        }
    }
    if(t) {
        *tp = t->getLink();
        delete t;
    }
	return t != nullptr;
}

/*
 * Function: nexttimer
 *
 * Triggers any timers whose timeouts have ellapsed. This is
 * primarily intended to be called from libjyq's select
 * loop.
 *
 * Returns:
 *	Returns the number of milliseconds until the next
 *	timer's timeout.
 * See also:
 *	F<settimer>, F<serverloop>
 */
long
Server::nexttimer() {
	Timer *t = nullptr;
	uint64_t time = 0ull;

    auto locker = getLock();
	while((t = _timer)) {
		time = msec();
		if(t->getMsec() > time) {
			break;
        }
		_timer = t->getLink();

        locker.unlock();
        t->call(t->getId(), t->aux);
        delete t;
        locker.lock();
	}
	if(t) {
		return static_cast<long>(t->getMsec() - time);
    } else {
        return 0;
    }
}
} // end namespace jyq

