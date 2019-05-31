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
#include "qid.h"
#include "Fcall.h"
#include "stat.h"
#include "map.h"
#include "Fid.h"

namespace jyq {
    struct Srv9;
    struct Conn9;
    struct Req9 : public HasAux {
        Fcall& getIFcall() noexcept { return _ifcall; }
        const Fcall& getIFcall() const noexcept { return _ifcall; }
        Fcall& getOFcall() noexcept { return _ofcall; }
        const Fcall& getOFcall() const noexcept { return _ofcall; }
        void setIFcall(const Fcall& value) { _ifcall = value; }
        void setOFcall(const Fcall& value) { _ofcall = value; }
        std::shared_ptr<Conn9> getConn() noexcept { return _conn; }
        void setConn(std::shared_ptr<Conn9> value) noexcept { _conn = value; }
        // methods
        void respond(const char *err);
        inline void respond(const std::string& err) { respond(err.c_str()); }
        void handle();
        void setSrv(Srv9* value) noexcept { _srv = value; }
        auto getSrv() noexcept { return _srv; }
        void setFid(Fid* value) noexcept { _fid = value; }
        auto getFid() noexcept { return _fid; }
        private:
            Srv9*	_srv;
            Fid*	_fid;    /* Fid structure corresponding to FHdr.fid */
        public:
            Fid*	newfid; /* Corresponds to FTWStat.newfid */
            Req9*	oldreq; /* For TFlush requests, the original request. */
        private:
            Fcall	_ifcall; /* The incoming request fcall. */
            Fcall	_ofcall; /* The response fcall, to be filled by handler. */
            std::shared_ptr<Conn9>  _conn;


    };
} // end namespace jyq
#endif // end LIBJYQ_REQ9_H__
