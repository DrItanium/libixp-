/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#define IXP_NO_P9_
#define IXP_P9_STRUCTS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ixp_local.h>
#include <string>
#include <list>
#include <functional>
#include <string>
#include <list>
#include <map>


namespace {
ixp::Client *client;

void
usage(int errorCode = 1) {
    ixp::errorPrint("usage: ", argv0, " [-a <address>] {create | read | ls [-ld] | remove | write | append} <file>\n"
                "       ", argv0, " [-a <address>] xwrite <file> <data>\n"
                "       ", argv0, " -v\n");
	exit(errorCode);
}

/* Utility Functions */
void
write_data(ixp::CFid *fid, char *name) {
	long len = 0;

	auto buf = ixp::emalloc(fid->iounit);
	do {
		len = read(0, buf, fid->iounit);
		if(len >= 0 && fid->write(buf, len) != len) {
            ixp::fatalPrint("cannot write file '", name, "': ", ixp::errbuf(), "\n");
        }
	} while(len > 0);

	free(buf);
}

int
comp_stat(const void *s1, const void *s2) {
	auto st1 = (ixp::Stat*)s1;
	auto st2 = (ixp::Stat*)s2;
	return strcmp(st1->name, st2->name);
}


std::string
str_of_mode(uint mode) {
    static std::vector<std::string> modes = {
		"---", "--x", "-w-",
		"-wx", "r--", "r-x",
		"rw-", "rwx",
	};
    std::stringstream buf;
    ixp::print(buf, (mode & P9_DMDIR ? 'd' : '-'),
            '-', 
            modes[(mode >> 6) & 0b111],
            modes[(mode >> 3) & 0b111],
            modes[(mode >> 0) & 0b111]);
    auto str = buf.str();
    return str;
}

std::string
str_of_time(uint val) {
	static char buf[32];
	ctime_r((time_t*)&val, buf);
	buf[strlen(buf) - 1] = '\0';
    return std::string(buf);
}

void
print_stat(ixp::Stat *s, int details) {
	if(details) {
        ixp::print(std::cout, str_of_mode(s->mode), " ", 
                s->uid, " ", s->gid, " ", s->length, " ", 
                str_of_time(s->mtime), " ", s->name, "\n");
    } else {
		if((s->mode&P9_DMDIR) && strcmp(s->name, "/")) {
            ixp::print(std::cout, s->name, "/\n");
        } else {
            ixp::print(std::cout, s->name, "\n");
        }
	}
}

/* Service Functions */
using ServiceFunction = std::function<int(int, char**)>;
int
xappend(int argc, char *argv[]) {
	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	auto file = EARGF(usage());
    auto fid = client->open(file, ixp::P9_OWRITE);
    if (!fid) {
        ixp::fatalPrint("Can't open file '", file, "': ", ixp::errbuf(), "\n");
    }
	
	auto stat = client->stat(file);
	fid->offset = stat->length;
    ixp::Stat::free(stat);
	free(stat);
	write_data(fid, file);
	return 0;
}

static int
xwrite(int argc, char *argv[]) {
	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	auto file = EARGF(usage());
    auto fid = client->open(file, ixp::P9_OWRITE);
    if (!fid) {
        ixp::fatalPrint("Can't open file '", file, "': ", ixp::errbuf(), "\n");
    }

	write_data(fid, file);
	return 0;
}

int
xawrite(int argc, char *argv[]) {
	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	auto file = EARGF(usage());
    auto fid = client->open(file, ixp::P9_OWRITE);
    if (!fid) {
        ixp::fatalPrint("Can't open file '", file, "': ", ixp::errbuf(), "\n");
    }

	auto nbuf = 0;
	auto mbuf = 128;
	auto buf = (char*)ixp::emalloc(mbuf);
	while(argc) {
		auto arg = ARGF();
		int len = strlen(arg);
		if(nbuf + len > mbuf) {
			mbuf <<= 1;
			buf = (decltype(buf))ixp::erealloc(buf, mbuf);
		}
		memcpy(buf+nbuf, arg, len);
		nbuf += len;
		if(argc)
			buf[nbuf++] = ' ';
	}

	if(fid->write(buf, nbuf) == -1) {
        ixp::fatalPrint("cannot write file '", file, "': ", ixp::errbuf(), "\n");
    }
	return 0;
}

int
xcreate(int argc, char *argv[]) {
	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	auto file = EARGF(usage());
    auto fid = client->create(file, 0777, ixp::P9_OWRITE);
    if (!fid) {
        ixp::fatalPrint("Can't create file '", file, "': ", ixp::errbuf(), "\n");
    }

	if((fid->qid.type&P9_DMDIR) == 0)
		write_data(fid, file);

	return 0;
}

int
xremove(int argc, char *argv[]) {
	ARGBEGIN{
	default:
		usage();
	}ARGEND;

    if (auto file = EARGF(usage()); !client->remove(file)) {
        ixp::fatalPrint("Can't remove file '", file, "': ", ixp::errbuf(), "\n");
    }
	return 0;
}

int
xread(int argc, char *argv[]) {
	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	auto file = EARGF(usage());
    auto fid = client->open(file, ixp::P9_OREAD);
    if (!fid) {
        ixp::fatalPrint("Can't open file '", file, "': ", ixp::errbuf(), "\n");
    }

    int count = 0;
	auto buf = (char*)ixp::emalloc(fid->iounit);
	while((count = fid->read(buf, fid->iounit)) > 0) {
		write(1, buf, count);
    }

	if(count == -1) {
        ixp::fatalPrint("cannot read file/directory '", file, "': ", ixp::errbuf(), "\n");
    }

	return 0;
}

int
xls(int argc, char *argv[]) {
	ixp::Msg m;
	ixp::CFid *fid;
	char *file, *buf;
	int count, nstat, mstat, i;

	auto lflag = 0;
    auto dflag = 0;

	ARGBEGIN{
	case 'l':
		lflag++;
		break;
	case 'd':
		dflag++;
		break;
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());

    auto stat = client->stat(file);
    if (!stat) {
        ixp::fatalPrint("Can't stat file '", file, "': ", ixp::errbuf(), "\n");
    }

	if(dflag || (stat->mode&P9_DMDIR) == 0) {
		print_stat(stat, lflag);
        ixp::Stat::free(stat);
		return 0;
	}
    ixp::Stat::free(stat);

    fid = client->open(file, ixp::P9_OREAD);
	if(!fid) {
        ixp::fatalPrint("Can't open file '", file, "': ", ixp::errbuf(), "\n");
    }

	nstat = 0;
	mstat = 16;
	stat = (decltype(stat))ixp::emalloc(sizeof(*stat) * mstat);
	buf = (decltype(buf))ixp::emalloc(fid->iounit);
	while((count = fid->read(buf, fid->iounit)) > 0) {
        m = ixp::Msg::message(buf, count, ixp::Msg::Unpack);
		while(m.pos < m.end) {
			if(nstat == mstat) {
				mstat <<= 1;
				stat = (decltype(stat))ixp::erealloc(stat, sizeof(*stat) * mstat);
			}
            m.pstat(&stat[nstat++]);
		}
	}

	qsort(stat, nstat, sizeof(*stat), comp_stat);
	for(i = 0; i < nstat; i++) {
		print_stat(&stat[i], lflag);
        ixp::Stat::free(&stat[i]);
	}
	free(stat);

	if(count == -1)
        ixp::fatalPrint("Can't read directory '", file, "': ", ixp::errbuf(), "\n");
	return 0;
}

std::map<std::string, ServiceFunction> etab = {
	{"append", xappend},
	{"write", xwrite},
	{"xwrite", xawrite},
	{"read", xread},
	{"create", xcreate},
	{"remove", xremove},
	{"ls", xls},
};
}

int
main(int argc, char *argv[]) {

	auto address = getenv("IXP_ADDRESS");
	ARGBEGIN{
	case 'v':
        ixp::print(std::cout, argv0, "-", VERSION, ", ", COPYRIGHT, "\n");
		exit(0);
	case 'a':
		address = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND;

    std::string cmd = EARGF(usage());

	if(!address) {
		ixp::fatalPrint("$IXP_ADDRESS not set\n");
    }

    if (client = ixp::Client::mount(address); !client) {
        ixp::fatalPrint(ixp::errbuf(), "\n");
    } else {
        if (auto result = etab.find(cmd); result != etab.end()) {
            auto ret = result->second(argc, argv);
            ixp::Client::unmount(client);
            return ret;
        } else {
            usage();
            return 99;
        }
    }
}
