/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_QID_H__
#define LIBJYQ_QID_H__
#include "types.h"
namespace jyq {
    struct Msg;
    struct Qid {
        uint8_t		type;
        uint32_t	version;
        uint64_t	path;
        /* Private members */
        uint8_t		dir_type;
        void packUnpack(Msg& msg);
    };
} // end namespace jyq
#endif // end LIBJYQ_QID_H__
