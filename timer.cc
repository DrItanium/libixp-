/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#include <cassert>
#include <cstdlib>
#include <sys/time.h>
#include "jyq.h"
#include "timer.h"


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

	t = (decltype(t))emallocz(sizeof *t);
    lock();
	t->id = lastid++;
	t->msec = time;
	t->fn = fn;
	t->aux = aux; // make a copy of the contents of the passed in std::aux

	for(tp=&timer; *tp; tp=&tp[0]->link)
		if(tp[0]->msec < time)
			break;
	t->link = *tp;
	*tp = t;
    unlock();
	return t->id;
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
    lock();
    for(tp=&timer; (t=*tp); tp=&t->link) {
        if(t->id == id) {
            break;
        }
    }
    if(t) {
        *tp = t->link;
        free(t);
    }
    unlock();
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
	Timer *t;
	uint64_t time;

    lock();
	while((t = timer)) {
		time = msec();
		if(t->msec > time) {
			break;
        }
		timer = t->link;

        unlock();
		t->fn(t->id, t->aux);
		free(t);
        lock();
	}
	long ret = 0;
	if(t) {
		ret = t->msec - time;
    }
    unlock();
	return ret;
}
} // end namespace jyq

