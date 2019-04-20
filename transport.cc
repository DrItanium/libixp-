/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "ixp_local.h"

namespace ixp {
namespace {
auto 
mread(int fd, Msg *msg, uint count) {
	auto n = msg->end - msg->pos;
	if (n <= 0) {
		werrstr("buffer full");
		return -1;
	}
	if(n > count) {
		n = count;
    }

	int r = concurrency::threadModel->read(fd, msg->pos, n);
	if(r > 0)
		msg->pos += r;
	return r;
}

int
readn(int fd, Msg *msg, uint count) {
	auto num = count;
	while(num > 0) {
		auto r = mread(fd, msg, num);
		if(r == -1 && errno == EINTR)
			continue;
		if(r == 0) {
			werrstr("broken pipe: %s", errbuf());
			return count - num;
		}
		num -= r;
	}
	return count - num;
}
} // end namespace


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
uint
sendmsg(int fd, Msg *msg) {
	msg->pos = msg->data;
	while(msg->pos < msg->end) {
		if (auto r = concurrency::threadModel->write(fd, msg->pos, msg->end - msg->pos); r < 1) {
			if(errno == EINTR) {
				continue;
            }
			werrstr("broken pipe: %s", errbuf());
			return 0;
		} else {
		    msg->pos += r;
        }
	}
	return msg->pos - msg->data;
}

uint
recvmsg(int fd, Msg *msg) {
    static constexpr auto SSize = 4;
	uint32_t msize;

	msg->mode = Msg::Unpack;
	msg->pos = msg->data;
	msg->end = msg->data + msg->size;
	if(ixp::readn(fd, msg, SSize) != SSize)
		return 0;

	msg->pos = msg->data;
    msg->pu32(&msize);

	uint32_t size = msize - SSize;
	if(size >= msg->end - msg->pos) {
		werrstr("message too large");
		return 0;
	}
	if(ixp::readn(fd, msg, size) != size) {
		werrstr("message incomplete");
		return 0;
	}

	msg->end = msg->pos;
	return msize;
}

} // end namespace ixp
