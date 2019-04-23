/* Written by Kris Maglione */
/* Public domain */
#include <cstdlib>
#include "ixp.h"

namespace ixp {
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
	
	te = (MapEnt*)ixp::emallocz(sizeof *te);
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
	int i;
	MapEnt *e;

	concurrency::threadModel->wlock(&lock);
	for(i=0; i < nhash; i++)
		while((e = bucket[i])) {
			bucket[i] = e->next;
			if(destroy)
				destroy(e->val);
			::free(e);
		}
	concurrency::threadModel->wunlock(&lock);
	concurrency::threadModel->rwdestroy(&lock);
}

void
Map::exec(std::function<void(void*, void*)> run, void* context) {
	int i;
	MapEnt *e;

	concurrency::threadModel->rlock(&lock);
	for(i=0; i < nhash; i++)
		for(e=bucket[i]; e; e=e->next)
			run(context, e->val);
	concurrency::threadModel->runlock(&lock);
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
	concurrency::threadModel->wlock(&lock);
	MapEnt* e = *map_getp(this, key, true, &existed);
	if(existed && !overwrite) {
		res = false;
    } else {
		e->val = val;
    }
	concurrency::threadModel->wunlock(&lock);
	return res;
}

void*
Map::get(ulong val) {
	MapEnt *e;
	void *res;
	
	concurrency::threadModel->rlock(&lock);
	e = *map_getp(this, val, false, nullptr);
	res = e ? e->val : nullptr;
	concurrency::threadModel->runlock(&lock);
	return res;
}

void*
Map::rm(ulong val) {
	MapEnt **e, *te;
	void *ret;
	
	ret = nullptr;
	concurrency::threadModel->wlock(&lock);
	e = map_getp(this, val, false, nullptr);
	if(*e) {
		te = *e;
		ret = te->val;
		*e = te->next;
		concurrency::threadModel->wunlock(&lock);
		::free(te);
	}
	else
		concurrency::threadModel->wunlock(&lock);
	return ret;
}

} // end namespace ixp
