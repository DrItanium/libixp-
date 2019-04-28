#ifndef JYQ_THREAD_PTHREAD_H__
#define JYQ_THREAD_PTHREAD_H__
#include "thread.h"
namespace jyq::concurrency
{

class PThreadImpl final : public ThreadImpl
{
    public:
        using Parent = ThreadImpl;
    public:
        using Parent::Parent;
        /* Read/write lock */
         bool init(jyq::RWLock*) override;
         void rlock(jyq::RWLock*) override;
         bool canrlock(jyq::RWLock*) override;
         void runlock(jyq::RWLock*) override;
         void wlock(jyq::RWLock*) override;
         bool canwlock(jyq::RWLock*) override;
         void wunlock(jyq::RWLock*) override;
         void destroy(jyq::RWLock*) override;
        /* Mutex */
         bool init(jyq::Mutex*) override;
         bool canlock(jyq::Mutex*) override;
         void lock(jyq::Mutex*) override;
         void unlock(jyq::Mutex*) override;
         void destroy(jyq::Mutex*) override;
        /* Rendezvous point */
         bool init(jyq::Rendez*) override;
         bool wake(jyq::Rendez*) override;
         bool wakeall(jyq::Rendez*) override;
         void sleep(jyq::Rendez*) override;
         void destroy(jyq::Rendez*) override;
        /* Other */
         char* errbuf() override;
};

} // end namespace jyq

#endif // end JYQ_THREAD_PTHREAD_H__
