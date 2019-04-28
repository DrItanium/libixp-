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
#include "thread.h"
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
constexpr auto TAG_BUCKETS = 61;
constexpr auto FID_BUCKETS = 61;
struct Conn9 {
    using TagMap = std::map<uint16_t, Req9>;
    TagMap    tagmap;
    Fid::Map  fidmap;
	Srv9*	srv;
	Conn*	conn;
	Mutex	rlock;
	Mutex	wlock;
	Msg		rmsg;
	Msg		wmsg;
	int		ref;
};
} // end namespace jyq
#endif // end LIBJYQ_CONN9_H__
