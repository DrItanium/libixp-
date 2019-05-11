/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_CLIENT_H__
#define LIBJYQ_CLIENT_H__
#include <string>
#include <functional>
#include <list>
#include <memory>
#include "types.h"
#include "thread.h"
#include "Msg.h"
#include "Fcall.h"
#include "Rpc.h"
#include "stat.h"
#include "socket.h"

namespace jyq {
    struct CFid;
    struct Client {
        public:
            static Client* mount(const char*);
            static Client* mountfd(int fd) { return mountfd(Connection(fd)); }
            static Client*  mountfd(const Connection& c);
            static Client*	nsmount(const char*);

            static Client* mount(const std::string& str) { return mount(str.c_str()); }
            static Client* nsmount(const std::string& str) { return nsmount(str.c_str()); }
            Client(int fd);
            Client(const Connection& c);
            ~Client();
            Connection& getConnection() noexcept { return fd; }
            const Connection& getConnection() const noexcept { return fd; }
        private:
            //int     fd;
            Connection fd;
        public:
            uint    msize;
            uint    lastfid;

            /* Private members */
            uint    nwait;
            uint    mwait;
            uint    freetag;
            std::list<std::shared_ptr<CFid>> freefid;
            Msg     rmsg;
            Msg     wmsg;
            Mutex	lk;
            Mutex	rlock;
            Mutex	wlock;
            Rendez	tagrend;
            Rpc**	wait;
            Rpc*		muxer;
            Rpc		sleep;
            int		mintag;
            int		maxtag;
            bool remove(const char*);
            std::shared_ptr<CFid> create(const char*, uint perm, uint8_t mode);
            std::shared_ptr<CFid> open(const char*, uint8_t);
            std::shared_ptr<Stat> stat(const char*);
            bool remove(const std::string& str) noexcept { return remove(str.c_str()); }
            auto create(const std::string& str, uint perm, uint8_t mode) { return create(str.c_str(), perm, mode); }
            auto create(const std::string& str, uint perm, OMode mode) { return create(str.c_str(), perm, uint8_t(mode)); }
            auto create(const char* str, uint perm, OMode mode) { return create(str, perm, uint8_t(mode)); }
            auto open(const std::string& str, uint8_t val) { return open(str.c_str(), val); }
            auto open(const std::string& str, OMode mode) { return open(str, uint8_t(mode)); }
            auto open(const char* str, OMode mode) { return open(str, uint8_t(mode)); }
            auto stat(const std::string& str) { return stat(str.c_str()); }
            Fcall* muxrpc(Fcall*);
            //std::shared_ptr<Fcall> muxrpc(Fcall& tx)
            std::shared_ptr<CFid> getFid();
            std::shared_ptr<CFid> walk(const char*);
            std::shared_ptr<CFid> walkdir(char *path, const char **rest);
            bool dofcall(Fcall *fcall);
            std::optional<Fcall> dofcall(Fcall& fcall);
            void enqueue(Rpc*);
            void dequeue(Rpc*);
            void putfid(std::shared_ptr<CFid> cfid);
            void clunk(std::shared_ptr<CFid> fid);
            DoFcallFunc getDoFcallLambda() noexcept { return [this](auto* ptr) { return this->dofcall(ptr); }; }
            int gettag(Rpc* r) { return gettag(*r); }
            int gettag(Rpc& r);
            void puttag(Rpc& r);
            void puttag(Rpc* r) { return puttag(*r); }
        private:
            Fcall* muxrecv();
            void electmuxer();
            void dispatchandqlock(Fcall *f);
            void allocmsg(int n);





    };
} // end namespace jyq
#endif // end LIBJYQ_CLIENT_H__
