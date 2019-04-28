#ifndef LIBJYQ_H__
#define LIBJYQ_H__
/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */

#include <cstdarg>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include <ostream>
#include <iostream>
#include <sstream>
#include <functional>
#include <memory>
#include <any>
#include <list>
#include "types.h"
#include "PrintFunctions.h"
#include "thread.h"
#include "util.h"

namespace jyq {
    /* 9P message types */
    enum class FType : uint8_t {
        TVersion = 100,
        RVersion,
        TAuth = 102,
        RAuth,
        TAttach = 104,
        RAttach,
        TError = 106, /* illegal */
        RError,
        TFlush = 108,
        RFlush,
        TWalk = 110,
        RWalk,
        TOpen = 112,
        ROpen,
        TCreate = 114,
        RCreate,
        TRead = 116,
        RRead,
        TWrite = 118,
        RWrite,
        TClunk = 120,
        RClunk,
        TRemove = 122,
        RRemove,
        TStat = 124,
        RStat,
        TWStat = 126,
        RWStat,
    };

    /* from libc.h in p9p */
    enum class OMode : uint16_t {
        READ	= 0,	/* open for read */
        WRITE	= 1,	/* write */
        RDWR	= 2,	/* read and write */
        EXEC	= 3,	/* execute, == read but check execute permission */
        TRUNC	= 16,	/* or'ed in (except for exec), truncate file first */
        CEXEC	= 32,	/* or'ed in, close on exec */
        RCLOSE	= 64,	/* or'ed in, remove on close */
        DIRECT	= 128,	/* or'ed in, direct access */
        NONBLOCK	= 256,	/* or'ed in, non-blocking call */
        EXCL	= 0x1000,	/* or'ed in, exclusive use (create only) */
        LOCK	= 0x2000,	/* or'ed in, lock after opening */
        APPEND	= 0x4000	/* or'ed in, append only */
    };

    /* bits in Qid.type */
    enum class QType : uint8_t {
        DIR	= 0x80,	/* type bit for directories */
        APPEND	= 0x40,	/* type bit for append only files */
        EXCL	= 0x20,	/* type bit for exclusive use files */
        MOUNT	= 0x10,	/* type bit for mounted channel */
        AUTH	= 0x08,	/* type bit for authentication file */
        TMP	= 0x04,	/* type bit for non-backed-up file */
        SYMLINK	= 0x02,	/* type bit for symbolic link */
        FILE	= 0x00	/* type bits for plain file */
    };

    /* bits in Stat.mode */
    enum class DMode : uint32_t {
        EXEC	= 0x1,		/* mode bit for execute permission */
        WRITE	= 0x2,		/* mode bit for write permission */
        READ	= 0x4,		/* mode bit for read permission */

        // these were originally macros in this exact location... no clue
        // why...
        DIR = 0x8000'0000, /* mode bit for directories */
        APPEND = 0x4000'0000, /* mode bit for append only files */
        EXCL = 0x2000'0000, /* mode bit for exclusive use files */
        MOUNT = 0x1000'0000, /* mode bit for mounted channel */
        AUTH = 0x0800'0000, /* mode bit for authentication file */
        TMP = 0x0400'0000, /* mode bit for non-backed-up file */
        SYMLINK = 0x0200'0000, /* mode bit for symbolic link (Unix, 9P2000.u) */
        DEVICE = 0x0080'0000, /* mode bit for device file (Unix, 9P2000.u) */
        NAMEDPIPE = 0x0020'0000, /* mode bit for named pipe (Unix, 9P2000.u) */
        SOCKET = 0x0010'0000, /* mode bit for socket (Unix, 9P2000.u) */
        SETUID = 0x0008'0000, /* mode bit for setuid (Unix, 9P2000.u) */
        SETGID = 0x0004'0000, /* mode bit for setgid (Unix, 9P2000.u) */
    };


    struct Map;
    struct Conn9;
    struct Req9;
    struct Srv9;
    struct CFid;
    struct Client;
    struct Conn;
    struct Fid;
    struct Msg;
    struct Qid;
    struct Rpc;
    struct Server;
    struct Stat;
    struct Timer;

