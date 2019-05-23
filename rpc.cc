/* From Plan 9's libmux.
 * Copyright (c) 2003 Russ Cox, Massachusetts Institute of Technology
 * Distributed under the same terms as libjyq.
 */
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "Rpc.h"
#include "Msg.h"
#include "Client.h"
#include "socket.h"
#include "util.h"
#include "PrintFunctions.h"

namespace jyq {
RpcBody::RpcBody() : _tag(0), _p(nullptr), _waiting(true), _async(false) { }

Fcall*
Client::muxrecv()
{
	//Fcall *f = nullptr;
    auto theRlock = getReadLock();
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
            rpc->getContents().getRendez().notify_one();
			return;
		}
	}
    muxer.reset();
}
int 
Client::gettag(Rpc &r, Lock& lock)
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
            _tagrend.wait(lock);
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
    if (wait[i] != r) {
        // assert(wait[i] == r);
        throw Exception("wait[",i,"] does not equal r");
    }
	wait[i] = nullptr;
	_nwait--;
	_freetag = i;
    _tagrend.notify_one();
    //r->getContents().getRendez().deactivate();
}
bool
Client::sendrpc(Rpc& r, Fcall& f) {
    { 
        auto lk = getLock();
        r->getContents().setTag(gettag(r, lk));
        f.setTag(r->getContents().getTag());
        enqueue(r);
    }
    { 
        auto wlock = getWriteLock();
        if (!getWmsg().pack(f) || !getConnection().sendmsg(_wmsg)) {
            auto lk = getLock();
            dequeue(r);
            puttag(r);
            return false;
        }
    }
    return true;
}

void
Client::dispatchandqlock(std::shared_ptr<Fcall> f, Lock& m)
{
	int tag = f->getTag() - _mintag;
    m.lock();
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
    r2->getContents().getRendez().notify_one();
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
    Rpc r = std::make_shared<BareRpc>();
    std::shared_ptr<Fcall> p;

    if (!sendrpc(r, tx)) {
        return nullptr;
    }
    auto currentLock = getLock();
	/* wait for our packet */
	while(muxer.lock() && (muxer.lock() != r) && !r->getContents().getP()) {
        r->getContents().getRendez().wait(currentLock);
    }

	/* if not done, there's no muxer; start muxing */
	if(!r->getContents().getP()){
        if (!(muxer.lock() == nullptr || muxer.lock() == r)) {
            //assert(muxer.lock() == nullptr || muxer.lock() == r);
            throw Exception("muxer check failed!");
        }
        muxer = r;
		while(!r->getContents().getP()){
            currentLock.unlock();
            p.reset(muxrecv());
            if (!p) {
				/* eof -- just give up and pass the buck */
                currentLock.lock();
                dequeue(r);
				break;
			}
			dispatchandqlock(p, currentLock);
		}
		electmuxer();
	}
    p = r->getContents().getP();
	puttag(&r);
    if (!p) {
        throw Exception("unexpected eof");
    }
	return p;
}
} // end namespace jyq


