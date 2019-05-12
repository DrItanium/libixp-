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
            int sendrpc(Fcall *f);
            bool sendrpc(Fcall& f); // can't make this const right now since modification to input happens...
            auto getNext() noexcept { return _next; }
            auto getPrevious() noexcept { return _prev; }
            void setNext(Rpc* next) noexcept { _next = next; }
            void setPrevious(Rpc* prev) noexcept { _prev = prev; }
            Rendez& getRendez() noexcept { return _r; }
            const Rendez& getRendez() const noexcept { return _r; }
            Client& getMux() noexcept { return _mux; }
            const Client& getMux() const noexcept { return _mux; }
            constexpr auto getTag() const noexcept { return _tag; }
            constexpr auto isWaiting() const noexcept { return _waiting; }
            constexpr auto isAsync() const noexcept { return _async; }
            void setTag(uint value) noexcept { _tag = value; }
            void setWaiting(bool value = true) noexcept { _waiting = value; }
            void setAsync(bool value = true) noexcept { _async = value; }
            void dequeueSelf();
            void enqueueSelf(Rpc& other);
            void setP(std::shared_ptr<Fcall> value) noexcept { _p = value; }
            auto getP() noexcept { return _p; }
        private:
            Client& _mux;
            Rpc*    _next;
            Rpc*    _prev;
            Rendez	_r;
            uint    _tag;
            std::shared_ptr<Fcall> _p;
            bool _waiting;
            bool _async;

    };
} // end namespace jyq
#endif // end LIBJYQ_RPC_H__
