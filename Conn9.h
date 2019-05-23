/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_CONN9_H__
#define LIBJYQ_CONN9_H__
#include <string>
#include <functional>
#include <list>
#include <memory>
#include <any>
#include "types.h"
#include "qid.h"
#include "Fcall.h"
#include "stat.h"
#include "map.h"
#include "Conn.h"
#include "Srv9.h"
#include "Fid.h"
#include "Req9.h"

namespace jyq {
struct Srv9;
struct Conn9 {
    public:
        using TagMap = jyq::Map<uint16_t, Req9>;
    public:
        Conn9() = default;
        ~Conn9() = default;
        Fid::Map& getFidMap() noexcept { return _fidmap; }
        TagMap& getTagMap() noexcept { return _tagmap; }
        auto getSrv() noexcept { return _srv; }
        void setSrv(Srv9* value) { _srv = value; }
        auto getConn() noexcept { return _conn; }
        void setConn(Conn* value) noexcept { _conn = value; }
        Msg& getRMsg() noexcept { return _rmsg; }
        Msg& getWMsg() noexcept { return _wmsg; }
        Mutex& getRLock() noexcept { return _rlock; }
        Mutex& getWLock() noexcept { return _wlock; }
        void alloc(uint n);
        constexpr auto getReferenceCount() const noexcept { return _ref; }
        constexpr auto referenceCountGreaterThan(int count) const noexcept { return _ref > count; }
        constexpr auto referenceCountIs(int count) const noexcept { return _ref == count; }
        void incrementReferenceCount() noexcept { ++_ref; }
        void decrementReferenceCount() noexcept { --_ref; }
        Req9* retrieveTag(uint16_t id);
        Fid* retrieveFid(int id);
        bool removeTag(uint16_t id);
        bool removeFid(int id);
        template<typename T>
            void tagExec(std::function<void(T, TagMap::iterator)> op, T context) {
                _tagmap.exec<T>(op, context);
            }
        template<typename T>
            void fidExec(std::function<void(T, Fid::Map::iterator)> op, T context) {
                _fidmap.exec<T>(op, context);
            }

        /**
         * Increment the reference count
         */
        Conn9& operator++() {
            incrementReferenceCount();
            return *this;
        }
        /**
         * decrement the reference count
         */
        Conn9& operator--() {
            decrementReferenceCount();
            return *this;
        }

    private:
        TagMap    _tagmap;
        Fid::Map  _fidmap;
        Srv9*   _srv;
        Conn*	_conn;
        Mutex	_rlock;
        Mutex	_wlock;
        Msg		_rmsg;
        Msg		_wmsg;
        int _ref;
};
} // end namespace jyq
#endif // end LIBJYQ_CONN9_H__
