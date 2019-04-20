#ifndef LIBIXP_SRVUTIL_H__
#define LIBIXP_SRVUTIL_H__

#include <functional>
namespace ixp {
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

enum {
	FLHide = 1,
};

bool	pending_clunk(Req9*);
void	pending_flush(Req9*);
int	pending_print(Pending*, const char*, ...);
void	pending_pushfid(Pending*, Fid*);
void	pending_respond(Req9*);
int	ixp_pending_vprint(Pending*, const char*, va_list ap);
void	ixp_pending_write(Pending*, const char*, long);
FileId*	ixp_srv_clonefiles(FileId*);
void	ixp_srv_data2cstring(Req9*);
void	ixp_srv_freefile(FileId*);
void	ixp_srv_readbuf(Req9*, char*, uint);
void	ixp_srv_readdir(Req9*, LookupFn, void (*)(Stat*, FileId*));
bool	ixp_srv_verifyfile(FileId*, LookupFn);
void	ixp_srv_walkandclone(Req9*, LookupFn);
void	ixp_srv_writebuf(Req9*, char**, uint*, uint);
char*	ixp_srv_writectl(Req9*, char* (*)(void*, Msg*));
FileId* ixp_srv_getfile(void);

} // end namespace ixp
#endif