    struct Mutex;
    struct RWLock;
    struct Rendez;
    union Fcall;

    /* Threading */

    struct Msg : public ContainsSizeParameter<uint> {
        enum class Mode {
            Pack,
            Unpack,
        };
        /**
         * Exploit RAII to save and restore the mode that was in the Msg prior.
         */
        class ModePreserver final {
            public:
                ModePreserver(Msg& target, Mode newMode) : _target(target), _oldMode(target.getMode()), _writePerformed(target.getMode() != newMode) {
                    if (_writePerformed) {
                        _target.setMode(newMode);
                    }
                }
                ~ModePreserver() {
                    if (_writePerformed) {
                        _target.setMode(_oldMode);
                    }
                }
                ModePreserver(const ModePreserver&) = delete;
                ModePreserver(ModePreserver&&) = delete;
                ModePreserver& operator=(const ModePreserver&) = delete;
                ModePreserver& operator=(ModePreserver&&) = delete;
            private:
                Msg& _target;
                Mode _oldMode;
                bool _writePerformed;
        };
        char*	data; /* Begining of buffer. */
        char*	pos;  /* Current position in buffer. */
        char*	end;  /* End of message. */ 
        void pu8(uint8_t*);
        void pu16(uint16_t*);
        void pu32(uint32_t*);
        void pu64(uint64_t*);
        void pdata(char**, uint);
        void pstring(char**);
        void pstrings(uint16_t*, char**, uint);
        void pqids(uint16_t*, Qid*, uint);
        void pqid(Qid* value) { packUnpack(value); }
        void pqid(Qid& value) { packUnpack(value); }
        void pstat(Stat* value) { packUnpack(value); }
        void pstat(Stat& value) { packUnpack(value); }
        void pfcall(Fcall* value) { packUnpack(value); }
        void pfcall(Fcall& value) { packUnpack(value); }
        static Msg message(char*, uint len, Mode mode);
        template<typename T>
        void packUnpack(T& value) noexcept {
            using K = std::decay_t<T>;
            static_assert(!std::is_same_v<K, Msg>, "This would cause an infinite loop!");
            if constexpr (std::is_same_v<K, uint8_t>) {
                pu8(&value);
            } else if constexpr (std::is_same_v<K, uint16_t>) {
                pu16(&value);
            } else if constexpr (std::is_same_v<K, uint32_t>) {
                pu32(&value);
            } else if constexpr (std::is_same_v<K, uint64_t>) {
                pu64(&value);
            } else {
                value.packUnpack(*this);
            }
        }
        template<typename T>
        void packUnpack(T* value) noexcept {
            using K = std::decay_t<T>;
            static_assert(!std::is_same_v<K, Msg>, "This would cause an infinite loop!");
            if constexpr (std::is_same_v<K, uint8_t>) {
                pu8(value);
            } else if constexpr (std::is_same_v<K, uint16_t>) {
                pu16(value);
            } else if constexpr (std::is_same_v<K, uint32_t>) {
                pu32(value);
            } else if constexpr (std::is_same_v<K, uint64_t>) {
                pu64(value);
            } else {
                value->packUnpack(*this);
            }
        }
        template<typename ... Args>
        void packUnpackMany(Args&& ... fields) noexcept {
            (packUnpack(std::forward<Args>(fields)), ...);
        }
        template<typename T>
        void pack(T* value) noexcept {
            ModePreserver(*this, Mode::Pack);
            packUnpack(value);
        }
        template<typename T>
        void pack(T& value) noexcept {
            ModePreserver(*this, Mode::Pack);
            packUnpack(value);
        }
        template<typename T>
        void unpack(T* value) noexcept {
            ModePreserver(*this, Mode::Unpack);
            packUnpack(value);
        }
        template<typename T>
        void unpack(T& value) noexcept {
            ModePreserver(*this, Mode::Unpack);
            packUnpack(value);
        }
        template<typename T>
        T unpack() noexcept {
            T value;
            unpack(value);
            return value;
        }
        constexpr bool unpackRequested() const noexcept {
            return _mode == Mode::Unpack;
        }
        constexpr bool packRequested() const noexcept {
            return _mode == Mode::Pack;
        }
        constexpr Mode getMode() const noexcept { 
            return _mode;
        }
        void setMode(Mode mode) noexcept {
            this->_mode = mode;
        }

