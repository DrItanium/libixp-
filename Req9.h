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
        Fcall& getIFcall() noexcept { return _ifcall; }
        const Fcall& getIFcall() const noexcept { return _ifcall; }
        Fcall& getOFcall() noexcept { return _ofcall; }
        const Fcall& getOFcall() const noexcept { return _ofcall; }
        void setIFcall(const Fcall& value) { _ifcall = value; }
        void setOFcall(const Fcall& value) { _ofcall = value; }
        std::any& getAux() noexcept { return _aux; }
        const std::any& getAux() const noexcept { return _aux; }
        void setAux(const std::any& value) { _aux = value; }
        private:
            Fcall	_ifcall; /* The incoming request fcall. */
            Fcall	_ofcall; /* The response fcall, to be filled by handler. */
            std::any    _aux; // Arbitrary pointer, to be used by handlers. 
        public:

        /* Private members */
        Conn9 *conn;
        // methods
        void respond(const char *err);
        inline void respond(const std::string& err) { respond(err.c_str()); }
        void handle();

    };
} // end namespace jyq
#endif // end LIBJYQ_REQ9_H__
