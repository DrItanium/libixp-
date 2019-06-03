/* Written by Kris Maglione <maglione.k at Gmail> */
/* C++ Implementation copyright (c)2019 Joshua Scoggins */
/* Public domain */
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <list>
#include <string>
#include <sstream>
#include "util.h"

namespace jyq {
namespace {
    const std::string&
        _user() {
            static std::string user;

            if(user.empty()) {
                if (auto pw = getpwuid(getuid()); pw) {
                    user = pw->pw_name;
                }
            }
            if(user.empty()) {
                user = "none";
            }
            return user;
        }
}

bool 
rmkdir(const std::string& path, int mode) {
    auto tokens = tokenize(path, '/');
    for (const auto& pathComponent : tokens) {
        if (auto ret = mkdir(pathComponent.c_str(), mode); (ret == -1) && (errno != EEXIST)) {
            throw Exception("Can't create path '", path, "': ", strerror(errno));
        }
    }
    return true;

}

static std::string
ns_display() {

	if (auto disp = std::getenv("DISPLAY"); !disp || disp[0] == '\0') {
        throw Exception("$DISPLAY is unset");
    } else {
        std::string displayVariable(disp);
        if (auto subComponent = displayVariable.substr(displayVariable.length() - 2); 
                (displayVariable != subComponent) && 
                (!std::strcmp(subComponent.c_str(), ".0"))) {
            // TODO strcmp must be replaced!
            displayVariable = displayVariable.substr(0, displayVariable.length() - 2);
        }
        if (std::string newPath = smprint("/tmp/ns.", _user(), ".", displayVariable); !rmkdir(newPath.c_str(), 0700)) {
            return "";
        } else if(struct stat st; stat(newPath.c_str(), &st)) {
            throw Exception("Cannot stat Namespace path '", newPath, "'");
        } else if(getuid() != st.st_uid) {
            throw Exception("Namespace path '", newPath, "' exists but is not owned by you");
        } else if((st.st_mode & 077) && chmod(newPath.c_str(), st.st_mode & ~077)) {
            throw Exception("Namespace path '", newPath, "' exists, but has wrong permissions");
        } else {
            return newPath;
        }
    }
}

/**
 * Function: namespace
 *
 * Returns the path of the canonical 9p namespace directory.
 * Either the value of $NAMESPACE, if it's set, or, roughly,
 * /tmp/ns.${USER}.${DISPLAY:%.0=%}. In the latter case, the
 * directory is created if it doesn't exist, and it is
 * ensured to be owned by the current user, with no group or
 * other permissions.
 *
 * Returns:
 *	A statically allocated string which must not be freed
 *	or altered by the caller. The same value is returned
 *	upon successive calls.
 * Bugs:
 *	This function is not thread safe until after its first
 *	call.
 */
/* Not especially threadsafe. */
std::string
getNamespace() {
    static std::string _namespace;
    if (_namespace.empty()) {
        if (auto ev = std::getenv("NAMESPACE"); !ev) {
            _namespace = ns_display();
        } else {
            _namespace = ev;
        }
    }
	return _namespace;
}


std::list<std::string>
tokenize(const std::string& input, char delim) {
    std::list<std::string> tokens;
    std::istringstream ss(input);
    for (std::string line; std::getline(ss, line, delim);) {
        if (!line.empty()) {
            tokens.emplace_back(line);
        }
    }
    return tokens;
}

} // end namespace jyq

char* argv0 = nullptr;
