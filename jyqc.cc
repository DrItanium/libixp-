/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#define JYQ_NO_P9_
#define JYQ_P9_STRUCTS
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <list>
#include <functional>
#include <string>
#include <list>
#include <map>
#include <sstream>
#include <boost/program_options.hpp>
#include "jyq.h"

namespace Options = boost::program_options;

namespace {
std::string address;
bool longView = false;
bool onlyDirectory = false;
std::vector<std::string> expression;
std::string etabEntry;
std::string file;
std::vector<std::string> data;

Options::options_description genericOptions("Options");
//Options::options_description lsOptions("ls specific options");
Options::positional_options_description positionalActions;

std::unique_ptr<jyq::Client> client;

void
setupOptions() {
    positionalActions.add("expression", -1);
    genericOptions.add_options()
        ("help", "produce help message")
        ("version,v", 
         "print version and exit")
        ("address,a", 
            Options::value<std::string>(&address)->default_value(""),
         "Address to connect to, form is 'kind!address'")
        ("expression", Options::value<std::vector<std::string>>(), "Expression to parse")
        ("long,l", "Long output form (ls specific)")
        ("only-directory,d", "Don't show the contents of the directory (ls specific)");
}
template<typename ... Args>
int
usage(std::ostream& out, int errorCode, Args&& ... descriptions) {
    (out << ... << descriptions) << std::endl;
    out << "Allowed expressions: " << std::endl;
    out << "\tls [-ld] <file>" << std::endl;
    out << "\tcreate <file>" << std::endl;
    out << "\tread <file>" << std::endl;
    out << "\tremove <file>" << std::endl;
    out << "\twrite <file>" << std::endl;
    out << "\tappend <file>" << std::endl;
    out << "\txwrite <file> <data>" << std::endl;
    return errorCode;
}

int
usage(int errorCode = 1) {
    return usage(std::cerr, errorCode, genericOptions);
}

/* Utility Functions */
void
write_data(std::shared_ptr<jyq::CFid> fid, char *name) {
	long len = 0;

    auto buf = std::make_unique<char[]>(fid->getIoUnit());
    jyq::Connection tmp(0);
	do {
        len = tmp.read(buf.get(), fid->getIoUnit());
		if(len >= 0 && fid->write(buf.get(), len, client->getDoFcallLambda()) != len) {
            throw jyq::Exception("cannot write file '", name, "'");
        }
	} while(len > 0);
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
printStat(std::shared_ptr<jyq::Stat> s, int details) {
	if(details) {
        jyq::print(std::cout, str_of_mode(s->getMode()), " ", 
                s->getUid(), " ", s->getGid(), " ", s->getLength(), " ", 
                str_of_time(s->getMtime()), " ", s->getName(), "\n");
    } else {
		if((s->getMode()&(uint32_t)jyq::DMode::DIR) && (s->getName() != "/")) {
            jyq::print(std::cout, s->getName(), "/\n");
        } else {
            jyq::print(std::cout, s->getName(), "\n");
        }
	}
}

/* Service Functions */
using ServiceFunction = std::function<int()>;
int
xappend() {
    if (auto fid = client->open(file, jyq::OMode::WRITE); !fid) {
        throw jyq::Exception("Can't open file '", file, "'");
    } else {
        auto stat = client->stat(file);
        fid->setOffset(stat->getLength());
        //jyq::Stat::free(stat.get());
        write_data(fid, file.data());
        return 0;
    }
}

int
xwrite() {
    auto fid = client->open(file, jyq::OMode::WRITE);
    if (!fid) {
        throw jyq::Exception("Can't open file '", file, "'");
    }

	write_data(fid, file.data());
	return 0;
}

int
xawrite() {
    auto fid = client->open(file, jyq::OMode::WRITE);
    if (!fid) {
        throw jyq::Exception("Can't open file '", file, "'");
    }

    std::stringstream buf;
    auto current = 0;
    for (const auto& datum : data) {
        buf << datum;
        if (current < data.size()) {
            buf << " ";
        }
        ++current;
    }
    auto str = buf.str();
    if (fid->write(str.c_str(), str.length(), client->getDoFcallLambda()) == -1) {
        throw jyq::Exception("cannot write file '", file, "'");
    }
	return 0;
}

int
xcreate() {
    auto fid = client->create(file, 0777, jyq::OMode::WRITE);
    if (!fid) {
        throw jyq::Exception("Can't create file '", file, "'");
    }

	if((fid->getQid().getType()&(uint32_t)jyq::DMode::DIR) == 0) {
		write_data(fid, file.data());
    }

	return 0;
}

int
xremove() {

    if (!client->remove(file)) {
        throw jyq::Exception("Can't remove file '", file, "'");
    }
	return 0;
}

int
xread() {
    auto fid = client->open(file, jyq::OMode::READ);
    if (!fid) {
        throw jyq::Exception("Can't open file '", file, "'");
    }

    int count = 0;
    auto buf = std::make_unique<char[]>(fid->getIoUnit());
	while((count = fid->read(buf.get(), fid->getIoUnit(), client->getDoFcallLambda())) > 0) {
		write(1, buf.get(), count);
    }

	if(count == -1) {
        throw jyq::Exception("cannot read file/directory '", file, "'");
    }
	return 0;
}

int
xls() {
    auto stat = client->stat(file);
    if (!stat) {
        throw jyq::Exception("Can't stat file '", file, "'");
    }

	if(onlyDirectory || (stat->getMode()&static_cast<uint32_t>(jyq::DMode::DIR)) == 0) {
		printStat(stat, longView);
        //jyq::Stat::free(stat.get());
		return 0;
	}
    //jyq::Stat::free(stat.get());

    auto fid = client->open(file, jyq::OMode::READ);
	if(!fid) {
        throw jyq::Exception("Can't open file '", file, "'");
    }

    std::vector<std::shared_ptr<jyq::Stat>> stats;
    // TODO fix this nonsense
    auto buf = std::make_unique<char[]>(fid->getIoUnit());
    int count = 0;
    for (count = fid->read(buf.get(), fid->getIoUnit(), client->getDoFcallLambda()); count > 0; count = fid->read(buf.get(), fid->getIoUnit(), client->getDoFcallLambda())) {
        auto bufCopy = buf.release();
        buf.reset(new char[fid->getIoUnit()]);
        jyq::Msg m(bufCopy, count, jyq::Msg::Mode::Unpack);
		while(m.getPos() < m.getEnd()) {
            stats.emplace_back(std::make_shared<jyq::Stat>());
            m.pstat(stats.back().get());
		}
	}
    // TODO implement sorting in the future
    for (auto& stat : stats) {
        printStat(stat, longView);
        //jyq::Stat::free(stat.get());
    }
	if(count == -1) {
        throw jyq::Exception("Can't read directory '", file, "'");
    } else {
	    return 0;
    }
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
    try {
        setupOptions();
        Options::variables_map vm;
        Options::store(Options::command_line_parser(argc, argv).
                options(genericOptions).positional(positionalActions).run(), vm);
        Options::notify(vm);

        if (vm.count("version")) {
            jyq::print(std::cout, argv[0], "-", VERSION, ", ", COPYRIGHT, "\n");
            return 0;
        }
        longView = vm.count("long") != 0;
        onlyDirectory = vm.count("only-directory") != 0;
        if (vm.count("expression")) {
            expression = vm["expression"].as<std::vector<std::string>>();
            if (expression.empty()) {
                return usage(1);
            } else {
                etabEntry = expression[0];
                if (expression.size() >= 2) {
                    file = expression[1];
                    if (expression.size() > 2) {
                        // make a subsequence, this is very inefficient but it
                        // works for the time being
                        for (int i = 2; i < expression.size(); ++i) {
                            data.emplace_back(expression[i]);
                        }
                    }
                } else {
                    std::cerr << "file not defined!" << std::endl;
                    return usage(1);
                }
            }
        } else {
            return usage();
        }

        if (address.empty()) {
            if (auto envAddress = getenv("JYQ_ADDRESS"); !envAddress) {
                throw jyq::Exception ("$JYQ_ADDRESS not set\n");
            } else {
                address = std::string(envAddress);
            }
        }

        if (client = jyq::Client::mount(address); !client) {
            throw jyq::Exception("Could not mount ", address);
        } else {
            if (auto result = etab.find(etabEntry); result != etab.end()) {
                return result->second();
            } else {
                return usage(99);
            }
        }
    } catch(jyq::Exception& e) {
        std::cerr << "Error happened: " << e.message() << std::endl;
        return 1;
    }
}
