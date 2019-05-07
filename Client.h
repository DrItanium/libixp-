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
        static void	unmount(Client*);
        static Client*	mount(const char*);
        static Client*	mountfd(int);
        static Client*	nsmount(const char*);

        static inline Client* mount(const std::string& str) { return mount(str.c_str()); }
        static inline Client* nsmount(const std::string& str) { return nsmount(str.c_str()); }
        Client(int fd);
        ~Client();
        //int     fd;
        Connection fd;
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
        bool  remove(const std::string& str) noexcept { return remove(str.c_str()); }
        auto create(const std::string& str, uint perm, uint8_t mode) { return create(str.c_str(), perm, mode); }
        auto create(const std::string& str, uint perm, OMode mode) { return create(str.c_str(), perm, uint8_t(mode)); }
        auto create(const char* str, uint perm, OMode mode) { return create(str, perm, uint8_t(mode)); }
        auto open(const std::string& str, uint8_t val) { return open(str.c_str(), val); }
        auto open(const std::string& str, OMode mode) { return open(str, uint8_t(mode)); }
        auto open(const char* str, OMode mode) { return open(str, uint8_t(mode)); }
        auto stat(const std::string& str) { return stat(str.c_str()); }
        void muxfree();
        void muxinit();
        Fcall* muxrpc(Fcall*);
        std::shared_ptr<CFid> getFid();
        std::shared_ptr<CFid> walk(const char*);
        std::shared_ptr<CFid> walkdir(char *path, const char **rest);
        bool dofcall(Fcall *fcall);
        void enqueue(Rpc*);
        void dequeue(Rpc*);
        void putfid(std::shared_ptr<CFid> cfid);
        void clunk(std::shared_ptr<CFid> fid);
        DoFcallFunc getDoFcallLambda() noexcept { return [this](auto* ptr) { return this->dofcall(ptr); }; }
    };
} // end namespace jyq
#endif // end LIBJYQ_CLIENT_H__
