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
        public:
            Rpc(Client& m);
            ~Rpc() = default;
            Client&	mux;
            Rpc*		next;
            Rpc*		prev;
            Rendez	r;
            std::shared_ptr<Fcall> p;
            int sendrpc(Fcall *f);
            bool sendrpc(Fcall& f); // can't make this const right now since modification to input happens...
            Rendez& getRendez() noexcept { return r; }
            Client& getMux() noexcept { return mux; }
            constexpr auto getTag() const noexcept { return tag; }
            constexpr auto isWaiting() const noexcept { return waiting; }
            constexpr auto isAsync() const noexcept { return async; }
            void setTag(uint value) noexcept { tag = value; }
            void setWaiting(bool value = true) noexcept { waiting = value; }
            void setAsync(bool value = true) noexcept { async = value; }
        private:
            uint		tag;
            bool waiting;
            bool async;

    };
} // end namespace jyq
#endif // end LIBJYQ_RPC_H__
