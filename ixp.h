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

namespace ixp {
    using uint = unsigned int;
    constexpr auto ApiVersion = 135;
    constexpr auto Version = "9P2000";
    constexpr auto NoTag = uint16_t(~0); 
    constexpr auto NoFid = ~0u;
#define STRUCT(x) __extension__ struct {x};
#define UNION(x) __extension__ union {x};
#define IXP_NOTAG	ixp::NoTag /* Dummy tag */
#define IXP_NOFID	ixp::NoFid
#define IXP_VERSION ixp::Version
#define IXP_API ixp::ApiVersion
    class FileHeader {
        public:
            FileHeader() = default;
            ~FileHeader() = default;
            FileHeader(const FileHeader&) = default;
            FileHeader(FileHeader&&) = default;
            constexpr auto getType() const noexcept { return _type; }
            void setType(uint8_t type) noexcept { _type = type; }
            constexpr auto getTag() const noexcept { return _tag; }
            void setTag(uint16_t tag) noexcept { _tag = tag; }
            constexpr auto getFid() const noexcept { return _fid; }
            void setFid(uint32_t fid) noexcept { _fid = fid; }
        private:
            uint8_t _type;
            uint16_t _tag;
            uint32_t _fid;
    };

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

} // end namespace ixp

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
enum IxpFType {
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
enum IxpOMode {
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

/* bits in IxpQid.type */
enum IxpQType {
	P9_QTDIR	= 0x80,	/* type bit for directories */
	P9_QTAPPEND	= 0x40,	/* type bit for append only files */
	P9_QTEXCL	= 0x20,	/* type bit for exclusive use files */
	P9_QTMOUNT	= 0x10,	/* type bit for mounted channel */
	P9_QTAUTH	= 0x08,	/* type bit for authentication file */
	P9_QTTMP	= 0x04,	/* type bit for non-backed-up file */
	P9_QTSYMLINK	= 0x02,	/* type bit for symbolic link */
	P9_QTFILE	= 0x00	/* type bits for plain file */
};

/* bits in IxpStat.mode */
enum IxpDMode {
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

struct IxpMap;
struct Ixp9Conn;
struct Ixp9Req;
struct Ixp9Srv;
struct IxpCFid;
struct IxpClient;
struct IxpConn;
struct IxpFid;
struct IxpMsg;
struct IxpQid;
struct IxpRpc;
struct IxpServer;
struct IxpStat;
struct IxpTimer;

struct IxpMutex;
struct IxpRWLock;
struct IxpRendez;

/* Threading */
enum {
	IXP_ERRMAX = IXP_MAX_ERROR,
};

struct IxpMutex {
	void*	aux;
};

struct IxpRendez {
	IxpMutex*	mutex;
	void*	aux;
};

struct IxpRWLock {
	void*	aux;
};

enum IxpMsgMode {
	MsgPack,
	MsgUnpack,
};
struct IxpMsg {
	char*	data; /* Begining of buffer. */
	char*	pos;  /* Current position in buffer. */
	char*	end;  /* End of message. */ 
	uint	size; /* Size of buffer. */
	uint	mode; /* MsgPack or MsgUnpack. */
};

struct IxpQid {
	uint8_t		type;
	uint32_t	version;
	uint64_t	path;
	/* Private members */
	uint8_t		dir_type;
};

/* stat structure */
struct IxpStat {
	uint16_t	type;
	uint32_t	dev;
	IxpQid		qid;
	uint32_t	mode;
	uint32_t	atime;
	uint32_t	mtime;
	uint64_t	length;
	char*	name;
	char*	uid;
	char*	gid;
	char*	muid;
};

struct IxpFHdr;
struct IxpFError;
struct IxpFROpen;
struct IxpFRAuth;
struct IxpFROpen;
struct IxpFROpen;
struct IxpFIO;
struct IxpFRStat;
struct IxpFVersion;
struct IxpFRWalk;
struct IxpFAttach;
struct IxpFAttach;
struct IxpFTCreate;
struct IxpFTFlush;
struct IxpFTCreate;
struct IxpFIO;
struct IxpFVersion;
struct IxpFTWalk;
struct IxpFIO;
struct IxpFTWStat;
struct IxpFAttach;
struct IxpFIO;
struct IxpFVersion;

struct IxpFHdr {
	uint8_t		type;
	uint16_t	tag;
	uint32_t	fid;
};
struct IxpFVersion {
	IxpFHdr		hdr;
	uint32_t	msize;
	char*		version;
};
struct IxpFTFlush {
	IxpFHdr		hdr;
	uint16_t	oldtag;
};
struct IxpFError {
	IxpFHdr		hdr;
	char*		ename;
};
struct IxpFROpen {
	IxpFHdr		hdr;
	IxpQid		qid; /* +Rattach */
	uint32_t	iounit;
};
struct IxpFRAuth {
	IxpFHdr		hdr;
	IxpQid		aqid;
};
struct IxpFAttach {
	IxpFHdr		hdr;
	uint32_t	afid;
	char*		uname;
	char*		aname;
};
struct IxpFTCreate {
	IxpFHdr		hdr;
	uint32_t	perm;
	char*		name;
	uint8_t		mode; /* +Topen */
};
struct IxpFTWalk {
	IxpFHdr	hdr;
	uint32_t	newfid;
	uint16_t	nwname;
	char*		wname[IXP_MAX_WELEM];
};
struct IxpFRWalk {
	IxpFHdr		hdr;
	uint16_t	nwqid;
	IxpQid		wqid[IXP_MAX_WELEM];
};
struct IxpFIO {
	IxpFHdr		hdr;
	uint64_t	offset; /* Tread, Twrite */
	uint32_t	count; /* Tread, Twrite, Rread */
	char*		data; /* Twrite, Rread */
};
struct IxpFRStat {
	IxpFHdr		hdr;
	uint16_t	nstat;
	uint8_t*	stat;
};
struct IxpFTWStat {
	IxpFHdr		hdr;
	IxpStat		stat;
};

/**
 * Type: IxpFcall
 * Type: IxpFType
 * Type: IxpFAttach
 * Type: IxpFError
 * Type: IxpFHdr
 * Type: IxpFIO
 * Type: IxpFRAuth
 * Type: IxpFROpen
 * Type: IxpFRStat
 * Type: IxpFRWalk
 * Type: IxpFTCreate
 * Type: IxpFTFlush
 * Type: IxpFTWStat
 * Type: IxpFTWalk
 * Type: IxpFVersion
 *
 * The IxpFcall structure represents a 9P protocol message. The
 * P<hdr> element is common to all Fcall types, and may be used to
 * determine the type and tag of the message. The IxpFcall type is
 * used heavily in server applications, where it both presents a
 * request to handler functions and returns a response to the
 * client.
 *
 * Each member of the IxpFcall structure represents a certain
 * message type, which can be discerned from the P<hdr.type> field.
 * This value corresponds to one of the IxpFType constants. Types
 * with significant overlap use the same structures, thus TRead and
 * RWrite are both represented by IxpFIO and can be accessed via the
 * P<io> member as well as P<tread> and P<rwrite> respectively.
 *
 * See also:
 *	T<Ixp9Srv>, T<Ixp9Req>
 */
union IxpFcall {
	IxpFHdr		hdr;
	IxpFVersion	version;
	IxpFVersion	tversion;
	IxpFVersion	rversion;
	IxpFTFlush	tflush;
	IxpFROpen	ropen;
	IxpFROpen	rcreate;
	IxpFROpen	rattach;
	IxpFError	error;
	IxpFRAuth	rauth;
	IxpFAttach	tattach;
	IxpFAttach	tauth;
	IxpFTCreate	tcreate;
	IxpFTCreate	topen;
	IxpFTWalk	twalk;
	IxpFRWalk	rwalk;
	IxpFTWStat	twstat;
	IxpFRStat	rstat;
	IxpFIO		twrite;
	IxpFIO		rwrite;
	IxpFIO		tread;
	IxpFIO		rread;
	IxpFIO		io;
};

#ifdef IXP_P9_STRUCTS
typedef IxpFcall	Fcall;
typedef IxpFid		Fid;
typedef IxpQid		Qid;
typedef IxpStat		Stat;
#endif

struct IxpConn {
	IxpServer*	srv;
	void*		aux;	/* Arbitrary pointer, to be used by handlers. */
	int		fd;	/* The file descriptor of the connection. */
    std::function<void(IxpConn*)> read, close;
	char		closed;	/* Non-zero when P<fd> has been closed. */

