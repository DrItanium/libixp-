/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_FID_H__
#define LIBJYQ_FID_H__
#include <string>
#include <functional>
#include <list>
#include <memory>
#include <any>
#include "types.h"
#include "thread.h"
#include "qid.h"
#include "Fcall.h"
#include "stat.h"
#include "map.h"

namespace jyq {
    struct Conn9;
    /**
     * Type: Fid
     *
     * Represents an open file for a 9P connection. The same
     * structure persists as long as the file remains open, and is
     * installed in the T<Req9> structure for any request Fcall
     * which references it. Handlers may use the P<aux> member to
     * store any data which must persist for the life of the open
     * file.
     *
     * See also:
     *	T<Req9>, T<Qid>, T<OMode>
     */
    struct Fid {
        public:
            using Map = jyq::Map<int, Fid>;
            Fid(uint32_t fid, Conn9& c);
            ~Fid();
        public:
            std::string		uid;	/* The uid of the file opener. */
            std::any    aux;    // Arbitrary pointer, to be used by handlers. 
            uint32_t		fid;    /* The ID number of the fid. */
            Qid		qid;    /* The filesystem-unique QID of the file. */
            signed char	omode;  /* The open mode of the file. */
            uint		iounit; /* The maximum size of any IO request. */
        public:
            Conn9& getConn() noexcept { return _conn; }
            const Conn9& getConn() const noexcept { return _conn; }
        private:
            Conn9& _conn;
    };
} // end namespace jyq
#endif // end LIBJYQ_FID_H__
