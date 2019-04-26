/* Written by Kris Maglione */
/* Public domain */
#include <cstdlib>
#include "jyq.h"

namespace jyq {
/* Edit s/^([a-zA-Z].*)\n([a-z].*) {/\1 \2;/g  x/^([^a-zA-Z]|static|$)/-+d  s/ (\*map|val|*str)//g */

struct MapEnt {
	ulong		hash;
	const char*	key;
	void*		val;
	MapEnt*		next;
};

MapEnt *NM;

static void
insert(MapEnt **e, ulong val, const char *key) {
	MapEnt *te;
	
	te = (MapEnt*)jyq::emallocz(sizeof *te);
	te->hash = val;
	te->key = key;
	te->next = *e;
	*e = te;
}

static MapEnt**
map_getp(Map *map, ulong val, bool create, bool *exists) {
	MapEnt **e;

	e = &map->bucket[val%map->nhash];
	for(; *e; e = &(*e)->next)
		if((*e)->hash >= val) break;
	if(exists)
		*exists = *e && (*e)->hash == val;

    if (!(*e) || (*e)->hash != val) {
		if(create)
			insert(e, val, nullptr);
		else
			e = &NM;
	}
	return e;
}

void
Map::free(std::function<void(void*)> destroy) {
	MapEnt *e;
    lock.writeLock();
	for(auto i = 0; i < nhash; i++) {
		while((e = bucket[i])) {
			bucket[i] = e->next;
			if(destroy) {
				destroy(e->val);
            }
			::free(e);
		}
    }
    lock.writeUnlock();
    lock.deactivate();
}

void
Map::exec(std::function<void(void*, void*)> run, void* context) {
    concurrency::Locker<RWLock> theLock(lock, true);
	for(auto i = 0; i < nhash; i++) {
		for(auto e = bucket[i]; e; e = e->next) {
			run(context, e->val);
        }
    }
}

void
Map::init(MapEnt **buckets, int nbuckets) {

	bucket = buckets;
	nhash = nbuckets;

	concurrency::threadModel->initrwlock(&lock);
}

bool
Map::insert(ulong key, void *val, bool overwrite) {
	bool existed;
	
	auto res = true;
    concurrency::Locker<RWLock> theLock(lock, false);
	MapEnt* e = *map_getp(this, key, true, &existed);
	if(existed && !overwrite) {
		res = false;
    } else {
		e->val = val;
    }
	return res;
}

void*
Map::get(ulong val) {
    concurrency::Locker<RWLock> theLock(lock, true);
	auto e = *map_getp(this, val, false, nullptr);
	return e ? e->val : nullptr;
}

void*
Map::rm(ulong val) {
	
	void* ret = nullptr;
    lock.writeLock();
	if (MapEnt** e = map_getp(this, val, false, nullptr); *e) {
		auto te = *e;
		ret = te->val;
		*e = te->next;
        lock.writeUnlock();
		::free(te);
	} else {
        lock.writeUnlock();
    }
	return ret;
}

} // end namespace jyq
