#ifndef LIBJYQ_THREAD_H__
#define LIBJYQ_THREAD_H__
/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */

#include <any>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <shared_mutex>
#include "types.h"

namespace jyq {
    using Mutex = std::mutex;
    using Rendez = std::condition_variable;
    using RWLock = std::shared_mutex;
#if 0
    struct Mutex {
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

    //struct Rendez {
    //    Rendez(Mutex* m = nullptr);
    //    ~Rendez();
    //    bool wake();
    //    bool wakeall();
    //    void sleep();
    //    constexpr bool isActive() const noexcept { return _active; }
    //    bool deactivate();
    //    Mutex* getMutex() noexcept { return _mutex; }
    //    const Mutex* getMutex() const noexcept { return _mutex; }
    //    void setMutex(Mutex* mux) noexcept { _mutex = mux; }
    //    bool hasMutex() const noexcept { return _mutex != nullptr; }
    //    private:
    //        std::condition_variable _cond;
    //        std::unique_lock<
    //        
    //        Mutex& _mutex;
    //        bool _active = false;
    //};

    // RWLock <=> std::shared_mutex
    //struct RWLock : public HasAux {
    //    RWLock();
    //    ~RWLock();
    //    void readLock();
    //    void readUnlock();
    //    bool canReadLock();
    //    void writeLock();
    //    void writeUnlock();
    //    bool canWriteLock();
    //    constexpr bool isActive() const noexcept { return _active; }
    //    bool deactivate();
    //    private:
    //        bool _active = false;
    //};
    namespace concurrency {
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
#endif
} // end namespace jyq

#endif // end LIBJYQ_THREAD_H__