	/* Private members */
	IxpConn		*next;
};

struct IxpServer {
	IxpConn*	conn;
	IxpMutex	lk;
	IxpTimer*	timer;
    std::function<void(IxpServer*)> preselect;
	void*		aux;
	bool	running;
	int		maxfd;
	fd_set		rd;
};

struct IxpRpc {
	IxpClient*	mux;
	IxpRpc*		next;
	IxpRpc*		prev;
	IxpRendez	r;
	uint		tag;
	IxpFcall*	p;
	int		waiting;
	int		async;
};

struct IxpClient {
	int	fd;
	uint	msize;
	uint	lastfid;

	/* Private members */
	uint		nwait;
	uint		mwait;
	uint		freetag;
	IxpCFid*	freefid;
	IxpMsg		rmsg;
	IxpMsg		wmsg;
	IxpMutex	lk;
	IxpMutex	rlock;
	IxpMutex	wlock;
	IxpRendez	tagrend;
	IxpRpc**	wait;
	IxpRpc*		muxer;
	IxpRpc		sleep;
	int		mintag;
	int		maxtag;
};

struct IxpCFid {
	uint32_t	fid;
	IxpQid		qid;
	uint8_t		mode;
	uint		open;
	uint		iounit;
	uint32_t	offset;
	IxpClient*	client;