        private:
           void puint(uint, uint32_t*);
           Mode _mode; /* MsgPack or MsgUnpack. */
    };

    struct Qid {
        uint8_t		type;
        uint32_t	version;
        uint64_t	path;
        /* Private members */
        uint8_t		dir_type;
        void packUnpack(Msg& msg);
    };

    /* stat structure */
    struct Stat {
        uint16_t	type;
        uint32_t	dev;
        Qid         qid;
        uint32_t	mode;
        uint32_t	atime;
        uint32_t	mtime;
        uint64_t	length;
        char*	name;
        char*	uid;
        char*	gid;
        char*	muid;
        uint16_t    size() noexcept;
        static void free(Stat* stat);
        void packUnpack(Msg& msg) noexcept;
        //~Stat();
    };

    struct FHdr;
    struct FError;
    struct FROpen;
    struct FRAuth;
    struct FROpen;
    struct FROpen;
    struct FIO;
    struct FRStat;
    struct FVersion;
    struct FRWalk;
    struct FAttach;
    struct FAttach;
    struct FTCreate;
    struct FTFlush;
    struct FTCreate;
    struct FIO;
    struct FVersion;
    struct FTWalk;
    struct FIO;
    struct FTWStat;
    struct FAttach;
    struct FIO;
    struct FVersion;

    struct FHdr {
        FType type;
        uint16_t	tag;
        uint32_t	fid;
    };
    struct QueryHeader {
        QueryHeader() = default;
        ~QueryHeader() = default;
        FHdr hdr;
        constexpr auto getType() const noexcept { return hdr.type; }
        constexpr auto getFid() const noexcept { return hdr.fid; }
        constexpr auto getTag() const noexcept { return hdr.tag; }
    };
    struct FVersion : public QueryHeader, public ContainsSizeParameter<uint32_t> {
        char*		version;
    };
    struct FTFlush : public QueryHeader {
        uint16_t	oldtag;
    };
    struct FError : public QueryHeader {
        char*		ename;
    };
    struct FROpen : public QueryHeader {
        Qid		qid; /* +Rattach */
        uint32_t	iounit;
    };
    struct FRAuth : public QueryHeader {
        Qid		aqid;
    };
    struct FAttach : public QueryHeader {
        uint32_t	afid;
        char*		uname;
        char*		aname;
    };
    struct FAttachCxx : public QueryHeader {
        uint32_t     afid;
        std::string  uname;
        std::string  aname;
    };
    struct FTCreate : public QueryHeader {
        uint32_t	perm;
        char*		name;
        uint8_t		mode; /* +Topen */
    };
    struct FTCreateCxx : public QueryHeader {
        uint32_t     perm;
        std::string  name;
        uint8_t      mode; /* +Topen */
    };
    struct FTWalk : public QueryHeader, public ContainsSizeParameter<uint16_t>  {
        uint32_t	newfid;
        char*		wname[maximum::Welem];
    };
    struct FTWalkCxx : public QueryHeader, public ContainsSizeParameter<uint16_t>  {
        uint32_t	newfid;
        std::array<std::string, maximum::Welem> wname;
    };
    struct FRWalk : public QueryHeader, public ContainsSizeParameter<uint16_t> {
        Qid		wqid[maximum::Welem];
    };
    struct FRWalkCxx : public QueryHeader, public ContainsSizeParameter<uint16_t> {
        std::array<Qid, maximum::Welem> wqid;
    };
    struct FIO : public QueryHeader, public ContainsSizeParameter<uint32_t> {
        uint64_t	offset; /* Tread, Twrite */
        char*		data; /* Twrite, Rread */
    };
    struct FRStat : public QueryHeader, public ContainsSizeParameter<uint16_t> {
        uint8_t*	stat;
    };
    struct FTWStat : public QueryHeader {
        Stat		stat;
    };

