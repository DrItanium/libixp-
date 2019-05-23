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
#include "Fcall.h"

namespace jyq {
    struct RpcBody {
        public:
            RpcBody();
            ~RpcBody() = default;
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
            mutable Rendez	_r;
            uint    _tag;
            std::shared_ptr<Fcall> _p;
            bool _waiting;
            bool _async;

    };
    using BareRpc = DoubleLinkedListNode<RpcBody>;
    using Rpc = std::shared_ptr<BareRpc>;
} // end namespace jyq
#endif // end LIBJYQ_RPC_H__