	/* Private members */
	IxpCFid*	next;
	IxpMutex	iolock;
};

/**
 * Type: IxpFid
 *
 * Represents an open file for a 9P connection. The same
 * structure persists as long as the file remains open, and is
 * installed in the T<Ixp9Req> structure for any request Fcall
 * which references it. Handlers may use the P<aux> member to
 * store any data which must persist for the life of the open
 * file.
 *
 * See also:
 *	T<Ixp9Req>, T<IxpQid>, T<IxpOMode>
 */
struct IxpFid {
	char*		uid;	/* The uid of the file opener. */
	void*		aux;    /* Arbitrary pointer, to be used by handlers. */
	uint32_t		fid;    /* The ID number of the fid. */
	IxpQid		qid;    /* The filesystem-unique QID of the file. */
	signed char	omode;  /* The open mode of the file. */
	uint		iounit; /* The maximum size of any IO request. */

	/* Private members */
	Ixp9Conn*	conn;
	IxpMap*		map;
};

struct Ixp9Req {
	Ixp9Srv*	srv;
	IxpFid*		fid;    /* Fid structure corresponding to IxpFHdr.fid */
	IxpFid*		newfid; /* Corresponds to IxpFTWStat.newfid */
	Ixp9Req*	oldreq; /* For TFlush requests, the original request. */
	IxpFcall	ifcall; /* The incoming request fcall. */
	IxpFcall	ofcall; /* The response fcall, to be filled by handler. */
	void*		aux;    /* Arbitrary pointer, to be used by handlers. */

