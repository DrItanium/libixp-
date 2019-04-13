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
         bool init(IxpRWLock*) override;
         void rlock(IxpRWLock*) override;
         bool canrlock(IxpRWLock*) override;
         void runlock(IxpRWLock*) override;
         void wlock(IxpRWLock*) override;
         bool canwlock(IxpRWLock*) override;
         void wunlock(IxpRWLock*) override;
         void destroy(IxpRWLock*) override;
        /* Mutex */
         bool init(IxpMutex*) override;
         bool canlock(IxpMutex*) override;
         void lock(IxpMutex*) override;
         void unlock(IxpMutex*) override;
         void destroy(IxpMutex*) override;
        /* Rendezvous point */
         bool init(IxpRendez*) override;
         bool wake(IxpRendez*) override;
         bool wakeall(IxpRendez*) override;
         void sleep(IxpRendez*) override;
         void destroy(IxpRendez*) override;
        /* Other */
         char* errbuf() override;
};

} // end namespace ixp

#endif // end JYQ_THREAD_PTHREAD_H__
