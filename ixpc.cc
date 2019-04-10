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

/* Temporary */
#define fatal(...) ixp_eprint("ixpc: fatal: " __VA_ARGS__); \

static IxpClient *client;
static void
usage(void) {
    ixp::errorPrint("usage: ", argv0, " [-a <address>] {create | read | ls [-ld] | remove | write | append} <file>\n"
                "       ", argv0, " [-a <address>] xwrite <file> <data>\n"
                "       ", argv0, " -v\n");
	exit(1);
}

/* Utility Functions */
static void
write_data(IxpCFid *fid, char *name) {
	void *buf;
	long len;

	buf = emalloc(fid->iounit);
	do {
		len = read(0, buf, fid->iounit);
		if(len >= 0 && ixp_write(fid, buf, len) != len) {
            ixp::fatalPrint("cannot write file '", name, "': ", ixp_errbuf(), "\n");
        }
	} while(len > 0);

	free(buf);
}

static int
comp_stat(const void *s1, const void *s2) {
	Stat *st1, *st2;

	st1 = (Stat*)s1;
	st2 = (Stat*)s2;
	return strcmp(st1->name, st2->name);
}

const std::string&
setrwx(long m) {
    static std::vector<std::string> modes = {
		"---", "--x", "-w-",
		"-wx", "r--", "r-x",
		"rw-", "rwx",
	};
    return modes[m];
}

std::string
str_of_mode(uint mode) {
    std::stringstream buf;
    ixp::print(buf, (mode & P9_DMDIR ? 'd' : '-'),
            '-', 
            setrwx((mode >> 6) & 7),
            setrwx((mode >> 3) & 7),
            setrwx((mode >> 0) & 7));
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

static void
print_stat(Stat *s, int details) {
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
static int
xappend(int argc, char *argv[]) {
	IxpCFid *fid;
	IxpStat *stat;
	char *file;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());
	fid = ixp_open(client, file, P9_OWRITE);
	if(fid == nil) {
        ixp::fatalPrint("Can't open file '", file, "': ", ixp_errbuf(), "\n");
    }
	
	stat = ixp_stat(client, file);
	fid->offset = stat->length;
	ixp_freestat(stat);
	free(stat);
	write_data(fid, file);
	return 0;
}

static int
xwrite(int argc, char *argv[]) {
	IxpCFid *fid;
	char *file;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());
	fid = ixp_open(client, file, P9_OWRITE);
	if(fid == nil) {
        ixp::fatalPrint("Can't open file '", file, "': ", ixp_errbuf(), "\n");
    }

	write_data(fid, file);
	return 0;
}

static int
xawrite(int argc, char *argv[]) {
	IxpCFid *fid;
	char *file, *buf, *arg;
	int nbuf, mbuf, len;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());
	fid = ixp_open(client, file, P9_OWRITE);
	if(fid == nil) {
        ixp::fatalPrint("Can't open file '", file, "': ", ixp_errbuf(), "\n");
    }

	nbuf = 0;
	mbuf = 128;
	buf = (decltype(buf))emalloc(mbuf);
	while(argc) {
		arg = ARGF();
		len = strlen(arg);
		if(nbuf + len > mbuf) {
			mbuf <<= 1;
			buf = (decltype(buf))ixp_erealloc(buf, mbuf);
		}
		memcpy(buf+nbuf, arg, len);
		nbuf += len;
		if(argc)
			buf[nbuf++] = ' ';
	}

	if(ixp_write(fid, buf, nbuf) == -1) {
        ixp::fatalPrint("cannot write file '", file, "': ", ixp_errbuf(), "\n");
    }
	return 0;
}

static int
xcreate(int argc, char *argv[]) {
	IxpCFid *fid;
	char *file;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());
	fid = ixp_create(client, file, 0777, P9_OWRITE);
	if(fid == nil) {
        ixp::fatalPrint("Can't create file '", file, "': ", ixp_errbuf(), "\n");
    }

	if((fid->qid.type&P9_DMDIR) == 0)
		write_data(fid, file);

	return 0;
}

static int
xremove(int argc, char *argv[]) {
	char *file;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());
	if(ixp_remove(client, file) == 0) {
        ixp::fatalPrint("Can't remove file '", file, "': ", ixp_errbuf(), "\n");
    }
	return 0;
}

static int
xread(int argc, char *argv[]) {
	IxpCFid *fid;
	char *file, *buf;
	int count;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());
	fid = ixp_open(client, file, P9_OREAD);
	if(fid == nil) {
        ixp::fatalPrint("Can't open file '", file, "': ", ixp_errbuf(), "\n");
    }

	buf = (decltype(buf))emalloc(fid->iounit);
	while((count = ixp_read(fid, buf, fid->iounit)) > 0)
		write(1, buf, count);

	if(count == -1) {
        ixp::fatalPrint("cannot read file/directory '", file, "': ", ixp_errbuf(), "\n");
    }

	return 0;
}

static int
xls(int argc, char *argv[]) {
	IxpMsg m;
	Stat *stat;
	IxpCFid *fid;
	char *file, *buf;
	int lflag, dflag, count, nstat, mstat, i;

	lflag = dflag = 0;

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

	stat = ixp_stat(client, file);
	if(stat == nil)
        ixp::fatalPrint("Can't stat file '", file, "': ", ixp_errbuf(), "\n");

	if(dflag || (stat->mode&P9_DMDIR) == 0) {
		print_stat(stat, lflag);
		ixp_freestat(stat);
		return 0;
	}
	ixp_freestat(stat);

	fid = ixp_open(client, file, P9_OREAD);
	if(fid == nil)
        ixp::fatalPrint("Can't open file '", file, "': ", ixp_errbuf(), "\n");

	nstat = 0;
	mstat = 16;
	stat = (decltype(stat))emalloc(sizeof(*stat) * mstat);
	buf = (decltype(buf))emalloc(fid->iounit);
	while((count = ixp_read(fid, buf, fid->iounit)) > 0) {
		m = ixp_message(buf, count, MsgUnpack);
		while(m.pos < m.end) {
			if(nstat == mstat) {
				mstat <<= 1;
				stat = (decltype(stat))ixp_erealloc(stat, sizeof(*stat) * mstat);
			}
			ixp_pstat(&m, &stat[nstat++]);
		}
	}

	qsort(stat, nstat, sizeof(*stat), comp_stat);
	for(i = 0; i < nstat; i++) {
		print_stat(&stat[i], lflag);
		ixp_freestat(&stat[i]);
	}
	free(stat);

	if(count == -1)
        ixp::fatalPrint("Can't read directory '", file, "': ", ixp_errbuf(), "\n");
	return 0;
}

std::map<std::string, std::function<int(int, char**)>> etab = {
	{"append", xappend},
	{"write", xwrite},
	{"xwrite", xawrite},
	{"read", xread},
	{"create", xcreate},
	{"remove", xremove},
	{"ls", xls},
};

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

	if(!address)
		ixp::fatalPrint("$IXP_ADDRESS not set\n");

	client = ixp_mount(address);
	if(client == nil)
        ixp::fatalPrint(ixp_errbuf(), "\n");

    bool foundCommand = false;
    int ret = 0;
    for (const auto& tab : etab) {
        if (cmd == tab.first) {
            foundCommand = true;
            ret = tab.second(argc, argv);
            break;
        }
    }
    if (!foundCommand) {
        usage();
    }
	ixp_unmount(client);
	return ret;
}
