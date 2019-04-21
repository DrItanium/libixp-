/* Public Domain --Kris Maglione */
#include <unistd.h>
#include "ixp_local.h"

namespace ixp {
Mutex::Mutex() { concurrency::threadModel->init(this); }
Mutex::~Mutex() { concurrency::threadModel->destroy(this); }

Rendez::Rendez() { concurrency::threadModel->init(this); }
Rendez::~Rendez() { concurrency::threadModel->destroy(this); }

RWLock::RWLock() { concurrency::threadModel->init(this); }
RWLock::~RWLock() { concurrency::threadModel->destroy(this); }
namespace concurrency {

ssize_t 
ThreadImpl::read(int fd, void* buf, size_t count) {
    return ::read(fd, buf, count);
}
ssize_t 
ThreadImpl::write(int fd, const void* buf, size_t count) {
    return ::write(fd, buf, count);
}
int 
ThreadImpl::select(int fd, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, timeval* timeout) {
    return ::select(fd, readfds, writefds, exceptfds, timeout);
}
char*
NoThreadImpl::errbuf() {
    static char errbuf[ErrorMax];
    return errbuf;
}
void
NoThreadImpl::sleep(Rendez*) {
    throw "unimplemented";
}
std::unique_ptr<ThreadImpl> threadModel = std::make_unique<NoThreadImpl>();
} // end namespace concurrency


}
