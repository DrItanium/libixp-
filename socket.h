/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_SOCKET_H__
#define LIBJYQ_SOCKET_H__
#include <string>
#include <functional>
#include <map>
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
            class Creator {
                public:
                    using Action = std::function<int(const std::string&)>;
                    Creator(const std::string& name, Action dial, Action announce);
                    ~Creator() = default;
                    int dial(const std::string&);
                    int announce(const std::string&);
                    std::string getName() const noexcept { return _name; }
                private:
                    std::string _name;
                    Action _dial, _announce;
            };
            static Connection dial(const std::string&);
            static Connection announce(const std::string&);
            static void registerCreator(const std::string& name, Creator::Action dial, Creator::Action announce);
        private:
            static std::map<std::string, Creator> _ctab;
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
