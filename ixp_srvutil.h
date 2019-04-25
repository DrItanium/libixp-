#ifndef LIBJYQ_SRVUTIL_H__
#define LIBJYQ_SRVUTIL_H__

#include <functional>
#include "jyq.h"
namespace jyq {
struct Dirtab;
struct PendingLink;
struct Pending;
struct Queue;
struct RequestLink;
struct FileId;
using FileIdU = void*;
using LookupFn = std::function<FileId*(FileId*, char*)>;

struct PendingLink {
	/* Private members */
	PendingLink*	next;
	PendingLink*	prev;
	Fid*		fid;
	Queue*	queue;
	Pending*	pending;
};

struct RequestLink {
	/* Private members */
	RequestLink*	next;
	RequestLink*	prev;
	Req9*	req;
};

struct Pending {
	/* Private members */
	RequestLink	req;
	PendingLink	fids;
};

struct Dirtab {
	char*	name;
	uint8_t	qtype;
	uint	type;
	uint	perm;
	uint	flags;
};

struct FileId {
	FileId*	next;
	FileIdU	p;
	bool		pending;
	uint		id;
	uint		index;
	Dirtab	tab;
	uint		nref;
	char		volatil;
};

constexpr auto FLHide = 1;

bool	pending_clunk(Req9*);
void	pending_flush(Req9*);
int	pending_print(Pending*, const char*, ...);
void	pending_pushfid(Pending*, Fid*);
void	pending_respond(Req9*);
int	pending_vprint(Pending*, const char*, va_list ap);
void	pending_write(Pending*, const char*, long);
FileId*	srv_clonefiles(FileId*);
void	srv_data2cstring(Req9*);
void	srv_freefile(FileId*);
void	srv_readbuf(Req9*, char*, uint);
void	srv_readdir(Req9*, LookupFn, std::function<void(Stat*, FileId*)>);
bool	srv_verifyfile(FileId*, LookupFn);
void	srv_walkandclone(Req9*, LookupFn);
void	srv_writebuf(Req9*, char**, uint*, uint);
char*	srv_writectl(Req9*, std::function<char*(void*, Msg*)>);
FileId* srv_getfile(void);

} // end namespace jyq
#endif

