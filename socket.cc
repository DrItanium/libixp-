/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <map>
#include <string>
#include "Msg.h"
#include "jyq.h"
#include "socket.h"


/* Note: These functions modify the strings that they are passed.
 *   The lookup function duplicates the original string, so it is
 *   not modified.
 */

#ifndef SUN_LEN
/* From FreeBSD's sys/su.h */
#define SUN_LEN(su) \
	(sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif
namespace jyq {
namespace {
std::string
get_port(const std::string& addr) {
    if (auto spos = addr.find('!'); spos == std::string::npos) {
        wErrorString("no port provided");
        return std::string();
    } else {
        return addr.substr(spos);
    }
}

int
sock_unix(const std::string& address, sockaddr_un *sa, socklen_t *salen) {

	memset(sa, 0, sizeof *sa);

	sa->sun_family = AF_UNIX;
	strncpy(sa->sun_path, address.c_str(), sizeof sa->sun_path);
	*salen = SUN_LEN(sa);

	if (auto fd = socket(AF_UNIX, SOCK_STREAM, 0); fd < 0) {
        return -1;
    } else {
        return fd;
    }
}

int
dial_unix(const std::string& address) {
	sockaddr_un sa;
	socklen_t salen;

    if (int fd = sock_unix(address, &sa, &salen); fd == -1) {
        return fd;
    } else {
        if(connect(fd, (sockaddr*) &sa, salen)) {
            ::close(fd);
            return -1;
        }
        return fd;
    }
}

int
announce_unix(const std::string& file) {
	const int yes = 1;
	sockaddr_un sa;
	socklen_t salen;

	signal(SIGPIPE, SIG_IGN);

	int fd = sock_unix(file, &sa, &salen);
	if(fd == -1)
		return fd;

	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof yes) < 0)
		goto fail;

	unlink(file.c_str());
	if(bind(fd, (sockaddr*)&sa, salen) < 0)
		goto fail;

	chmod(file.c_str(), S_IRWXU);
	if(::listen(fd, maximum::Cache) < 0)
		goto fail;

	return fd;

fail:
	::close(fd);
	return -1;
}

template<bool announce>
addrinfo*
alookup(const std::string& host) {
	/* Truncates host at '!' */
    if (auto port = get_port(host); !port.empty()) {
        return nullptr;
    } else {
        bool useHost = true;
        addrinfo hints;
        addrinfo* ret;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if constexpr (announce) {
            hints.ai_flags = AI_PASSIVE;
            if (!host.compare("*")) {
                useHost = false;
            }
        }

        if (int err = getaddrinfo(useHost ? host.c_str() : nullptr, port.c_str(), &hints, &ret); err) {
            wErrorString("getaddrinfo: ", gai_strerror(err));
            return nullptr;
        } else {
            return ret;
        }
    }
}

int
ai_socket(addrinfo *ai) {
	return socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
}

int
dial_tcp(const std::string& host) {
	if (auto aip = alookup<false>(host); !aip) {
        return -1;
    } else {
        int fd;
        for(auto ai = aip; ai; ai = ai->ai_next) {
            fd = ai_socket(ai);
            if(fd == -1) {
                wErrorString("socket: ", strerror(errno));
                continue;
            }

            if(connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
                break;

            wErrorString("connect: ", strerror(errno));
            ::close(fd);
            fd = -1;
        }

        freeaddrinfo(aip);
        return fd;
    }
}

int
announce_tcp(const std::string& host) {
	addrinfo *ai, *aip;
	int fd;

	aip = alookup<true>(host);
    if (!aip)
		return -1;

	/* Probably don't need to loop */
	for(ai = aip; ai; ai = ai->ai_next) {
		fd = ai_socket(ai);
		if(fd == -1)
			continue;

		if(bind(fd, ai->ai_addr, ai->ai_addrlen) < 0)
			goto fail;

		if(::listen(fd, maximum::Cache) < 0)
			goto fail;
		break;
	fail:
		::close(fd);
		fd = -1;
	}

	freeaddrinfo(aip);
	return fd;
}

} // end namespace
static Connection::CreatorRegistrar unixConnection("unix", dial_unix, announce_unix);
static Connection::CreatorRegistrar tcpConnection("tcp", dial_tcp, announce_tcp);

std::tuple<std::string, std::string>
Connection::decompose(const std::string& address) {
    std::string _address(address);
	if (auto addrPos = _address.find('!'); addrPos == std::string::npos) {
        throw Exception("no address type defined!");
    } else {
        std::string type(_address.substr(0, addrPos));
        return std::make_tuple(_address.substr(0, addrPos), _address.substr(addrPos+1));
	}
}

/**
 * Function: dial
 * Function: announce
 *
 * Params:
 *	address: An address on which to connect or listen,
 *		 specified in the Plan 9 resources
 *		 specification format
 *		 (<protocol>!address[!<port>])
 *
 * These functions hide some of the ugliness of Berkely
 * Sockets. dial connects to the resource at P<address>,
 * while announce begins listening on P<address>.
 *
 * Returns:
 *	These functions return file descriptors on success, and -1
 *	on failure. errbuf(3) may be inspected on failure.
 * See also:
 *	socket(2)
 */
Connection
Connection::dial(const std::string& address) {
    auto [kind, path] = decompose(address);
    if (auto target = getCtab().find(kind); target != getCtab().end()) {
        return Connection(target->second.dial(path));
    } else {
        throw Exception("Given kind '", kind, "' is not a registered connection creator type!");
    }
}

Connection
Connection::announce(const std::string& address) {
    auto [kind, path] = decompose(address);
    if (auto target = getCtab().find(kind); target != getCtab().end()) {
        return Connection(target->second.announce(path));
    } else {
        throw Exception("Given kind '", kind, "' is not a registered connection creator type!");
    }
}

Connection::Connection(int fid) : _fid(fid) { }

ssize_t 
Connection::write(const std::string& msg, size_t count) {
    return concurrency::threadModel->write(_fid, msg.c_str(), count);
}
ssize_t
Connection::write(const std::string& msg) {
    return write(msg, msg.length());
}

ssize_t
Connection::read(std::string& msg, size_t count) {
    msg.reserve(count);
    return concurrency::threadModel->read(_fid, msg.data(), count);
}

ssize_t
Connection::write(char* c, size_t count) {
    return concurrency::threadModel->write(_fid, c, count);
}
ssize_t
Connection::read(char* c, size_t count) {
    return concurrency::threadModel->read(_fid, c, count);
}

bool
Connection::shutdown(int how) {
    return ::shutdown(_fid, how) == 0;
}

bool
Connection::close() {
    return ::close(_fid) == 0;
}

Connection::operator int() const {
    return _fid;
}

std::map<std::string, Connection::Creator>&
Connection::getCtab() noexcept {
    static std::map<std::string, Connection::Creator> _map;
    return _map;
}

void
Connection::registerCreator(const std::string& name, Connection::Action dial, Connection::Action announce) {
    if (auto result = getCtab().emplace(name, Creator(name, dial, announce)); !result.second) {
        throw Exception(name, " already registered as a creator kind!");
    }
}
Connection::Creator::Creator(const std::string& name, Action dial, Action announce) : _name(name), _dial(dial), _announce(announce) { }

int
Connection::Creator::dial(const std::string& address) {
    return _dial(address);
}

int
Connection::Creator::announce(const std::string& address) {
    return _announce(address);
}

Connection::CreatorRegistrar::CreatorRegistrar(const std::string& name, Action dial, Action announce) {
    Connection::registerCreator(name, dial, announce);
}


} // end namespace jyq
