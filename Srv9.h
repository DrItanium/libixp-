/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_SRV9_H__
#define LIBJYQ_SRV9_H__
#include <any>
#include <functional>
#include "types.h"
#include "Conn9.h"
#include "Req9.h"
#include "Fid.h"

namespace jyq {
    struct Srv9 {
        std::any aux;
        std::function<void(Req9*)> attach,
                clunk,
                create,
                flush,
                open,
                read,
                remove,
                stat,
                walk,
                write,
                wstat;
        std::function<void(Fid*)> freefid;
    };
} // end namespace jyq
#endif // end LIBJYQ_SRV9_H__
