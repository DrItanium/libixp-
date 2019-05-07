/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_REQ9_H__
#define LIBJYQ_REQ9_H__
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
#include "Fid.h"

namespace jyq {
    struct Srv9;
    struct Conn9;
    struct Req9 {
        Srv9*	srv;
        Fid*	fid;    /* Fid structure corresponding to FHdr.fid */
        Fid*	newfid; /* Corresponds to FTWStat.newfid */
        Req9*	oldreq; /* For TFlush requests, the original request. */
        Fcall	ifcall; /* The incoming request fcall. */
        Fcall	ofcall; /* The response fcall, to be filled by handler. */
        std::any    aux; // Arbitrary pointer, to be used by handlers. 

        /* Private members */
        Conn9 *conn;
        // methods
        void respond(const char *err);
        inline void respond(const std::string& err) { respond(err.c_str()); }
    };
} // end namespace jyq
#endif // end LIBJYQ_REQ9_H__
