/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_SOCKET_H__
#define LIBJYQ_SOCKET_H__
#include <string>
#include "types.h"
#include "Msg.h"

namespace jyq {
    int dial(const std::string&);
    int announce(const std::string&);
    uint sendmsg(int, Msg*);
    uint recvmsg(int, Msg*);
} // end namespace jyq
#endif // end LIBJYQ_SOCKET_H__
