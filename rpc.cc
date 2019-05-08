/* From Plan 9's libmux.
 * Copyright (c) 2003 Russ Cox, Massachusetts Institute of Technology
 * Distributed under the same terms as libjyq.
 */
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "Rpc.h"
#include "Msg.h"
#include "thread.h"
#include "Client.h"
#include "socket.h"
#include "util.h"
#include "PrintFunctions.h"

namespace jyq {
Rpc::Rpc(Client& m) : mux(m) {
    waiting = true;
    r.mutex = &m.lk;
    p = nullptr;
}
namespace {

Fcall*
muxrecv(Client *mux)
{
	//Fcall *f = nullptr;
    mux->rlock.lock();
    if (mux->fd.recvmsg(mux->rmsg) == 0) {
        mux->rlock.unlock();
        return nullptr;
    }
    auto f = new Fcall();
	//f = (decltype(f))jyq::emallocz(sizeof *f);
	if(msg2fcall(&mux->rmsg, f) == 0) {
        delete f;
		f = nullptr;
	}
    mux->rlock.unlock();
	return f;
}


void
electmuxer(Client *mux)
{
	/* if there is anyone else sleeping, wake them to mux */
	for(auto rpc=mux->sleep.next; rpc != &mux->sleep; rpc = rpc->next){
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
    mux->enqueue(r);
}

void
dequeue(Client * c, Rpc *r)
{
    c->dequeue(r);
}

int 
gettag(Client *mux, Rpc *r)
{
	int i, mw;
	Rpc **w;
    auto Found = [mux, r](auto index) {
        mux->nwait++;
        mux->wait[index] = r;
        r->tag = index+mux->mintag;
        return r->tag;
    };
	for(;;){
		/* wait for a free tag */
		while(mux->nwait == mux->mwait){
			if(mux->mwait < mux->maxtag-mux->mintag){
				mw = mux->mwait;
				if(mw == 0) {
					mw = 1;
                } else {
					mw <<= 1;
                }
				w = (decltype(w))realloc(mux->wait, mw * sizeof *w);
                if (!w) {
					return -1;
                }
				memset(w+mux->mwait, 0, (mw-mux->mwait) * sizeof *w);
				mux->wait = w;
				mux->freetag = mux->mwait;
				mux->mwait = mw;
				break;
			}
            mux->tagrend.sleep();
		}

		i=mux->freetag;
		if(mux->wait[i] == 0)
            return Found(i);
		for(; i<mux->mwait; i++)
			if(mux->wait[i] == 0)
                return Found(i);
		for(i=0; i<mux->freetag; i++)
			if(mux->wait[i] == 0)
                return Found(i);
		/* should not fall out of while without free tag */
        throw "Fell out of loop without free tag!";
	}

}

void
puttag(Client *mux, Rpc *r)
{
	auto i = r->tag - mux->mintag;
	assert(mux->wait[i] == r);
	mux->wait[i] = nullptr;
	mux->nwait--;
	mux->freetag = i;
    mux->tagrend.wake();
    r->r.deactivate();
}
int
sendrpc(Rpc *r, Fcall *f)
{
	auto ret = 0;
	auto mux = &(r->mux);
	/* assign the tag, add selves to response queue */
    {
        concurrency::Locker<Mutex> lk(mux->lk);
        r->tag = gettag(mux, r);
        f->hdr.tag = r->tag;
        enqueue(mux, r);
    }

    {
        concurrency::Locker<Mutex> a(mux->wlock);
        if(!fcall2msg(&mux->wmsg, f) || !mux->fd.sendmsg(mux->wmsg)) {
            /* werrstr("settag/send tag %d: %r", tag); fprint(2, "%r\n"); */
            concurrency::Locker<Mutex> lk(mux->lk);
            dequeue(mux, r);
            puttag(mux, r);
            ret = -1;
        }
    }
    return ret;
}
void
dispatchandqlock(Client *mux, Fcall *f)
{
	int tag = f->hdr.tag - mux->mintag;
    mux->lk.lock();
	/* hand packet to correct sleeper */
	if(tag < 0 || tag >= mux->mwait) {
		fprintf(stderr, "libjyq: received unfeasible tag: %d (min: %d, max: %d)\n", f->hdr.tag, mux->mintag, mux->mintag+mux->mwait);
        Fcall::free(f);
        delete f;
        return;
	}
	auto r2 = mux->wait[tag];
    if (!r2 || !(r2->prev)) {
		fprintf(stderr, "libjyq: received message with bad tag\n");
        Fcall::free(f);
        delete f;
        return;
	}
	r2->p = f;
	dequeue(mux, r2);
    r2->r.wake();
	return;
}
} // end namespace
void
Client::enqueue(Rpc* r) {
	r->next = sleep.next;
	r->prev = &sleep;
	r->next->prev = r;
	r->prev->next = r;
}

void
Client::dequeue(Rpc* r) {
	r->next->prev = r->prev;
	r->prev->next = r->next;
	r->prev = nullptr;
	r->next = nullptr;

}

void
Client::muxinit()
{
	tagrend.mutex = &lk;
	sleep.next = &sleep;
	sleep.prev = &sleep;
}

Fcall*
Client::muxrpc(Fcall *tx)
{
	Rpc r(*this);
	Fcall *p;

	if(sendrpc(&r, tx) < 0)
		return nullptr;

    lk.lock();
	/* wait for our packet */
	while(muxer && muxer != &r && !r.p) {
        r.r.sleep();
    }

	/* if not done, there's no muxer; start muxing */
	if(!r.p){
		assert(muxer == nullptr || muxer == &r);
		muxer = &r;
		while(!r.p){
            lk.unlock();
			p = muxrecv(this);
            if (!p) {
				/* eof -- just give up and pass the buck */
                lk.lock();
                dequeue(&r);
				break;
			}
			dispatchandqlock(this, p);
		}
		electmuxer(this);
	}
	p = r.p;
	puttag(this, &r);
    lk.unlock();
    if (!p) {
        wErrorString("unexpected eof");
    }
	return p;
}
} // end namespace jyq


