/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_RPC_H__
#define LIBJYQ_RPC_H__
#include <any>
#include <functional>
#include <list>
#include <memory>
#include "types.h"
#include "thread.h"
#include "Fcall.h"

namespace jyq {
    struct Client;
    struct Rpc {
        Rpc(Client& m);
        ~Rpc() = default;
        Client&	mux;
        Rpc*		next;
        Rpc*		prev;
        Rendez	r;
        uint		tag;
        Fcall*	p;
        bool waiting;
        bool async;
    };
} // end namespace jyq
#endif // end LIBJYQ_RPC_H__
