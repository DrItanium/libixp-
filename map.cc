/* Written by Kris Maglione */
/* Public domain */
#include <stdlib.h>
#include "ixp_local.h"

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
mapfree(Map *map, std::function<void(void*)> destroy) {
	int i;
	MapEnt *e;

	concurrency::threadModel->wlock(&map->lock);
	for(i=0; i < map->nhash; i++)
		while((e = map->bucket[i])) {
			map->bucket[i] = e->next;
			if(destroy)
				destroy(e->val);
			free(e);
		}
	concurrency::threadModel->wunlock(&map->lock);
	concurrency::threadModel->rwdestroy(&map->lock);
}

void
mapexec(Map *map, std::function<void(void*, void*)> run, void* context) {
	int i;
	MapEnt *e;

	concurrency::threadModel->rlock(&map->lock);
	for(i=0; i < map->nhash; i++)
		for(e=map->bucket[i]; e; e=e->next)
			run(context, e->val);
	concurrency::threadModel->runlock(&map->lock);
}

void
mapinit(Map *map, MapEnt **buckets, int nbuckets) {

	map->bucket = buckets;
	map->nhash = nbuckets;

	concurrency::threadModel->initrwlock(&map->lock);
}

bool
mapinsert(Map *map, ulong key, void *val, bool overwrite) {
	MapEnt *e;
	bool existed, res;
	
	res = true;
	concurrency::threadModel->wlock(&map->lock);
	e = *map_getp(map, key, true, &existed);
	if(existed && !overwrite)
		res = false;
	else
		e->val = val;
	concurrency::threadModel->wunlock(&map->lock);
	return res;
}

void*
mapget(Map *map, ulong val) {
	MapEnt *e;
	void *res;
	
	concurrency::threadModel->rlock(&map->lock);
	e = *map_getp(map, val, false, nullptr);
	res = e ? e->val : nullptr;
	concurrency::threadModel->runlock(&map->lock);
	return res;
}

void*
maprm(Map *map, ulong val) {
	MapEnt **e, *te;
	void *ret;
	
	ret = nullptr;
	concurrency::threadModel->wlock(&map->lock);
	e = map_getp(map, val, false, nullptr);
	if(*e) {
		te = *e;
		ret = te->val;
		*e = te->next;
		concurrency::threadModel->wunlock(&map->lock);
		free(te);
	}
	else
		concurrency::threadModel->wunlock(&map->lock);
	return ret;
}

} // end namespace ixp
