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
    template<typename T>
    class DoubleLinkedListNode {
        public:
            using Self = DoubleLinkedListNode<T>;
            using Link = std::shared_ptr<Self>;
        public:
            DoubleLinkedListNode(T value) : _contents(value) { }
            T& getContents() noexcept { return _contents; }
            const T& getContents() const noexcept { return _contents; }
            auto getNext() noexcept { return _next; }
            auto getPrevious() noexcept { return _prev; }
            void setNext(Link value) noexcept { _next = value; }
            void setPrevious(Link value) noexcept { _prev = value; }
            auto hasNext() const noexcept { return _next; }
            auto hasPrevious() const noexcept { return _prev; }
            void clearLinks() noexcept {
                _next.reset();
                _prev.reset();
            }
        private:
            T _contents;
            Link _next;
            Link _prev;


    };
    struct RpcBody {
        public:
            //RpcBody(Client& m);
            RpcBody(Mutex& m);
            ~RpcBody() = default;
            //int sendrpc(Fcall *f);
            //bool sendrpc(Fcall& f); // can't make this const right now since modification to input happens...
            Rendez& getRendez() noexcept { return _r; }
            const Rendez& getRendez() const noexcept { return _r; }
            constexpr auto getTag() const noexcept { return _tag; }
            constexpr auto isWaiting() const noexcept { return _waiting; }
            constexpr auto isAsync() const noexcept { return _async; }
            void setTag(uint value) noexcept { _tag = value; }
            void setWaiting(bool value = true) noexcept { _waiting = value; }
            void setAsync(bool value = true) noexcept { _async = value; }
            void setP(std::shared_ptr<Fcall> value) noexcept { _p = value; }
            auto getP() noexcept { return _p; }
        private:
            Rendez	_r;
            uint    _tag;
            std::shared_ptr<Fcall> _p;
            bool _waiting;
            bool _async;

    };
    using BareRpc = DoubleLinkedListNode<RpcBody>;
    using Rpc = std::shared_ptr<BareRpc>;
} // end namespace jyq
#endif // end LIBJYQ_RPC_H__
