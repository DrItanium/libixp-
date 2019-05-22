/**
 * @file
 * Thread implementation which uses C++17 library features
 * @copyright 
 * © 2019 Joshua Scoggins 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef JYQ_THREAD_CXX17_THREAD_H__
#define JYQ_THREAD_CXX17_THREAD_H__
#include "thread.h"
namespace jyq::concurrency
{

class Cxx17ThreadImpl final : public ThreadImpl
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

#endif // end JYQ_THREAD_CXX17_THREAD_H__
