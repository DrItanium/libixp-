/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#define JYQ_NO_P9_
#define JYQ_P9_STRUCTS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <jyq.h>
#include <string>
#include <list>
#include <functional>
#include <string>
#include <list>
#include <map>


namespace {
jyq::Client *client;

void
usage(int errorCode = 1) {
    jyq::errorPrint("usage: ", argv0, " [-a <address>] {create | read | ls [-ld] | remove | write | append} <file>\n"
                "       ", argv0, " [-a <address>] xwrite <file> <data>\n"
                "       ", argv0, " -v\n");
	exit(errorCode);
}

/* Utility Functions */
void
write_data(jyq::CFid *fid, char *name) {
	long len = 0;

	auto buf = jyq::emalloc(fid->iounit);
	do {
		len = read(0, buf, fid->iounit);
		if(len >= 0 && fid->write(buf, len) != len) {
            jyq::fatalPrint("cannot write file '", name, "': ", jyq::errbuf(), "\n");
        }
	} while(len > 0);

	free(buf);
}

int
comp_stat(const void *s1, const void *s2) {
	auto st1 = (jyq::Stat*)s1;
	auto st2 = (jyq::Stat*)s2;
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
    jyq::print(buf, (mode & (uint32_t)jyq::DMode::DIR ? 'd' : '-'),
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
print_stat(jyq::Stat *s, int details) {
	if(details) {
        jyq::print(std::cout, str_of_mode(s->mode), " ", 
                s->uid, " ", s->gid, " ", s->length, " ", 
                str_of_time(s->mtime), " ", s->name, "\n");
    } else {
		if((s->mode&(uint32_t)jyq::DMode::DIR) && strcmp(s->name, "/")) {
            jyq::print(std::cout, s->name, "/\n");
        } else {
            jyq::print(std::cout, s->name, "\n");
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
    auto fid = client->open(file, jyq::OMode::WRITE);
    if (!fid) {
        jyq::fatalPrint("Can't open file '", file, "': ", jyq::errbuf(), "\n");
    }
	
	auto stat = client->stat(file);
	fid->offset = stat->length;
    jyq::Stat::free(stat);
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
    auto fid = client->open(file, jyq::OMode::WRITE);
    if (!fid) {
        jyq::fatalPrint("Can't open file '", file, "': ", jyq::errbuf(), "\n");
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
    auto fid = client->open(file, jyq::OMode::WRITE);
    if (!fid) {
        jyq::fatalPrint("Can't open file '", file, "': ", jyq::errbuf(), "\n");
    }

	auto nbuf = 0;
	auto mbuf = 128;
	auto buf = (char*)jyq::emalloc(mbuf);
	while(argc) {
		auto arg = ARGF();
		int len = strlen(arg);
		if(nbuf + len > mbuf) {
			mbuf <<= 1;
			buf = (decltype(buf))jyq::erealloc(buf, mbuf);
		}
		memcpy(buf+nbuf, arg, len);
		nbuf += len;
		if(argc)
			buf[nbuf++] = ' ';
	}

	if(fid->write(buf, nbuf) == -1) {
        jyq::fatalPrint("cannot write file '", file, "': ", jyq::errbuf(), "\n");
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
    auto fid = client->create(file, 0777, jyq::OMode::WRITE);
    if (!fid) {
        jyq::fatalPrint("Can't create file '", file, "': ", jyq::errbuf(), "\n");
    }

	if((fid->qid.type&(uint32_t)jyq::DMode::DIR) == 0)
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
        jyq::fatalPrint("Can't remove file '", file, "': ", jyq::errbuf(), "\n");
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
    auto fid = client->open(file, jyq::OMode::READ);
    if (!fid) {
        jyq::fatalPrint("Can't open file '", file, "': ", jyq::errbuf(), "\n");
    }

    int count = 0;
	auto buf = (char*)jyq::emalloc(fid->iounit);
	while((count = fid->read(buf, fid->iounit)) > 0) {
		write(1, buf, count);
    }

	if(count == -1) {
        jyq::fatalPrint("cannot read file/directory '", file, "': ", jyq::errbuf(), "\n");
    }

	return 0;
}

int
xls(int argc, char *argv[]) {
	jyq::Msg m;
	jyq::CFid *fid;
	char *file, *buf;
	int count;

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
        jyq::fatalPrint("Can't stat file '", file, "': ", jyq::errbuf(), "\n");
    }

	if(dflag || (stat->mode&static_cast<uint32_t>(jyq::DMode::DIR)) == 0) {
		print_stat(stat, lflag);
        jyq::Stat::free(stat);
		return 0;
	}
    jyq::Stat::free(stat);

    fid = client->open(file, jyq::OMode::READ);
	if(!fid) {
        jyq::fatalPrint("Can't open file '", file, "': ", jyq::errbuf(), "\n");
    }

    std::vector<jyq::Stat> stats;
	buf = (decltype(buf))jyq::emalloc(fid->iounit);
	while((count = fid->read(buf, fid->iounit)) > 0) {
        m = jyq::Msg::message(buf, count, jyq::Msg::Mode::Unpack);
		while(m.pos < m.end) {
            stats.emplace_back();
            m.pstat(stats.back());
		}
	}

    // TODO implement sorting in the future
	//qsort(stats, nstat, sizeof(*stat), comp_stat);
    for (auto& stat : stats) {
        print_stat(&stat, lflag);
        jyq::Stat::free(&stat);
    }
	//free(stat);

	if(count == -1)
        jyq::fatalPrint("Can't read directory '", file, "': ", jyq::errbuf(), "\n");
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

	auto address = getenv("JYQ_ADDRESS");
	ARGBEGIN{
	case 'v':
        jyq::print(std::cout, argv0, "-", VERSION, ", ", COPYRIGHT, "\n");
		exit(0);
	case 'a':
		address = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND;

    std::string cmd = EARGF(usage());

	if(!address) {
		jyq::fatalPrint("$JYQ_ADDRESS not set\n");
    }

    if (client = jyq::Client::mount(address); !client) {
        jyq::fatalPrint(jyq::errbuf(), "\n");
    } else {
        if (auto result = etab.find(cmd); result != etab.end()) {
            auto ret = result->second(argc, argv);
            jyq::Client::unmount(client);
            return ret;
        } else {
            usage();
            return 99;
        }
    }
}