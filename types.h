#ifndef LIBJYQ_TYPES_H__
#define LIBJYQ_TYPES_H__
/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */

#include <cstdint>
#include <cstdbool>
#include <iostream>
#include <sys/types.h>
#include <exception>
#include <sstream>
#include <memory>

namespace jyq {
    using uint = unsigned int;
    using ulong = unsigned long;
    namespace maximum {
        constexpr auto Version = 32;
        constexpr auto Msg = 8192;
        constexpr auto Error = 128;
        constexpr auto Cache = 32;
        constexpr auto Flen = 128;
        constexpr auto Ulen = 32;
        constexpr auto Welem = 16;
    } // end namespace maximum

    constexpr auto ErrorMax = maximum::Error;

    constexpr auto ApiVersion = 135;
    constexpr auto Version = "9P2000";
    constexpr auto NoTag = uint16_t(~0); 
    constexpr auto NoFid = ~0u;

    template<typename ... Args>
    void print(std::ostream& os, Args&& ... args) {
        (os << ... << args);
    }


    template<typename T>
    constexpr T min(T a, T b) noexcept {
        return a < b ? a : b;
    }
    // sanity checks
    static_assert(min(1,2) == 1);
    static_assert(min(2,1) == 1);

    template<typename T>
    constexpr T max(T a, T b) noexcept {
        return a > b ? a : b;
    }
    static_assert(max(1,2) == 2);
    static_assert(max(2,1) == 2);

    template<typename T>
    struct ContainsSizeParameter {
        ContainsSizeParameter(T size) : _value(size) { }
        ContainsSizeParameter() = default;
        ~ContainsSizeParameter() = default;
        constexpr T size() const noexcept { return _value; }
        constexpr bool empty() const noexcept { return size() == 0; }
        T& getSizeReference() noexcept { return _value; }
        void setSize(T value) noexcept { _value = value; }
        private:
            T _value;
    };
    /* 9P message types */
    enum class FType : uint8_t {
        TVersion = 100,
        RVersion,
        TAuth = 102,
        RAuth,
        TAttach = 104,
        RAttach,
        TError = 106, /* illegal */
        RError,
        TFlush = 108,
        RFlush,
        TWalk = 110,
        RWalk,
        TOpen = 112,
        ROpen,
        TCreate = 114,
        RCreate,
        TRead = 116,
        RRead,
        TWrite = 118,
        RWrite,
        TClunk = 120,
        RClunk,
        TRemove = 122,
        RRemove,
        TStat = 124,
        RStat,
        TWStat = 126,
        RWStat,
    };

    /* from libc.h in p9p */
    enum class OMode : uint16_t {
        READ	= 0,	/* open for read */
        WRITE	= 1,	/* write */
        RDWR	= 2,	/* read and write */
        EXEC	= 3,	/* execute, == read but check execute permission */
        TRUNC	= 16,	/* or'ed in (except for exec), truncate file first */
        CEXEC	= 32,	/* or'ed in, close on exec */
        RCLOSE	= 64,	/* or'ed in, remove on close */
        DIRECT	= 128,	/* or'ed in, direct access */
        NONBLOCK	= 256,	/* or'ed in, non-blocking call */
        EXCL	= 0x1000,	/* or'ed in, exclusive use (create only) */
        LOCK	= 0x2000,	/* or'ed in, lock after opening */
        APPEND	= 0x4000	/* or'ed in, append only */
    };

    /* bits in Qid.type */
    enum class QType : uint8_t {
        DIR	= 0x80,	/* type bit for directories */
        APPEND	= 0x40,	/* type bit for append only files */
        EXCL	= 0x20,	/* type bit for exclusive use files */
        MOUNT	= 0x10,	/* type bit for mounted channel */
        AUTH	= 0x08,	/* type bit for authentication file */
        TMP	= 0x04,	/* type bit for non-backed-up file */
        SYMLINK	= 0x02,	/* type bit for symbolic link */
        FILE	= 0x00	/* type bits for plain file */
    };

    /* bits in Stat.mode */
    enum class DMode : uint32_t {
        EXEC	= 0x1,		/* mode bit for execute permission */
        WRITE	= 0x2,		/* mode bit for write permission */
        READ	= 0x4,		/* mode bit for read permission */

        // these were originally macros in this exact location... no clue
        // why...
        DIR = 0x8000'0000, /* mode bit for directories */
        APPEND = 0x4000'0000, /* mode bit for append only files */
        EXCL = 0x2000'0000, /* mode bit for exclusive use files */
        MOUNT = 0x1000'0000, /* mode bit for mounted channel */
        AUTH = 0x0800'0000, /* mode bit for authentication file */
        TMP = 0x0400'0000, /* mode bit for non-backed-up file */
        SYMLINK = 0x0200'0000, /* mode bit for symbolic link (Unix, 9P2000.u) */
        DEVICE = 0x0080'0000, /* mode bit for device file (Unix, 9P2000.u) */
        NAMEDPIPE = 0x0020'0000, /* mode bit for named pipe (Unix, 9P2000.u) */
        SOCKET = 0x0010'0000, /* mode bit for socket (Unix, 9P2000.u) */
        SETUID = 0x0008'0000, /* mode bit for setuid (Unix, 9P2000.u) */
        SETGID = 0x0004'0000, /* mode bit for setgid (Unix, 9P2000.u) */
    };

    class Exception : public std::exception {
        public:
            template<typename ... Args>
            explicit Exception(Args&& ... args) noexcept {
                std::ostringstream os;
                print(os, args...);
                _message = os.str();
            }
            virtual ~Exception() noexcept;
            std::string message() const noexcept { return _message; }
            virtual const char* what() const noexcept final;
            Exception& operator=(const Exception&) = delete;
        private:
            std::string _message;
    };
    template<typename T>
    class SingleLinkedListNode {
        public:
            using Self = SingleLinkedListNode<T>;
            using Link = std::shared_ptr<Self>;
        public:
            SingleLinkedListNode() = default;
            SingleLinkedListNode(T value) : _contents(value) { }
            T& getContents() noexcept { return _contents; }
            const T& getContents() const noexcept { return _contents; }
            auto getNext() noexcept { return _next; }
            void setNext(Link value) noexcept { _next = value; }
            auto hasNext() const noexcept { return _next; }
            void clearLinks() noexcept {
                _next.reset();
            }
        private:
            T _contents;
            Link _next;

    };
    template<typename T>
    class DoubleLinkedListNode {
        public:
            using Self = DoubleLinkedListNode<T>;
            using Link = std::shared_ptr<Self>;
        public:
            static void circularLink(Link link) {
                link->_next = link;
                link->_prev = link;
            }
        public:
            DoubleLinkedListNode() = default;
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
            void unlink() noexcept {
                if (_prev) {
                    _prev->setNext(_next);
                }
                if (_next) {
                    _next->setPrevious(_prev);
                }
                clearLinks();
            }
        private:
            T _contents;
            Link _next;
            Link _prev;
    };

} // end namespace jyq

#endif // end LIBJYQ_TYPES_H__
