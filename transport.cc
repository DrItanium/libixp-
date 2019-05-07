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

    msg->setMode(Msg::Mode::Unpack);
	msg->pos = msg->data;
	msg->end = msg->data + msg->size();
	if(jyq::readn(fd, msg, SSize) != SSize)
		return 0;

	msg->pos = msg->data;
    msg->pu32(&msize);

	uint32_t size = msize - SSize;
	if(size >= msg->end - msg->pos) {
		werrstr("message too large");
		return 0;
	}
	if(jyq::readn(fd, msg, size) != size) {
		werrstr("message incomplete");
		return 0;
	}

	msg->end = msg->pos;
	return msize;
}
int
Connection::readn(Msg& msg, size_t count) {
    auto num = count;
    while (num > 0) {
        auto r = mread(msg, num);
        if (r == -1 && errno == EINTR) {
            continue;
        }
        if (r == 0) {
            werrstr("broken pipe: %s", errbuf());
            return count = num;
        }
        num -= r;
    }
    return count - num;
}

int
Connection::mread(Msg& msg, size_t count) {
    //std::cout << "fd = " << fd << std::endl;
	auto n = msg.end - msg.pos;
	if (n <= 0) {
		werrstr("buffer full");
		return -1;
	}
	if(n > count) {
		n = count;
    }

    auto r = read(msg.pos, n);
	if(r > 0) {
        msg.pos += r;
    }
	return r;
}

uint
Connection::sendmsg(Msg& msg) {
    msg.pos = msg.data;
	while(msg.pos < msg.end) {
        if (auto r = write(msg.pos, msg.end - msg.pos); r < 1) {
			if(errno == EINTR) {
				continue;
            }
			werrstr("broken pipe: %s", errbuf());
			return 0;
		} else {
            msg.pos += r;
        }
	}
    return msg.pos - msg.data;
}

uint
Connection::recvmsg(Msg& msg) {
    static constexpr auto SSize = 4;
	uint32_t msize;

    msg.setMode(Msg::Mode::Unpack);
	msg.pos = msg.data;
	msg.end = msg.data + msg.size();
    if (readn(msg, SSize) != SSize) {
        return 0;
    }

	msg.pos = msg.data;
    msg.pu32(&msize);

	uint32_t size = msize - SSize;
	if(size >= msg.end - msg.pos) {
		werrstr("message too large");
		return 0;
	}
    if (readn(msg, size) != size) {
		werrstr("message incomplete");
		return 0;
	}

    msg.end = msg.pos;
	return msize;

}

} // end namespace jyq