	/* Private members */
	Ixp9Conn *conn;
};

struct Ixp9Srv {
	void* aux;
    using ReqFunc = std::function<void(Ixp9Req*)>;
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
    std::function<void(IxpFid*)> freefid;
};

namespace ixp::concurrency {
/**
 * Type: IxpThread
 * Type: IxpMutex
 * Type: IxpRWLock
 * Type: IxpRendez
 * Variable: ixp_thread
 *
 * The IxpThread structure is used to adapt libixp to any of the
 * myriad threading systems it may be used with. Before any
 * other of libixp's functions is called, ixp_thread may be set
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
 *	F<ixp_pthread_init>, F<ixp_taskinit>, F<ixp_rubyinit>
 */
class ThreadImpl {
    public:
        ThreadImpl() = default;
        virtual ~ThreadImpl() = default;
        /* Read/write lock */
        virtual bool init(IxpRWLock*) = 0;
        virtual void rlock(IxpRWLock*) = 0;
        virtual bool canrlock(IxpRWLock*) = 0;
        virtual void runlock(IxpRWLock*) = 0;
        virtual void wlock(IxpRWLock*) = 0;
        virtual bool canwlock(IxpRWLock*) = 0;
        virtual void wunlock(IxpRWLock*) = 0;
        virtual void destroy(IxpRWLock*) = 0;
        /* Mutex */
        virtual bool init(IxpMutex*) = 0;
        virtual bool canlock(IxpMutex*) = 0;
        virtual void lock(IxpMutex*) = 0;
        virtual void unlock(IxpMutex*) = 0;
        virtual void destroy(IxpMutex*) = 0;
        /* Rendezvous point */
        virtual bool init(IxpRendez*) = 0;
        virtual bool wake(IxpRendez*) = 0;
        virtual bool wakeall(IxpRendez*) = 0;
        virtual void sleep(IxpRendez*) = 0;
        virtual void destroy(IxpRendez*) = 0;
        /* Other */
        virtual char* errbuf() = 0;
        virtual ssize_t read(int, void*, size_t);
        virtual ssize_t write(int, const void*, size_t);
        virtual int select(int, fd_set*, fd_set*, fd_set*, timeval*);
        // interface with old design
        auto initmutex(IxpMutex* m) { return init(m); }
        auto initrwlock(IxpRWLock* a) { return init(a); }
        void mdestroy(IxpMutex* m) { destroy(m); }
        void rwdestroy(IxpRWLock* a) { destroy(a); }
        auto initrendez(IxpRendez* r) { return init(r); }
        void rdestroy(IxpRendez* r) { destroy(r); }

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
         bool init(IxpRWLock*) override { return false; }
         void rlock(IxpRWLock*) override { }
         bool canrlock(IxpRWLock*) override { return true; }
         void runlock(IxpRWLock*) override { }
         void wlock(IxpRWLock*) override { }
         bool canwlock(IxpRWLock*) override { return true; }
         void wunlock(IxpRWLock*) override { }
         void destroy(IxpRWLock*) override { }
        /* Mutex */
         bool init(IxpMutex*) override { return false; }
         bool canlock(IxpMutex*) override { return true; }
         void lock(IxpMutex*) override { }
         void unlock(IxpMutex*) override { }
         void destroy(IxpMutex*) override { }
        /* Rendezvous point */
         bool init(IxpRendez*) override { return false; }
         bool wake(IxpRendez*) override { return false; }
         bool wakeall(IxpRendez*) override { return false; }
         void sleep(IxpRendez*) override;
         void destroy(IxpRendez*) override { }
        /* Other */
         char* errbuf() override;
};
} // end namespace ixp

namespace ixp
{
    extern std::function<int(char*, int, const char*, va_list)> vsnprint;
    extern std::function<char*(const char*, va_list)> vsmprint;
    extern std::function<void(IxpFcall*)> printfcall;
} // end namespace ixp
#define ixp_printfcall ixp::printfcall
#define ixp_vsnprint ixp::vsnprint
#define ixp_vsmprint ixp::vsmprint

/* client.c */
long	ixp_pread(IxpCFid*, void*, long, int64_t);
int	ixp_print(IxpCFid*, const char*, ...);
long	ixp_pwrite(IxpCFid*, const void*, long, int64_t);
long	ixp_read(IxpCFid*, void*, long);
namespace ixp {
    bool close(IxpCFid*);
    inline bool remove(IxpClient* client, const std::string& str) noexcept { return remove(client, str.c_str()); }
    bool remove(IxpClient*, const char*);
} // end namespace ixp

#define ixp_remove ixp::remove
#define ixp_close ixp::close

void	ixp_unmount(IxpClient*);
int	ixp_vprint(IxpCFid*, const char*, va_list);
long	ixp_write(IxpCFid*, const void*, long);
IxpCFid*	ixp_create(IxpClient*, const char*, uint perm, uint8_t mode);
IxpStat*	ixp_fstat(IxpCFid*);
IxpClient*	ixp_mount(const char*);
IxpClient*	ixp_mountfd(int);
IxpClient*	ixp_nsmount(const char*);
IxpCFid*	ixp_open(IxpClient*, const char*, uint8_t);
IxpStat*	ixp_stat(IxpClient*, const char*);

/* convert.c */
void ixp_pu8(IxpMsg*, uint8_t*);
void ixp_pu16(IxpMsg*, uint16_t*);
void ixp_pu32(IxpMsg*, uint32_t*);
void ixp_pu64(IxpMsg*, uint64_t*);
void ixp_pdata(IxpMsg*, char**, uint);
void ixp_pstring(IxpMsg*, char**);
void ixp_pstrings(IxpMsg*, uint16_t*, char**, uint);
void ixp_pqid(IxpMsg*, IxpQid*);
void ixp_pqids(IxpMsg*, uint16_t*, IxpQid*, uint);
void ixp_pstat(IxpMsg*, IxpStat*);
void ixp_pfcall(IxpMsg*, IxpFcall*);

/* error.h */
char*	ixp_errbuf(void);
void	ixp_errstr(char*, int);
void	ixp_rerrstr(char*, int);
void	ixp_werrstr(const char*, ...);

/* request.c */
void ixp_respond(Ixp9Req*, const char *err);
void ixp_serve9conn(IxpConn*);

/* message.c */
uint16_t	ixp_sizeof_stat(IxpStat*);
IxpMsg	ixp_message(char*, uint len, uint mode);
void	ixp_freestat(IxpStat*);
void	ixp_freefcall(IxpFcall*);
uint	ixp_msg2fcall(IxpMsg*, IxpFcall*);
uint	ixp_fcall2msg(IxpMsg*, IxpFcall*);

/* server.c */
IxpConn* ixp_listen(IxpServer*, int, void*,
		void (*read)(IxpConn*),
		void (*close)(IxpConn*));
void	ixp_hangup(IxpConn*);
int	    ixp_serverloop(IxpServer*);
void	ixp_server_close(IxpServer*);

/* socket.c */
namespace ixp {
  int dial(const std::string&);
  int announce(const std::string&);
} // end namespace ixp

#define ixp_dial ixp::dial
#define ixp_announce ixp::announce

/* transport.c */
uint ixp_sendmsg(int, IxpMsg*);
uint ixp_recvmsg(int, IxpMsg*);

/* timer.c */
namespace ixp {
    uint64_t msec();
    bool unsettimer(IxpServer*, long);
    long settimer(IxpServer*, long, std::function<void(long, void*)>, void*);
}
#define ixp_msec ixp::msec
#define ixp_unsettimer ixp::unsettimer
#define ixp_settimer ixp::settimer

/* util.c */
void	ixp_cleanname(char*);
void*	ixp_emalloc(uint);
void*	ixp_emallocz(uint);
void	ixp_eprint(const char*, ...);
void*	ixp_erealloc(void*, uint);
char*	ixp_estrdup(const char*);
char*	ixp_namespace(void);
char*	ixp_smprint(const char*, ...);
uint	ixp_strlcat(char*, const char*, uint);
uint	ixp_tokenize(char**, uint len, char*, char);

#endif
