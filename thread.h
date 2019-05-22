#ifndef LIBJYQ_THREAD_H__
#define LIBJYQ_THREAD_H__
/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */

#include <any>
#include <memory>
#include <mutex>
#include "types.h"

namespace jyq {
    struct Mutex : public HasAux {
        Mutex();
        ~Mutex();
        void lock();
        void unlock();
        bool canlock();
        constexpr bool isActive() const noexcept { return _active; }
        bool deactivate();
        private:
            bool _active = false;
    };

    struct Rendez : public HasAux {
        Rendez(Mutex* m = nullptr);
        ~Rendez();
        bool wake();
        bool wakeall();
        void sleep();
        constexpr bool isActive() const noexcept { return _active; }
        bool deactivate();
        Mutex* getMutex() noexcept { return _mutex; }
        const Mutex* getMutex() const noexcept { return _mutex; }
        void setMutex(Mutex* mux) noexcept { _mutex = mux; }
        bool hasMutex() const noexcept { return _mutex != nullptr; }
        private:
            Mutex* _mutex;
            bool _active = false;
    };

    struct RWLock : public HasAux {
        RWLock();
        ~RWLock();
        void readLock();
        void readUnlock();
        bool canReadLock();
        void writeLock();
        void writeUnlock();
        bool canWriteLock();
        constexpr bool isActive() const noexcept { return _active; }
        bool deactivate();
        private:
            bool _active = false;
    };

    namespace concurrency {
        /**
         * Type: Thread
         * Type: Mutex
         * Type: RWLock
         * Type: Rendez
         * Variable: thread
         *
         * The Thread structure is used to adapt libjyq to any of the
         * myriad threading systems it may be used with. Before any
         * other of libjyq's functions is called, thread may be set
         * to a structure filled with implementations of various locking
         * primitives, along with primitive IO functions which may
         * perform context switches until data is available.
         *
         * The names of the functions should be fairly self-explanitory.
         * Read/write locks should allow multiple readers and a single
         * writer of a shared resource, but should not allow new readers
         * while a writer is waitng for a lock. Mutexes should allow
         * only one accessor at a time. Rendezvous points are similar to
         * pthread condition types. P<errbuf> should return a
         * thread-local buffer or the size JYQ_ERRMAX.
         *
         * See also:
         *	F<pthread_init>, F<taskinit>, F<rubyinit>
         */
        class ThreadImpl {
            public:
                ThreadImpl() = default;
                virtual ~ThreadImpl() = default;
                /* Read/write lock */
                virtual bool init(RWLock*) = 0;
                virtual void rlock(RWLock*) = 0;
                virtual bool canrlock(RWLock*) = 0;
                virtual void runlock(RWLock*) = 0;
                virtual void wlock(RWLock*) = 0;
                virtual bool canwlock(RWLock*) = 0;
                virtual void wunlock(RWLock*) = 0;
                virtual void destroy(RWLock*) = 0;
                /* Mutex */
                virtual bool init(Mutex*) = 0;
                virtual bool canlock(Mutex*) = 0;
                virtual void lock(Mutex*) = 0;
                virtual void unlock(Mutex*) = 0;
                virtual void destroy(Mutex*) = 0;
                /* Rendezvous point */
                virtual bool init(Rendez*) = 0;
                virtual bool wake(Rendez*) = 0;
                virtual bool wakeall(Rendez*) = 0;
                virtual void sleep(Rendez*) = 0;
                virtual void destroy(Rendez*) = 0;
                /* Other */
                virtual char* errbuf() = 0;
                virtual ssize_t read(int, void*, size_t);
                virtual ssize_t write(int, const void*, size_t);
                virtual int select(int, fd_set*, fd_set*, fd_set*, timeval*);
                // interface with old design
                auto initmutex(Mutex* m) { return init(m); }
                auto initrwlock(RWLock* a) { return init(a); }
                void mdestroy(Mutex* m) { destroy(m); }
                void rwdestroy(RWLock* a) { destroy(a); }
                auto initrendez(Rendez* r) { return init(r); }
                void rdestroy(Rendez* r) { destroy(r); }
        };


        extern std::unique_ptr<ThreadImpl> threadModel;

        template<typename T>
            void setThreadingModel() noexcept {
                static_assert(std::is_base_of_v<ThreadImpl, T>, "Threading model must be a child of Thread");
                static_assert(std::is_default_constructible_v<T>, "Provided type is not default constructible");
                // TODO: insert static assertions to make sure that the type is a child of Thread
                threadModel = std::make_unique<T>();
            }

        class NoThreadImpl final : public ThreadImpl {
            public:
                using ThreadImpl::ThreadImpl;
                /* Read/write lock */
                bool init(RWLock*) override     { return false; }
                void rlock(RWLock*) override    { }
                bool canrlock(RWLock*) override { return true; }
                void runlock(RWLock*) override  { }
                void wlock(RWLock*) override    { }
                bool canwlock(RWLock*) override { return true; }
                void wunlock(RWLock*) override  { }
                void destroy(RWLock*) override  { }
                /* Mutex */
                bool init(Mutex*) override    { return false; }
                bool canlock(Mutex*) override { return true; }
                void lock(Mutex*) override    { }
                void unlock(Mutex*) override  { }
                void destroy(Mutex*) override { }
                /* Rendezvous point */
                bool init(Rendez*) override    { return false; }
                bool wake(Rendez*) override    { return false; }
                bool wakeall(Rendez*) override { return false; }
                void destroy(Rendez*) override { }
                void sleep(Rendez*) override;
                /* Other */
                char* errbuf() override;
        };
        class ReadLockWrapper final {
            public:
                ReadLockWrapper(RWLock& lock) : _lock(lock) { }
                ~ReadLockWrapper() = default;
                ReadLockWrapper(const ReadLockWrapper&) = delete;
                ReadLockWrapper(ReadLockWrapper&&) = delete;
                ReadLockWrapper& operator=(const ReadLockWrapper&) = delete;
                ReadLockWrapper& operator=(ReadLockWrapper&&) = delete;
                void lock() { _lock.readLock(); }
                void unlock() { _lock.readUnlock(); }
            private:
                RWLock& _lock;
        };
        class WriteLockWrapper final {
            public:
                WriteLockWrapper(RWLock& lock) : _lock(lock) { }
                ~WriteLockWrapper() = default;
                WriteLockWrapper(const WriteLockWrapper&) = delete;
                WriteLockWrapper(WriteLockWrapper&&) = delete;
                WriteLockWrapper& operator=(const WriteLockWrapper&) = delete;
                WriteLockWrapper& operator=(WriteLockWrapper&&) = delete;
                void lock() { _lock.writeLock(); }
                void unlock() { _lock.writeUnlock(); }
            private:
                RWLock& _lock;
        };
        template<typename T>
        using Locker = std::lock_guard<T>;
        using WriteLocker = Locker<WriteLockWrapper>;
        using ReadLocker = Locker<ReadLockWrapper>;
    } // end namespace concurrency
} // end namespace jyq

#endif // end LIBJYQ_THREAD_H__
