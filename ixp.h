#ifndef LIBIXP_H__
#define LIBIXP_H__
/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */

#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include <ostream>
#include <iostream>
#include <sstream>
#include <functional>
#include <memory>
#include <any>

namespace ixp {
    using uint = unsigned int;
    constexpr auto ApiVersion = 135;
    constexpr auto Version = "9P2000";
    constexpr auto NoTag = uint16_t(~0); 
    constexpr auto NoFid = ~0u;

    template<typename ... Args>
        void print(std::ostream& os, Args&& ... args) {
            (os << ... << args);
        }

    template<typename ... Args>
        void errorPrint(Args&& ... args) {
            ixp::print(std::cerr, args...);
        }

    template<typename ... Args>
        void fatalPrint(Args&& ... args) {
            errorPrint(args...);
            throw 1;
        }

    template<typename T>
    constexpr T min(T a, T b) noexcept {
        return a < b ? a : b;
    }
    namespace maximum {
        constexpr auto Version = 32;
        constexpr auto Msg = 8192;
        constexpr auto Error = 128;
        constexpr auto Cache = 32;
        constexpr auto Flen = 128;
        constexpr auto Ulen = 32;
        constexpr auto Welem = 16;
    } // end namespace maximum

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
    enum {
        IXP_ERRMAX = maximum::Error,
    };

    struct Mutex {
        Mutex();
        ~Mutex();
        std::any aux;
    };

    struct Rendez {
        Rendez();
        ~Rendez();
        Mutex* mutex;
        std::any aux;
    };

    struct RWLock {
        RWLock();
        ~RWLock();
        std::any aux;
    };

    struct Msg {
        enum Mode {
            Pack,
            Unpack,
        };
        char*	data; /* Begining of buffer. */
        char*	pos;  /* Current position in buffer. */
        char*	end;  /* End of message. */ 
        uint	size; /* Size of buffer. */
        Mode mode; /* MsgPack or MsgUnpack. */
        void pu8(uint8_t*);
        void pu16(uint16_t*);
        void pu32(uint32_t*);
        void pu64(uint64_t*);
        void pdata(char**, uint);
        void pstring(char**);
        void pstrings(uint16_t*, char**, uint);
        void pqids(uint16_t*, Qid*, uint);
        void pqid(Qid*);
        void pstat(Stat*);
        void pfcall(Fcall*);
        static Msg message(char*, uint len, Mode mode);
        void packUnpack(uint8_t* v) { pu8(v); }
        void packUnpack(uint16_t* v) { pu16(v); }
        void packUnpack(uint32_t* v) { pu32(v); }
        void packUnpack(uint64_t* v) { pu64(v); }
        template<typename T>
        void packUnpack(T value) noexcept {
            static_assert(!std::is_same_v<std::decay_t<T>, Msg>, "This would cause an infinite loop!");
            value.packUnpack(*this);
        }
        template<typename T>
        void packUnpack(T* value) noexcept {
            static_assert(!std::is_same_v<std::decay_t<T>, Msg>, "This would cause an infinite loop!");
            value->packUnpack(*this);
        }
        template<typename ... Args>
        void packUnpackMany(Args&& ... fields) noexcept {
            (packUnpack(std::forward<Args>(fields)), ...);
        }
        private:
           void puint(uint, uint32_t*);
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
        Qid		qid;
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
    struct FVersion {
        FHdr		hdr;
        uint32_t	msize;
        char*		version;
    };
    struct FTFlush {
        FHdr		hdr;
        uint16_t	oldtag;
    };
    struct FError {
        FHdr		hdr;
        char*		ename;
    };
    struct FROpen {
        FHdr		hdr;
        Qid		qid; /* +Rattach */
        uint32_t	iounit;
    };
    struct FRAuth {
        FHdr		hdr;
        Qid		aqid;
    };
    struct FAttach {
        FHdr		hdr;
        uint32_t	afid;
        char*		uname;
        char*		aname;
    };
    struct FTCreate {
        FHdr		hdr;
        uint32_t	perm;
        char*		name;
        uint8_t		mode; /* +Topen */
    };
    struct FTWalk {
        FHdr	hdr;
        uint32_t	newfid;
        uint16_t	nwname;
        char*		wname[maximum::Welem];
    };
    struct FRWalk {
        FHdr		hdr;
        uint16_t	nwqid;
        Qid		wqid[maximum::Welem];
    };
    struct FIO {
        FHdr		hdr;
        uint64_t	offset; /* Tread, Twrite */
        uint32_t	count; /* Tread, Twrite, Rread */
        char*		data; /* Twrite, Rread */
    };
    struct FRStat {
        FHdr		hdr;
        uint16_t	nstat;
        uint8_t*	stat;
    };
    struct FTWStat {
        FHdr		hdr;
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
        FHdr		hdr;
        FVersion	version;
        FVersion	tversion;
        FVersion	rversion;
        FTFlush	tflush;
        FROpen	ropen;
        FROpen	rcreate;
        FROpen	rattach;
        FError	error;
        FRAuth	rauth;
        FAttach	tattach;
        FAttach	tauth;
        FTCreate	tcreate;
        FTCreate	topen;
        FTWalk	twalk;
        FRWalk	rwalk;
        FTWStat	twstat;
        FRStat	rstat;
        FIO		twrite;
        FIO		rwrite;
        FIO		tread;
        FIO		rread;
        FIO		io;
        void packUnpack(Msg& msg) noexcept;
        static void free(Fcall*);
        FHdr& getHeader() noexcept { return hdr; }
        const FHdr& getHeader() const noexcept { return hdr; }
        void setType(FType type) noexcept { hdr.type = type; }
        void setFid(decltype(FHdr::fid) value) noexcept { hdr.fid = value; }
        void setTypeAndFid(FType type, decltype(FHdr::fid) value) noexcept {
            setType(type);
            setFid(value);
        }
        Fcall() = default;
        Fcall(FType type) {
            setType(type);
        }
        Fcall(FType type, decltype(FHdr::fid) value) : Fcall(type) {
            setFid(value);
        }
    };

