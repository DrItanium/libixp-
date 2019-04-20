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
            exit(1);
        }

    template<typename T>
        constexpr T min(T a, T b) noexcept {
            if (a < b) {
                return a;
            } else {
                return b;
            }
        }


    enum {
        IXP_MAX_VERSION = 32,
        IXP_MAX_MSG = 8192,
        IXP_MAX_ERROR = 128,
        IXP_MAX_CACHE = 32,
        IXP_MAX_FLEN = 128,
        IXP_MAX_ULEN = 32,
        IXP_MAX_WELEM = 16,
    };

    /* 9P message types */
    enum FType {
        P9_TVersion = 100,
        P9_RVersion,
        P9_TAuth = 102,
        P9_RAuth,
        P9_TAttach = 104,
        P9_RAttach,
        P9_TError = 106, /* illegal */
        P9_RError,
        P9_TFlush = 108,
        P9_RFlush,
        P9_TWalk = 110,
        P9_RWalk,
        P9_TOpen = 112,
        P9_ROpen,
        P9_TCreate = 114,
        P9_RCreate,
        P9_TRead = 116,
        P9_RRead,
        P9_TWrite = 118,
        P9_RWrite,
        P9_TClunk = 120,
        P9_RClunk,
        P9_TRemove = 122,
        P9_RRemove,
        P9_TStat = 124,
        P9_RStat,
        P9_TWStat = 126,
        P9_RWStat,
    };

    /* from libc.h in p9p */
    enum OMode {
        P9_OREAD	= 0,	/* open for read */
        P9_OWRITE	= 1,	/* write */
        P9_ORDWR	= 2,	/* read and write */
        P9_OEXEC	= 3,	/* execute, == read but check execute permission */
        P9_OTRUNC	= 16,	/* or'ed in (except for exec), truncate file first */
        P9_OCEXEC	= 32,	/* or'ed in, close on exec */
        P9_ORCLOSE	= 64,	/* or'ed in, remove on close */
        P9_ODIRECT	= 128,	/* or'ed in, direct access */
        P9_ONONBLOCK	= 256,	/* or'ed in, non-blocking call */
        P9_OEXCL	= 0x1000,	/* or'ed in, exclusive use (create only) */
        P9_OLOCK	= 0x2000,	/* or'ed in, lock after opening */
        P9_OAPPEND	= 0x4000	/* or'ed in, append only */
    };

    /* bits in Qid.type */
    enum QType {
        P9_QTDIR	= 0x80,	/* type bit for directories */
        P9_QTAPPEND	= 0x40,	/* type bit for append only files */
        P9_QTEXCL	= 0x20,	/* type bit for exclusive use files */
        P9_QTMOUNT	= 0x10,	/* type bit for mounted channel */
        P9_QTAUTH	= 0x08,	/* type bit for authentication file */
        P9_QTTMP	= 0x04,	/* type bit for non-backed-up file */
        P9_QTSYMLINK	= 0x02,	/* type bit for symbolic link */
        P9_QTFILE	= 0x00	/* type bits for plain file */
    };

    /* bits in Stat.mode */
    enum DMode {
        P9_DMEXEC	= 0x1,		/* mode bit for execute permission */
        P9_DMWRITE	= 0x2,		/* mode bit for write permission */
        P9_DMREAD	= 0x4,		/* mode bit for read permission */

#define P9_DMDIR	0x8000'0000	/* mode bit for directories */
#define P9_DMAPPEND	0x4000'0000	/* mode bit for append only files */
#define P9_DMEXCL	0x2000'0000	/* mode bit for exclusive use files */
#define P9_DMMOUNT	0x1000'0000	/* mode bit for mounted channel */
#define P9_DMAUTH	0x0800'0000	/* mode bit for authentication file */
#define P9_DMTMP	0x0400'0000	/* mode bit for non-backed-up file */
#define P9_DMSYMLINK	0x0200'0000	/* mode bit for symbolic link (Unix, 9P2000.u) */
#define P9_DMDEVICE	0x0080'0000	/* mode bit for device file (Unix, 9P2000.u) */
#define P9_DMNAMEDPIPE	0x0020'0000	/* mode bit for named pipe (Unix, 9P2000.u) */
#define P9_DMSOCKET	0x0010'0000	/* mode bit for socket (Unix, 9P2000.u) */
#define P9_DMSETUID	0x0008'0000	/* mode bit for setuid (Unix, 9P2000.u) */
#define P9_DMSETGID	0x0004'0000	/* mode bit for setgid (Unix, 9P2000.u) */
    };

