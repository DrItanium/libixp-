#ifndef IXP_THREAD_PTHREAD_H__
#define IXP_THREAD_PTHREAD_H__
#include "ixp.h"
namespace ixp::concurrency
{

class PThreadImpl final : public ThreadImpl
{
    public:
        using Parent = ThreadImpl;
    public:
        using Parent::Parent;
        /* Read/write lock */
         bool init(ixp::RWLock*) override;
         void rlock(ixp::RWLock*) override;
         bool canrlock(ixp::RWLock*) override;
         void runlock(ixp::RWLock*) override;
         void wlock(ixp::RWLock*) override;
         bool canwlock(ixp::RWLock*) override;
         void wunlock(ixp::RWLock*) override;
         void destroy(ixp::RWLock*) override;
        /* Mutex */
         bool init(ixp::Mutex*) override;
         bool canlock(ixp::Mutex*) override;
         void lock(ixp::Mutex*) override;
         void unlock(ixp::Mutex*) override;
         void destroy(ixp::Mutex*) override;
        /* Rendezvous point */
         bool init(ixp::Rendez*) override;
         bool wake(ixp::Rendez*) override;
         bool wakeall(ixp::Rendez*) override;
         void sleep(ixp::Rendez*) override;
         void destroy(ixp::Rendez*) override;
        /* Other */
         char* errbuf() override;
};

} // end namespace ixp

#endif // end JYQ_THREAD_PTHREAD_H__
