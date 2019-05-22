/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <cerrno>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "Msg.h"
#include "jyq.h"
#include "socket.h"

namespace jyq {


/**
 * Function: sendmsg
 * Function: recvmsg
 *
 * These functions read and write messages to and from the given
 * file descriptors.
 *
 * sendmsg writes the data at P<msg>->pos upto P<msg>->end.
 * If the call returns non-zero, all data is assured to have
 * been written.
 *
 * recvmsg first reads a 32 bit, little-endian length from
 * P<fd> and then reads a message of that length (including the
 * 4 byte size specifier) into the buffer at P<msg>->data, so
 * long as the size is less than P<msg>->size.
 *
 * Returns:
 *	These functions return the number of bytes read or
 *	written, or 0 on error. Errors are stored in
 *	F<errbuf>.
 */
int
Connection::readn(Msg& msg, size_t count) {
    auto num = count;
    while (num > 0) {
        auto r = mread(msg, num);
        if (r == -1 && errno == EINTR) {
            continue;
        }
        if (r == 0) {
            wErrorString("broken pipe: ", errbuf());
            return count = num;
        }
        num -= r;
    }
    return count - num;
}

int
Connection::mread(Msg& msg, size_t count) {
    //std::cout << "fd = " << fd << std::endl;
	auto n = msg.getEnd() - msg.getPos();
	if (n <= 0) {
        wErrorString("buffer full");
		return -1;
	}
	if(n > count) {
		n = count;
    }

    auto r = read(msg.getPos(), n);
	if(r > 0) {
        msg.setPos(msg.getPos() + r);
    }
	return r;
}

uint
Connection::sendmsg(Msg& msg) {
    msg.setPos(msg.data);
	while(msg.getPos() < msg.getEnd()) {
        if (auto r = write(msg.getPos(), msg.getEnd() - msg.getPos()); r < 1) {
			if(errno == EINTR) {
				continue;
            }
            wErrorString("broken pipe: ", errbuf());
			return 0;
		} else {
            msg.setPos(msg.getPos() + r);
        }
	}
    return msg.getPos() - msg.data;
}

uint
Connection::recvmsg(Msg& msg) {
    static constexpr auto SSize = 4;

    msg.setMode(Msg::Mode::Unpack);
    msg.setPos(msg.data);
	msg.setEnd(msg.data + msg.size());
    if (readn(msg, SSize) != SSize) {
        return 0;
    }

    msg.setPos(msg.data);
    uint32_t msize;
    msg.pu32(&msize);

	uint32_t size = msize - SSize;
	if(size >= msg.getEnd()- msg.getPos()) {
        wErrorString("message too large");
		return 0;
	}
    if (readn(msg, size) != size) {
        wErrorString("message incomplete");
		return 0;
	}

    msg.setEnd(msg.getPos());
	return msize;

}


} // end namespace jyq
