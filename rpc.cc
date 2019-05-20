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
RpcBody::RpcBody(Client& m) : _mux(m) {
    _waiting = true;
    _r.setMutex(&m.getLock());
    _p = nullptr;
}

Fcall*
Client::muxrecv()
{
	//Fcall *f = nullptr;
    concurrency::Locker<Mutex> theRlock(_rlock);
    if (fd.recvmsg(_rmsg) == 0) {
        return nullptr;
    }
	if(auto f = new Fcall(); _rmsg.unpack(*f) == 0) {
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
    for(auto rpc=sleep->getNext(); rpc != sleep; rpc = rpc->getNext()) {
        if (!rpc->getContents().isAsync()) {
            muxer = rpc;
			concurrency::threadModel->wake(&rpc->getContents().getRendez());
			return;
		}
	}
    muxer.reset();
}
int 
Client::gettag(Rpc &r)
{
	int i, mw;
    auto Found = [this, &r](auto index) {
        _nwait++;
        wait[index] = r;
        r->getContents().setTag(index + _mintag);
        return r->getContents().getTag();
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
                wait.resize(mw, nullptr);
				_freetag = _mwait;
				_mwait = mw;
				break;
			}
            _tagrend.sleep();
		}

		i=_freetag;
		if(wait[i] == nullptr) {
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
	auto i = r->getContents().getTag() - _mintag;
	assert(wait[i] == r);
	wait[i] = nullptr;
	_nwait--;
	_freetag = i;
    _tagrend.wake();
    r->getContents().getRendez().deactivate();
}
bool
RpcBody::sendrpc(Fcall& f) {
    { 
        concurrency::Locker<Mutex> lk(_mux.getLock());
        _tag = _mux.gettag(this);
        f.setTag(_tag);
        _mux.enqueue(this);
    }
    { 
        concurrency::Locker<Mutex> a(_mux.getWriteLock());
        if (!_mux.getWmsg().pack(f) || !_mux.getConnection().sendmsg(_mux.getWmsg())) {
            concurrency::Locker<Mutex> lk(_mux.getLock());
            _mux.dequeue(this);
            _mux.puttag(this);
            return false;
        }
    }
    return true;
}
int
RpcBody::sendrpc(Fcall *f)
{
	auto ret = 0;
	/* assign the tag, add selves to response queue */
    {
        concurrency::Locker<Mutex> lk(_mux.getLock());
        _tag = _mux.gettag(this);
        f->setTag(_tag);
        _mux.enqueue(this);
    }

    {
        concurrency::Locker<Mutex> a(_mux.getWriteLock());
        if (!_mux.getWmsg().pack(*f) || !_mux.getConnection().sendmsg(_mux.getWmsg())) {
            concurrency::Locker<Mutex> lk(_mux.getLock());
            _mux.dequeue(this);
            _mux.puttag(this);
            ret = -1;
        }
    }
    return ret;
}

void
Client::dispatchandqlock(std::shared_ptr<Fcall> f)
{
	int tag = f->getTag() - _mintag;
    _lk.lock();
	/* hand packet to correct sleeper */
	if(tag < 0 || tag >= _mwait) {
        throw Exception("libjyq: received unfeasible tag: ", f->getTag(), "(min: ", _mintag, ", max: ", _mintag+_mwait, ")\n");
	}
	auto r2 = wait[tag];
    if (!r2 || !(r2->getPrevious())) {
        throw Exception("libjyq: received message with bad tag\n");
	}
	r2->getContents().setP(f);
    dequeue(r2);
    r2->getContents().getRendez().wake();
}
void
Client::enqueue(Rpc& r) {
    r->setNext(sleep->getNext());
    r->setPrevious(sleep);
    r->getNext()->setPrevious(r);
    r->getPrevious()->setNext(r);
}

void
Client::dequeue(Rpc& r) {
    r->getNext()->setPrevious(r->getPrevious());
    r->getPrevious()->setNext(r->getNext());
    r->clearLinks();
}
std::shared_ptr<Fcall>
Client::muxrpc(Fcall& tx) 
{
    Rpc r = std::make_shared<BareRpc>(*this);
    std::shared_ptr<Fcall> p;

    if (!r->getContents().sendrpc(tx)) {
        return nullptr;
    }
    _lk.lock();
	/* wait for our packet */
	while(muxer.lock() && (muxer.lock() != r) && !r->getContents().getP()) {
        r->getContents().getRendez().sleep();
    }

	/* if not done, there's no muxer; start muxing */
	if(!r->getContents().getP()){
        assert(muxer.lock() == nullptr || muxer.lock() == r);
        muxer = r;
		while(!r->getContents().getP()){
            _lk.unlock();
            p.reset(muxrecv());
            if (!p) {
				/* eof -- just give up and pass the buck */
                _lk.lock();
                dequeue(r);
				break;
			}
			dispatchandqlock(p);
		}
		electmuxer();
	}
    p = r->getContents().getP();
	puttag(&r);
    _lk.unlock();
    if (!p) {
        throw Exception("unexpected eof");
    }
	return p;
}
} // end namespace jyq


