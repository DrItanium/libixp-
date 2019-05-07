/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_SOCKET_H__
#define LIBJYQ_SOCKET_H__
#include <string>
#include "types.h"
#include "Msg.h"

namespace jyq {
    //int dial(const std::string&);
    //int announce(const std::string&);
    //uint sendmsg(int, Msg*);
    //uint recvmsg(int, Msg*);
    /**
     * Generic connection to a file of some kind!
     */
    class Connection {
        public:
            static Connection dial(const std::string&);
            static Connection announce(const std::string&);
        public:
            explicit Connection(int fid);
            ~Connection() = default;
            int getFid() const noexcept { return _fid; }
            ssize_t write(const std::string& msg);
            ssize_t write(char* c, size_t count);
            ssize_t write(const std::string& msg, size_t count);
            ssize_t read(std::string& msg, size_t count);
            ssize_t read(char* c, size_t count);
            /**
             * Write a message to this connection. 
             * @param msg the message to write to the connection
             * @return number of bytes written, zero means error with error messages being stored in errbuf
             */
            uint sendmsg(Msg& msg);
            /**
             * Read a message from this connection
             * @param msg the message container to write the result to
             * @return the number of bytes received, zero means error happened, use errbuf to get message
             */
            uint recvmsg(Msg& msg);
            bool shutdown(int how);
            bool close();
            operator int() const;
        private:
            int mread(Msg& msg, size_t count);
            int readn(Msg& msg, size_t count);
        private:
            int _fid;
    };
} // end namespace jyq
#endif // end LIBJYQ_SOCKET_H__