    struct Conn {
        Server*	srv;
        std::any	aux;	/* Arbitrary pointer, to be used by handlers. */
        int		fd;	/* The file descriptor of the connection. */
        std::function<void(Conn*)> read, close;
        bool		closed;	/* Non-zero when P<fd> has been closed. */

        /* Private members */
        Conn		*next;
    };

    struct Server {
        Conn*	conn;
        Mutex	lk;
        Timer*	timer;
        std::function<void(Server*)> preselect;
        std::any   aux;
        bool	running;
        int		maxfd;
        fd_set		rd;
        Conn* listen(int, const std::any&,
                std::function<void(Conn*)> read,
                std::function<void(Conn*)> close);
        int	    serverloop();
        void	server_close();
        bool unsettimer(long);
        long settimer(long, std::function<void(long, const std::any&)>, const std::any& aux);
        long nexttimer();
    };

    struct Rpc {
        Client*	mux;
        Rpc*		next;
        Rpc*		prev;
        Rendez	r;
        uint		tag;
        Fcall*	p;
        int		waiting;
        int		async;
    };

    struct Client {
        static void	unmount(Client*);
        static Client*	mount(const char*);
        static Client*	mountfd(int);
        static Client*	nsmount(const char*);

        static inline Client* mount(const std::string& str) { return mount(str.c_str()); }
        static inline Client* nsmount(const std::string& str) { return nsmount(str.c_str()); }
        ~Client();
        int	fd;
        uint	msize;
        uint	lastfid;

        /* Private members */
        uint		nwait;
        uint		mwait;
        uint		freetag;
        CFid*	freefid;
        Msg		rmsg;
        Msg		wmsg;
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
        CFid*	create(const char*, uint perm, uint8_t mode);
        CFid*	open(const char*, uint8_t);
        Stat*	stat(const char*);
        inline bool  remove(const std::string& str) noexcept { return remove(str.c_str()); }
        inline CFid* create(const std::string& str, uint perm, uint8_t mode) { return create(str.c_str(), perm, mode); }
        inline CFid* create(const std::string& str, uint perm, OMode mode) { return create(str.c_str(), perm, mode); }
        inline CFid* create(const char* str, uint perm, OMode mode) { return create(str, perm, uint8_t(mode)); }
        inline CFid* open(const std::string& str, uint8_t val) { return open(str.c_str(), val); }
        inline CFid* open(const std::string& str, OMode mode) { return open(str, uint8_t(mode)); }
        inline CFid* open(const char* str, OMode mode) { return open(str, uint8_t(mode)); }
        inline Stat* stat(const std::string& str) { return stat(str.c_str()); }
        void	muxfree();
        void	muxinit();
        Fcall*	muxrpc(Fcall*);
    };

    struct CFid {
        uint32_t	fid;
        Qid		qid;
        uint8_t		mode;
        uint		open;
        uint		iounit;
        uint32_t	offset;
        Client*	client;

        /* Private members */
        CFid*	next;
        Mutex	iolock;
        bool close();
        bool clunk(); 
        long read(void*, long);
        long pread(void*, long, int64_t);
        int  print(const char*, ...);
        long pwrite(const void*, long, int64_t);
        int vprint(const char*, va_list);
        inline int vprint(const std::string& str, va_list l) { return vprint(str.c_str(), l); }
        long write(const void*, long);
        Stat*	fstat();
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
        char*		uid;	/* The uid of the file opener. */
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
        using ReqFunc = std::function<void(Req9*)>;
        ReqFunc attach,
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