#ifdef IXP_NO_P9_
#  define TVersion P9_TVersion
#  define RVersion P9_RVersion
#  define TAuth P9_TAuth
#  define RAuth P9_RAuth
#  define TAttach P9_TAttach
#  define RAttach P9_RAttach
#  define TError P9_TError
#  define RError P9_RError
#  define TFlush P9_TFlush
#  define RFlush P9_RFlush
#  define TWalk P9_TWalk
#  define RWalk P9_RWalk
#  define TOpen P9_TOpen
#  define ROpen P9_ROpen
#  define TCreate P9_TCreate
#  define RCreate P9_RCreate
#  define TRead P9_TRead
#  define RRead P9_RRead
#  define TWrite P9_TWrite
#  define RWrite P9_RWrite
#  define TClunk P9_TClunk
#  define RClunk P9_RClunk
#  define TRemove P9_TRemove
#  define RRemove P9_RRemove
#  define TStat P9_TStat
#  define RStat P9_RStat
#  define TWStat P9_TWStat
#  define RWStat P9_RWStat
#
#  define OREAD P9_OREAD
#  define OWRITE P9_OWRITE
#  define ORDWR P9_ORDWR
#  define OEXEC P9_OEXEC
#  define OTRUNC P9_OTRUNC
#  define OCEXEC P9_OCEXEC
#  define ORCLOSE P9_ORCLOSE
#  define ODIRECT P9_ODIRECT
#  define ONONBLOCK P9_ONONBLOCK
#  define OEXCL P9_OEXCL
#  define OLOCK P9_OLOCK
#  define OAPPEND P9_OAPPEND
#
#  define QTDIR P9_QTDIR
#  define QTAPPEND P9_QTAPPEND
#  define QTEXCL P9_QTEXCL
#  define QTMOUNT P9_QTMOUNT
#  define QTAUTH P9_QTAUTH
#  define QTTMP P9_QTTMP
#  define QTSYMLINK P9_QTSYMLINK
#  define QTFILE P9_QTFILE
#  define DMDIR P9_DMDIR
#  define DMAPPEND P9_DMAPPEND
#  define DMEXCL P9_DMEXCL
#  define DMMOUNT P9_DMMOUNT
#  define DMAUTH P9_DMAUTH
#  define DMTMP P9_DMTMP
#
#  define DMSYMLINK P9_DMSYMLINK
#  define DMDEVICE P9_DMDEVICE
#  define DMNAMEDPIPE P9_DMNAMEDPIPE
#  define DMSOCKET P9_DMSOCKET
#  define DMSETUID P9_DMSETUID
#  define DMSETGID P9_DMSETGID
#endif

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
        IXP_ERRMAX = IXP_MAX_ERROR,
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
        uint8_t		type;
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
        char*		wname[IXP_MAX_WELEM];
    };
    struct FRWalk {
        FHdr		hdr;
        uint16_t	nwqid;
        Qid		wqid[IXP_MAX_WELEM];
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
    };

#ifdef IXP_P9_STRUCTS
    using Fcall = Fcall;
    using Fid = Fid;
    using Qid = Qid;
    using Stat = Stat;
#endif

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
        inline CFid* open(const std::string& str, uint8_t val) { return open(str.c_str(), val); }
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