    /**
     * Type: Fcall
     * Type: FType
     * Type: FAttach
     * Type: FError
     * Type: FHdr
     * Type: FIO
     * Type: FRAuth
     * Type: FROpen
     * Type: FRStat
     * Type: FRWalk
     * Type: FTCreate
     * Type: FTFlush
     * Type: FTWStat
     * Type: FTWalk
     * Type: FVersion
     *
     * The Fcall structure represents a 9P protocol message. The
     * P<hdr> element is common to all Fcall types, and may be used to
     * determine the type and tag of the message. The Fcall type is
     * used heavily in server applications, where it both presents a
     * request to handler functions and returns a response to the
     * client.
     *
     * Each member of the Fcall structure represents a certain
     * message type, which can be discerned from the P<hdr.type> field.
     * This value corresponds to one of the FType constants. Types
     * with significant overlap use the same structures, thus TRead and
     * RWrite are both represented by FIO and can be accessed via the
     * P<io> member as well as P<tread> and P<rwrite> respectively.
     *
     * See also:
     *	T<Srv9>, T<Req9>
     */
    union Fcall {
        FHdr     hdr;
        FVersion version, tversion,
                 rversion;
        FTFlush  tflush;
        FROpen   ropen, rcreate,
                 rattach;
        FError   error;
        FRAuth   rauth;
        FAttach  tattach, tauth;
        FTCreate tcreate, topen;
        FTWalk   twalk;
        FRWalk   rwalk;
        FTWStat  twstat;
        FRStat   rstat;
        FIO      twrite, rwrite, 
                 tread, rread,
                 io;
        void packUnpack(Msg& msg) noexcept;
        static void free(Fcall*);
        FType getType() const noexcept { return hdr.type; }
        decltype(FHdr::fid) getFid() const noexcept { return hdr.fid; }
        void setType(FType type) noexcept { hdr.type = type; }
        void setFid(decltype(FHdr::fid) value) noexcept { hdr.fid = value; }
        void setTypeAndFid(FType type, decltype(FHdr::fid) value) noexcept {
            setType(type);
            setFid(value);
        }
        void setTag(decltype(FHdr::tag) value) noexcept { hdr.tag = value; }
        void setNoTag() noexcept { setTag(NoTag); }
        Fcall() = default;
        Fcall(FType type) {
            setType(type);
        }
        Fcall(FType type, decltype(FHdr::fid) value) : Fcall(type) {
            setFid(value);
        }
    };

    struct Conn {
        ~Conn();
        Server*	srv;
        std::any	aux;	/* Arbitrary pointer, to be used by handlers. */
        int		fd;	/* The file descriptor of the connection. */
        std::function<void(Conn*)> read, close;
        bool		closed;	/* Non-zero when P<fd> has been closed. */

        void    serve9conn();
    };
    void	hangup(Conn*);

    struct Server {
        std::list<std::shared_ptr<Conn>> conns;
        Mutex	lk;
        Timer*	timer;
        std::function<void(Server*)> _preselect;
        std::any   aux;
        bool	running;
        int		maxfd;
        fd_set		rd;
        std::shared_ptr<Conn> listen(int, const std::any&,
                std::function<void(Conn*)> read,
                std::function<void(Conn*)> close);
        bool serverloop();
        void close();
        bool unsettimer(long);
        long settimer(long, std::function<void(long, const std::any&)>, const std::any& aux);
        long nexttimer();
        void lock();
        void unlock();
        bool canlock();
        void preselect() {
            if (_preselect) {
                _preselect(this);
            }
        }
        private:
            void prepareSelect();
            void handleConns();
    };

    struct Rpc {
        Rpc(Client& m);
        ~Rpc() = default;
        Client&	mux;
        Rpc*		next;
        Rpc*		prev;
        Rendez	r;
        uint		tag;
        Fcall*	p;
        bool waiting;
        bool async;
    };

    struct Client {
        using DoFcallFunc = std::function<bool(Fcall*)>;
        static void	unmount(Client*);
        static Client*	mount(const char*);
        static Client*	mountfd(int);
        static Client*	nsmount(const char*);

