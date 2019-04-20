/* From Plan 9's libmux.
 * Copyright (c) 2003 Russ Cox, Massachusetts Institute of Technology
 * Distributed under the same terms as libixp.
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ixp_local.h"

namespace ixp {
namespace {
void
initrpc(Client *mux, Rpc *r)
{
	r->mux = mux;
	r->waiting = 1;
	r->r.mutex = (decltype(r->r.mutex))&mux->lk;
	r->p = nullptr;
	concurrency::threadModel->initrendez(&r->r);
}

void
freemuxrpc(Rpc *r)
{
	concurrency::threadModel->rdestroy(&r->r);
}

Fcall*
muxrecv(Client *mux)
{
	Fcall *f = nullptr;
	concurrency::threadModel->lock(&mux->rlock);
	if(recvmsg(mux->fd, &mux->rmsg) == 0)
		goto fail;
	f = (decltype(f))ixp::emallocz(sizeof *f);
	if(msg2fcall(&mux->rmsg, f) == 0) {
		free(f);
		f = nullptr;
	}
fail:
	concurrency::threadModel->unlock(&mux->rlock);
	return f;
}


void
electmuxer(Client *mux)
{
	Rpc *rpc;

	/* if there is anyone else sleeping, wake them to mux */
	for(rpc=mux->sleep.next; rpc != &mux->sleep; rpc = rpc->next){
		if(!rpc->async){
			mux->muxer = rpc;
			concurrency::threadModel->wake(&rpc->r);
			return;
		}
	}
	mux->muxer = nullptr;
}
void
enqueue(Client *mux, Rpc *r)
{
	r->next = mux->sleep.next;
	r->prev = &mux->sleep;
	r->next->prev = r;
	r->prev->next = r;
}

void
dequeue(Client *, Rpc *r)
{
	r->next->prev = r->prev;
	r->prev->next = r->next;
	r->prev = nullptr;
	r->next = nullptr;
}

int 
gettag(Client *mux, Rpc *r)
{
	int i, mw;
	Rpc **w;

	for(;;){
		/* wait for a free tag */
		while(mux->nwait == mux->mwait){
			if(mux->mwait < mux->maxtag-mux->mintag){
				mw = mux->mwait;
				if(mw == 0)
					mw = 1;
				else
					mw <<= 1;
				w = (decltype(w))realloc(mux->wait, mw * sizeof *w);
                if (!w)
					return -1;
				memset(w+mux->mwait, 0, (mw-mux->mwait) * sizeof *w);
				mux->wait = w;
				mux->freetag = mux->mwait;
				mux->mwait = mw;
				break;
			}
			concurrency::threadModel->sleep(&mux->tagrend);
		}

		i=mux->freetag;
		if(mux->wait[i] == 0)
			goto Found;
		for(; i<mux->mwait; i++)
			if(mux->wait[i] == 0)
				goto Found;
		for(i=0; i<mux->freetag; i++)
			if(mux->wait[i] == 0)
				goto Found;
		/* should not fall out of while without free tag */
		abort();
	}

Found:
	mux->nwait++;
	mux->wait[i] = r;
	r->tag = i+mux->mintag;
	return r->tag;
}

void
puttag(Client *mux, Rpc *r)
{
	auto i = r->tag - mux->mintag;
	assert(mux->wait[i] == r);
	mux->wait[i] = nullptr;
	mux->nwait--;
	mux->freetag = i;
	concurrency::threadModel->wake(&mux->tagrend);
	freemuxrpc(r);
}
int
sendrpc(Rpc *r, Fcall *f)
{
	auto ret = 0;
	auto mux = r->mux;
	/* assign the tag, add selves to response queue */
	concurrency::threadModel->lock(&mux->lk);
	r->tag = gettag(mux, r);
	f->hdr.tag = r->tag;
	enqueue(mux, r);
	concurrency::threadModel->unlock(&mux->lk);

	concurrency::threadModel->lock(&mux->wlock);
	if(!fcall2msg(&mux->wmsg, f) || !sendmsg(mux->fd, &mux->wmsg)) {
		/* werrstr("settag/send tag %d: %r", tag); fprint(2, "%r\n"); */
		concurrency::threadModel->lock(&mux->lk);
		dequeue(mux, r);
		puttag(mux, r);
		concurrency::threadModel->unlock(&mux->lk);
		ret = -1;
	}
	concurrency::threadModel->unlock(&mux->wlock);
	return ret;
}
void
dispatchandqlock(Client *mux, Fcall *f)
{
	int tag;
	Rpc *r2;

	tag = f->hdr.tag - mux->mintag;
	concurrency::threadModel->lock(&mux->lk);
	/* hand packet to correct sleeper */
	if(tag < 0 || tag >= mux->mwait) {
		fprintf(stderr, "libixp: received unfeasible tag: %d (min: %d, max: %d)\n", f->hdr.tag, mux->mintag, mux->mintag+mux->mwait);
		goto fail;
	}
	r2 = mux->wait[tag];
    if (!r2 || !(r2->prev)) {
		fprintf(stderr, "libixp: received message with bad tag\n");
		goto fail;
	}
	r2->p = f;
	dequeue(mux, r2);
	concurrency::threadModel->wake(&r2->r);
	return;
fail:
	freefcall(f);
	free(f);
}
} // end namespace

void
muxinit(Client *mux)
{
	mux->tagrend.mutex = &mux->lk;
	mux->sleep.next = &mux->sleep;
	mux->sleep.prev = &mux->sleep;
	concurrency::threadModel->initmutex(&mux->lk);
	concurrency::threadModel->initmutex(&mux->rlock);
	concurrency::threadModel->initmutex(&mux->wlock);
	concurrency::threadModel->initrendez(&mux->tagrend);
}

void
muxfree(Client *mux)
{
	concurrency::threadModel->mdestroy(&mux->lk);
	concurrency::threadModel->mdestroy(&mux->rlock);
	concurrency::threadModel->mdestroy(&mux->wlock);
	concurrency::threadModel->rdestroy(&mux->tagrend);
	free(mux->wait);
}


Fcall*
muxrpc(Client *mux, Fcall *tx)
{
	Rpc r;
	Fcall *p;

	initrpc(mux, &r);
	if(sendrpc(&r, tx) < 0)
		return nullptr;

	concurrency::threadModel->lock(&mux->lk);
	/* wait for our packet */
	while(mux->muxer && mux->muxer != &r && !r.p)
		concurrency::threadModel->sleep(&r.r);

	/* if not done, there's no muxer; start muxing */
	if(!r.p){
		assert(mux->muxer == nullptr || mux->muxer == &r);
		mux->muxer = &r;
		while(!r.p){
			concurrency::threadModel->unlock(&mux->lk);
			p = muxrecv(mux);
            if (!p) {
				/* eof -- just give up and pass the buck */
				concurrency::threadModel->lock(&mux->lk);
				dequeue(mux, &r);
				break;
			}
			dispatchandqlock(mux, p);
		}
		electmuxer(mux);
	}
	p = r.p;
	puttag(mux, &r);
	concurrency::threadModel->unlock(&mux->lk);
    if (!p)
		werrstr("unexpected eof");
	return p;
}
} // end namespace ixp