    namespace concurrency {
        /**
         * Type: Thread
         * Type: Mutex
         * Type: RWLock
         * Type: Rendez
         * Variable: thread
         *
         * The Thread structure is used to adapt libixp to any of the
         * myriad threading systems it may be used with. Before any
         * other of libixp's functions is called, thread may be set
         * to a structure filled with implementations of various locking
         * primitives, along with primitive IO functions which may
         * perform context switches until data is available.
         *
         * The names of the functions should be fairly self-explanitory.
         * Read/write locks should allow multiple readers and a single
         * writer of a shared resource, but should not allow new readers
         * while a writer is waitng for a lock. Mutexes should allow
         * only one accessor at a time. Rendezvous points are similar to
         * pthread condition types. P<errbuf> should return a
         * thread-local buffer or the size IXP_ERRMAX.
         *
         * See also:
         *	F<pthread_init>, F<taskinit>, F<rubyinit>
         */
        class ThreadImpl {
            public:
                ThreadImpl() = default;
                virtual ~ThreadImpl() = default;
                /* Read/write lock */
                virtual bool init(RWLock*) = 0;
                virtual void rlock(RWLock*) = 0;
                virtual bool canrlock(RWLock*) = 0;
                virtual void runlock(RWLock*) = 0;
                virtual void wlock(RWLock*) = 0;
                virtual bool canwlock(RWLock*) = 0;
                virtual void wunlock(RWLock*) = 0;
                virtual void destroy(RWLock*) = 0;
                /* Mutex */
                virtual bool init(Mutex*) = 0;
                virtual bool canlock(Mutex*) = 0;
                virtual void lock(Mutex*) = 0;
                virtual void unlock(Mutex*) = 0;
                virtual void destroy(Mutex*) = 0;
                /* Rendezvous point */
                virtual bool init(Rendez*) = 0;
                virtual bool wake(Rendez*) = 0;
                virtual bool wakeall(Rendez*) = 0;
                virtual void sleep(Rendez*) = 0;
                virtual void destroy(Rendez*) = 0;
                /* Other */
                virtual char* errbuf() = 0;
                virtual ssize_t read(int, void*, size_t);
                virtual ssize_t write(int, const void*, size_t);
                virtual int select(int, fd_set*, fd_set*, fd_set*, timeval*);
                // interface with old design
                auto initmutex(Mutex* m) { return init(m); }
                auto initrwlock(RWLock* a) { return init(a); }
                void mdestroy(Mutex* m) { destroy(m); }
                void rwdestroy(RWLock* a) { destroy(a); }
                auto initrendez(Rendez* r) { return init(r); }
                void rdestroy(Rendez* r) { destroy(r); }

        };

        extern std::unique_ptr<ThreadImpl> threadModel;

        template<typename T>
            void setThreadingModel() noexcept {
                static_assert(std::is_base_of_v<ThreadImpl, T>, "Threading model must be a child of Thread");
                static_assert(std::is_default_constructible_v<T>, "Provided type is not default constructible");
                // TODO: insert static assertions to make sure that the type is a child of Thread
                threadModel = std::make_unique<T>();
            }

        class NoThreadImpl final : public ThreadImpl {
            public:
                using ThreadImpl::ThreadImpl;
                /* Read/write lock */
                bool init(RWLock*) override     { return false; }
                void rlock(RWLock*) override    { }
                bool canrlock(RWLock*) override { return true; }
                void runlock(RWLock*) override  { }
                void wlock(RWLock*) override    { }
                bool canwlock(RWLock*) override { return true; }
                void wunlock(RWLock*) override  { }
                void destroy(RWLock*) override  { }
                /* Mutex */
                bool init(Mutex*) override    { return false; }
                bool canlock(Mutex*) override { return true; }
                void lock(Mutex*) override    { }
                void unlock(Mutex*) override  { }
                void destroy(Mutex*) override { }
                /* Rendezvous point */
                bool init(Rendez*) override    { return false; }
                bool wake(Rendez*) override    { return false; }
                bool wakeall(Rendez*) override { return false; }
                void destroy(Rendez*) override { }
                void sleep(Rendez*) override;
                /* Other */
                char* errbuf() override;
        };
    } // end namespace concurrency

    extern std::function<int(char*, int, const char*, va_list)> vsnprint;
    extern std::function<char*(const char*, va_list)> vsmprint;
    extern std::function<void(Fcall*)> printfcall;

    uint	msg2fcall(Msg*, Fcall*);
    uint	fcall2msg(Msg*, Fcall*);
    char*	errbuf(void);
    void	errstr(char*, int);
    void	rerrstr(char*, int);
    void	werrstr(const char*, ...);
    void serve9conn(Conn*);
    void	hangup(Conn*);

    /* socket.c */
    int dial(const std::string&);
    int announce(const std::string&);
    uint sendmsg(int, Msg*);
    uint recvmsg(int, Msg*);
    uint64_t msec();

    /* util.c */
    void	cleanname(char*);
    void*	emalloc(uint);
    void*	emallocz(uint);
    void	eprint(const char*, ...);
    void*	erealloc(void*, uint);
    char*	estrdup(const char*);
    char*	getNamespace(void);
    char*	smprint(const char*, ...);
    uint	strlcat(char*, const char*, uint);
    uint	tokenize(char**, uint len, char*, char);
} // end namespace ixp

#endif