        static inline Client* mount(const std::string& str) { return mount(str.c_str()); }
        static inline Client* nsmount(const std::string& str) { return nsmount(str.c_str()); }
        Client();
        ~Client();
        int     fd;
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

    struct CFid {
        using DoFcallFunc = Client::DoFcallFunc;
        uint32_t fid;
        Qid      qid;
        uint8_t  mode;
        uint     open;
        uint     iounit;
        uint32_t offset;

        /* Private members */
        Mutex iolock;
        bool close(DoFcallFunc);
        bool clunk(DoFcallFunc); 
        bool performClunk(DoFcallFunc);
        long read(void*, long, DoFcallFunc);
        long pread(void*, long, int64_t, DoFcallFunc);
        long pwrite(const void*, long, int64_t, DoFcallFunc);
        long write(const void*, long, DoFcallFunc);
        std::shared_ptr<Stat> fstat(DoFcallFunc);
    };

    /**
     * Type: Fid
     *
     * Represents an open file for a 9P connection. The same
     * structure persists as long as the file remains open, and is
     * installed in the T<Req9> structure for any request Fcall
     * which references it. Handlers may use the P<aux> member to
     * store any data which must persist for the life of the open
     * file.
     *
     * See also:
     *	T<Req9>, T<Qid>, T<OMode>
     */
    struct Fid {
        std::string		uid;	/* The uid of the file opener. */
        std::any    aux;    // Arbitrary pointer, to be used by handlers. 
        uint32_t		fid;    /* The ID number of the fid. */
        Qid		qid;    /* The filesystem-unique QID of the file. */
        signed char	omode;  /* The open mode of the file. */
        uint		iounit; /* The maximum size of any IO request. */

        /* Private members */
        Conn9*	conn;
        Map*		map;
    };

    struct Req9 {
        Srv9*	srv;
        Fid*		fid;    /* Fid structure corresponding to FHdr.fid */
        Fid*		newfid; /* Corresponds to FTWStat.newfid */
        Req9*	oldreq; /* For TFlush requests, the original request. */
        Fcall	ifcall; /* The incoming request fcall. */
        Fcall	ofcall; /* The response fcall, to be filled by handler. */
        std::any    aux; // Arbitrary pointer, to be used by handlers. 

        /* Private members */
        Conn9 *conn;
        void respond(const char *err);
        inline void respond(const std::string& err) { respond(err.c_str()); }
    };

    struct Srv9 {
        std::any aux;
        std::function<void(Req9*)> attach,
                clunk,
                create,
                flush,
                open,
                read,
                remove,
                stat,
                walk,
                write,
                wstat;
        std::function<void(Fid*)> freefid;
    };



    uint	msg2fcall(Msg*, Fcall*);
    uint	fcall2msg(Msg*, Fcall*);

    /* socket.c */
    int dial(const std::string&);
    int announce(const std::string&);
    uint sendmsg(int, Msg*);
    uint recvmsg(int, Msg*);
    uint64_t msec();

    struct Timer {
        Timer*		link;
        uint64_t	msec;
        long		id;
        std::function<void(long, const std::any&)> fn;
        std::any	aux;
    };
} // end namespace jyq
extern char* argv0;
#define ARGBEGIN \
    int _argtmp=0, _inargv=0; char *_argv=nullptr; \
if(!argv0) {argv0=*argv; argv++, argc--;} \
_inargv=1; \
while(argc && argv[0][0] == '-') { \
    _argv=&argv[0][1]; argv++; argc--; \
    if(_argv[0] == '-' && _argv[1] == '\0') \
    break; \
    while(*_argv) switch(*_argv++)
#define ARGEND }_inargv=0

#define EARGF(f) ((_inargv && *_argv) ? \
        (_argtmp=strlen(_argv), _argv+=_argtmp, _argv-_argtmp) \
        : ((argc > 0) ? \
            (--argc, ++argv, *(argv-1)) \
            : ((f), (char*)0)))
#define ARGF() EARGF(0)
#define nelem(ary) (sizeof(ary) / sizeof(*ary))

#endif
