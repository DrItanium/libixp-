/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_CFID_H__
#define LIBJYQ_CFID_H__
#include <string>
#include <functional>
#include <list>
#include <memory>
#include "types.h"
#include "thread.h"
#include "qid.h"
#include "Fcall.h"
#include "stat.h"

namespace jyq {
    struct CFid {
        uint32_t fid;
        Qid      qid;
        uint8_t  mode;
        uint     open;
        uint     iounit;
        uint32_t offset;

        /* Private members */
        Mutex iolock;
        bool close(DoFcallFunc);
        bool clunk(DoFcallFunc); 
        bool performClunk(DoFcallFunc);
        long read(void*, long, DoFcallFunc);
        long pread(void*, long, int64_t, DoFcallFunc);
        long pwrite(const void*, long, int64_t, DoFcallFunc);
        long write(const void*, long, DoFcallFunc);
        std::shared_ptr<Stat> fstat(DoFcallFunc);
    };
} // end namespace jyq
#endif // end LIBJYQ_CFID_H__
