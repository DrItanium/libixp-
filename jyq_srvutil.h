#ifndef LIBJYQ_SRVUTIL_H__
#define LIBJYQ_SRVUTIL_H__

#include <functional>
#include <list>
#include <string>
#include "jyq.h"
namespace jyq {
struct Pending;
using Queue = std::list<std::string>;
using FileIdU = void*;


struct PendingLinkBody {
	/* Private members */
	Fid*		fid;
    Queue queue;
	Pending*	pending;
};
using RawPendingLink = DoubleLinkedListNode<PendingLinkBody>;
using PendingLink = std::shared_ptr<RawPendingLink>;

struct RequestLinkBody {
	/* Private members */
	Req9*	req;
};
using RawRequestLink = DoubleLinkedListNode<RequestLinkBody>;
using RequestLink = std::shared_ptr<RawRequestLink>;

struct Pending {
    void	pushfid(Fid*);
    void	write(const char*, long);
    void    write(const std::string&); 
    template<typename ... Args>
    void print(Args&& ... values) {
        std::stringstream str;
        jyq::print(str, std::forward(values)...);
        auto result = str.str();
        write(result);
    }
    template<typename ... Args>
    void vprint(Args&& ... values) {
        std::stringstream str;
        jyq::print(str, std::forward(values)...);
        auto result = str.str();
        write(result);
    }
    RequestLink	req;
    PendingLink	fids;
};

struct Dirtab {
    std::string name;
	uint8_t	qtype;
	uint	type;
	uint	perm;
	uint	flags;
};

struct FileIdBody {
	void* p; // this needs to be here for a special purpose
	bool		pending;
	uint		id;
	uint		index;
	Dirtab      tab;
	uint		nref;
	char		volatil;
};
using RawFileId = SingleLinkedListNode<FileIdBody>;
using FileId = std::shared_ptr<RawFileId>;
using LookupFn = std::function<FileId(FileId, const std::string&)>;

constexpr auto FLHide = 1;

bool	pending_clunk(Req9*);
void	pending_flush(Req9*);
void	pending_respond(Req9*);
FileId	srv_clonefiles(FileId&);
void	srv_data2cstring(Req9*);
void	srv_freefile(FileId&);
void	srv_readbuf(Req9*, char*, uint);
void	srv_readdir(Req9*, LookupFn, std::function<void(Stat*, FileId&)>);
bool	srv_verifyfile(FileId&, LookupFn);
void	srv_walkandclone(Req9*, LookupFn);
void	srv_writebuf(Req9*, char**, uint*, uint);
char*	srv_writectl(Req9*, std::function<char*(void*, Msg*)>);
FileId srv_getfile(void);

} // end namespace jyq
#endif

