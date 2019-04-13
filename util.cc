/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include "ixp_local.h"

namespace ixp {
/**
 * Function: ixp_smprint
 *
 * This function formats its arguments as F<printf> and returns
 * a F<malloc> allocated string containing the result.
 */
char*
smprint(const char *fmt, ...) {
	va_list ap;
	char *s;

	va_start(ap, fmt);
	s = ixp_vsmprint(fmt, ap);
	va_end(ap);
	if(!s)
		ixp_werrstr("no memory");
	return s;
}

static char*
_user(void) {
	static char *user;
	struct passwd *pw;

	if(!user) {
		pw = getpwuid(getuid());
		if(pw)
			user = strdup(pw->pw_name);
	}
	if(!user)
		user = "none";
	return user;
}

static int
rmkdir(char *path, int mode) {
	char *p;
	int ret;
	char c;

	for(p = path+1; ; p++) {
		c = *p;
		if((c == '/') || (c == '\0')) {
			*p = '\0';
			ret = mkdir(path, mode);
			if((ret == -1) && (errno != EEXIST)) {
				ixp_werrstr("Can't create path '%s': %s", path, ixp_errbuf());
				return 0;
			}
			*p = c;
		}
		if(c == '\0')
			break;
	}
	return 1;
}

static char*
ns_display(void) {
	char *path, *disp;
	struct stat st;

	disp = getenv("DISPLAY");
	if(!disp || disp[0] == '\0') {
		ixp_werrstr("$DISPLAY is unset");
		return nullptr;
	}

	disp = estrdup(disp);
	path = &disp[strlen(disp) - 2];
	if(path > disp && !strcmp(path, ".0"))
		*path = '\0';

	path = smprint("/tmp/ns.%s.%s", _user(), disp);
	free(disp);

	if(!rmkdir(path, 0700))
		;
	else if(stat(path, &st))
		ixp_werrstr("Can't stat Namespace path '%s': %s", path, ixp_errbuf());
	else if(getuid() != st.st_uid)
		ixp_werrstr("Namespace path '%s' exists but is not owned by you", path);
	else if((st.st_mode & 077) && chmod(path, st.st_mode & ~077))
		ixp_werrstr("Namespace path '%s' exists, but has wrong permissions: %s", path, ixp_errbuf());
	else
		return path;
	free(path);
	return nullptr;
}

/**
 * Function: ixp_namespace
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
char*
getNamespace(void) {
    static char* _namespace;
    if (!_namespace) {
        if (auto ev = getenv("NAMESPACE"); !ev) {
            _namespace = ns_display();
        } else {
            _namespace = ev;
        }
    }
	return _namespace;
}

/**
 * Function: ixp_eprint
 *
 * libixp calls this function on error. It formats its arguments
 * as F<printf> and exits the program.
 */
void
ixp_eprint(const char *fmt, ...) {
	va_list ap;

	int err = errno;
	fprintf(stderr, "libixp: fatal: ");

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if(fmt[strlen(fmt)-1] == ':')
		fprintf(stderr, " %s\n", strerror(err));
	else
		fprintf(stderr, "\n");

	exit(1);
}

/* Can't malloc */
static void
mfatal(char *name, uint size) {
	const char
		couldnot[] = "libixp: fatal: Could not ",
		paren[] = "() ",
		bytes[] = " bytes\n";
	char sizestr[8];
	
	int i = sizeof sizestr;
	do {
		sizestr[--i] = '0' + (size%10);
		size /= 10;
	} while(size > 0);

	write(1, couldnot, sizeof(couldnot)-1);
	write(1, name, strlen(name));
	write(1, paren, sizeof(paren)-1);
	write(1, sizestr+i, sizeof(sizestr)-i);
	write(1, bytes, sizeof(bytes)-1);

	exit(1);
}

/**
 * Function: ixp_emalloc
 * Function: ixp_emallocz
 * Function: ixp_erealloc
 * Function: ixp_estrdup
 *
 * These functions act like their stdlib counterparts, but print
 * an error message and exit the program if allocation fails.
 * ixp_emallocz acts like ixp_emalloc but additionally zeros the
 * result of the allocation.
 */
void*
emalloc(uint size) {
	if (void *ret = malloc(size); !ret) {
		mfatal("malloc", size);
        throw "SHOULDN'T GET HERE!";
    } else {
        return ret;
    }
}

void*
emallocz(uint size) {
	void *ret = emalloc(size);
	memset(ret, 0, size);
	return ret;
}

void*
erealloc(void *ptr, uint size) {
	if (void *ret = realloc(ptr, size); !ret) {
		mfatal("realloc", size);
        throw "SHOULDN'T GET HERE!";
    } else {
        return ret;
    }
}

char*
estrdup(const char *str) {
	if (void *ret = strdup(str); !ret) {
		mfatal("strdup", strlen(str));
        throw "SHOULDN'T GET HERE!";
    } else {
        return (char*)ret;
    }
}

uint
tokenize(char *res[], uint reslen, char *str, char delim) {
	uint i = 0;
	char* s = str;
	while(i < reslen && *s) {
		while(*s == delim)
			*(s++) = '\0';
		if(*s)
			res[i++] = s;
		while(*s && *s != delim)
			s++;
	}
	return i;
}

uint
strlcat(char *dst, const char *src, uint size) {
	char* d = dst;
	const char* s = src;
	int n = size;
	while(n-- > 0 && *d != '\0')
		d++;
	auto len = n;

	while(*s != '\0' && n-- > 0)
		*d++ = *s++;
	while(*s++ != '\0')
		n--;
	if(len > 0)
		*d = '\0';
	return size - n - 1;
}
} // end namespace ixp

char* argv0 = nullptr;
