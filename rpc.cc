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

Fcall*
Client::muxrecv()
{
	//Fcall *f = nullptr;
    concurrency::Locker<Mutex> theRlock(rlock);
    if (fd.recvmsg(_rmsg) == 0) {
        return nullptr;
    }
	if(auto f = new Fcall(); msg2fcall(&_rmsg, f) == 0) {
        delete f;
        return nullptr;
	} else {
        return f;
    }
}


void
Client::electmuxer()
{
	/* if there is anyone else sleeping, wake them to mux */
	for(auto rpc=sleep.next; rpc != &sleep; rpc = rpc->next){
        if (!rpc->isAsync()) {
			muxer = rpc;
			concurrency::threadModel->wake(&rpc->r);
			return;
		}
	}
	muxer = nullptr;
}
int 
Client::gettag(Rpc &r)
{
	int i, mw;
	Rpc **w;
    auto Found = [this, &r](auto index) {
        _nwait++;
        wait[index] = &r;
        r.setTag(index + _mintag);
        return r.getTag();
    };
	for(;;){
		/* wait for a free tag */
		while(_nwait == _mwait){
			if(_mwait < (_maxtag-_mintag)) {
				mw = _mwait;
				if(mw == 0) {
					mw = 1;
                } else {
					mw <<= 1;
                }
				w = (decltype(w))realloc(wait, mw * sizeof *w);
                if (!w) {
					return -1;
                }
				memset(w+_mwait, 0, (mw-_mwait) * sizeof *w);
				wait = w;
				_freetag = _mwait;
				_mwait = mw;
				break;
			}
            tagrend.sleep();
		}

		i=_freetag;
		if(wait[i] == 0) {
            return Found(i);
        }
		for(; i<_mwait; i++) {
			if(wait[i] == 0) {
                return Found(i);
            }
        }
		for(i=0; i<_freetag; i++) {
			if(wait[i] == 0) {
                return Found(i);
            }
        }
		/* should not fall out of while without free tag */
        throw "Fell out of loop without free tag!";
	}

}

void
Client::puttag(Rpc& r)
{
	auto i = r.getTag() - _mintag;
	assert(wait[i] == &r);
	wait[i] = nullptr;
	_nwait--;
	_freetag = i;
    tagrend.wake();
    r.getRendez().deactivate();
}
bool
Rpc::sendrpc(Fcall& f) {
    { 
        concurrency::Locker<Mutex> lk(mux.lk);
        tag = mux.gettag(this);
        f.setTag(tag);
        mux.enqueue(this);
    }
    { 
        concurrency::Locker<Mutex> a(mux.wlock);
        if (!fcall2msg(&mux.getWmsg(), &f) || !mux.getConnection().sendmsg(mux.getWmsg())) {
            concurrency::Locker<Mutex> lk(mux.lk);
            mux.dequeue(this);
            mux.puttag(this);
            return false;
        }
    }
    return true;
}
int
Rpc::sendrpc(Fcall *f)
{
	auto ret = 0;
	/* assign the tag, add selves to response queue */
    {
        concurrency::Locker<Mutex> lk(mux.lk);
        tag = mux.gettag(this);
        f->setTag(tag);
        mux.enqueue(this);
    }

    {
        concurrency::Locker<Mutex> a(mux.wlock);
        if(!fcall2msg(&mux.getWmsg(), f) || !mux.getConnection().sendmsg(mux.getWmsg())) {
            concurrency::Locker<Mutex> lk(mux.lk);
            mux.dequeue(this);
            mux.puttag(this);
            ret = -1;
        }
    }
    return ret;
}

void
Client::dispatchandqlock(std::shared_ptr<Fcall> f)
{
	int tag = f->getTag() - _mintag;
    lk.lock();
	/* hand packet to correct sleeper */
	if(tag < 0 || tag >= _mwait) {
        throw Exception("libjyq: received unfeasible tag: ", f->getTag(), "(min: ", _mintag, ", max: ", _mintag+_mwait, ")\n");
	}
	auto r2 = wait[tag];
    if (!r2 || !(r2->prev)) {
        throw Exception("libjyq: received message with bad tag\n");
	}
	r2->p = f;
    dequeue(r2);
    r2->r.wake();
}
void
Rpc::enqueueSelf(Rpc& other) {
	next = other.next;
	prev = &other;
	next->prev = this;
	prev->next = this;
}
void
Client::enqueue(Rpc* r) {
    r->enqueueSelf(sleep);
}
void
Rpc::dequeueSelf() {
	next->prev = prev;
	prev->next = next;
	prev = nullptr;
	next = nullptr;
}
void
Client::dequeue(Rpc* r) {
    r->dequeueSelf();
}
std::shared_ptr<Fcall>
Client::muxrpc(Fcall& tx) 
{
    Rpc r(*this);
    std::shared_ptr<Fcall> p;

    if (!r.sendrpc(tx)) {
        return nullptr;
    }
    lk.lock();
	/* wait for our packet */
	while(muxer && muxer != &r && !r.p) {
        r.getRendez().sleep();
    }

	/* if not done, there's no muxer; start muxing */
	if(!r.p){
		assert(muxer == nullptr || muxer == &r);
		muxer = &r;
		while(!r.p){
            lk.unlock();
            p.reset(muxrecv());
            if (!p) {
				/* eof -- just give up and pass the buck */
                lk.lock();
                dequeue(&r);
				break;
			}
			dispatchandqlock(p);
		}
		electmuxer();
	}
	p = r.p;
	puttag(&r);
    lk.unlock();
    if (!p) {
        throw Exception("unexpected eof");
    }
	return p;
}
} // end namespace jyq


