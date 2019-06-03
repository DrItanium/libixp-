include config.mk

VERSION := 0.1
COPYRIGHT = ©2019 Joshua Scoggins 
CXXFLAGS += '-DVERSION="$(VERSION)"' \
			'-DCOPYRIGHT="$(COPYRIGHT)"'

LIBJYQ_CORE_OBJS := client.o \
					convert.o \
					error.o \
					message.o \
					request.o \
					rpc.o \
					server.o \
					socket.o \
					timer.o \
					transport.o \
					util.o 
LIBJYQ_UTIL_OBJS := srv_util.o 
JYQC_OBJS := jyqc.o 

JYQC_PROG := jyqc
LIBJYQ_ARCHIVE := libjyq.a
LIBJYQ_UTIL_ARCHIVE := libjyq_util.a

OBJS := $(LIBJYQ_CORE_OBJS) $(JYQC_OBJS) 
PROGS := $(JYQC_PROG) $(LIBJYQ_ARCHIVE) $(LIBJYQ_UTIL_ARCHIVE)


all: options $(PROGS)

options:
	@echo Build Options
	@echo ------------------
	@echo CXXFLAGS = ${CXXFLAGS}
	@echo LDFLAGS = ${LDFLAGS}
	@echo ------------------


$(JYQC_PROG): $(JYQC_OBJS) $(LIBJYQ_ARCHIVE)
	@echo LD ${JYQC_PROG}
	@${LD} ${LDFLAGS} -o ${JYQC_PROG} ${JYQC_OBJS} ${LIBJYQ_ARCHIVE} -lboost_program_options

$(LIBJYQ_ARCHIVE): $(LIBJYQ_CORE_OBJS)
	@echo AR ${LIBJYQ_ARCHIVE}
	@${AR} rcs ${LIBJYQ_ARCHIVE} ${LIBJYQ_CORE_OBJS}

$(LIBJYQ_UTIL_ARCHIVE): $(LIBJYQ_UTIL_OBJS)
	@echo AR ${LIBJYQ_UTIL_ARCHIVE}
	@${AR} rcs ${LIBJYQ_UTIL_ARCHIVE} ${LIBJYQ_UTIL_OBJS}

.cc.o :
	@echo CXX $<
	@${CXX} -I. ${CXXFLAGS} -c $< -o $@

clean: 
	@echo Cleaning...
	@rm -f ${OBJS} ${PROGS}



.PHONY: options

# generated via g++ -MM -std=c++17 *.cc


client.o: client.cc Client.h types.h Msg.h qid.h stat.h Fcall.h Rpc.h \
 socket.h CFid.h util.h
convert.o: convert.cc qid.h types.h Msg.h stat.h jyq.h Srv9.h Conn9.h \
 Fcall.h map.h Conn.h socket.h Fid.h Req9.h util.h Client.h Rpc.h CFid.h \
  Server.h timer.h
error.o: error.cc types.h
jyqc.o: jyqc.cc jyq.h types.h Srv9.h Conn9.h qid.h Fcall.h stat.h Msg.h \
 map.h Conn.h socket.h Fid.h Req9.h util.h Client.h Rpc.h CFid.h  \
 Server.h timer.h
message.o: message.cc Msg.h types.h qid.h stat.h jyq.h Srv9.h Conn9.h \
 Fcall.h map.h Conn.h socket.h Fid.h Req9.h util.h Client.h Rpc.h CFid.h \
  Server.h timer.h
request.o: request.cc Msg.h types.h qid.h stat.h jyq.h Srv9.h Conn9.h \
 Fcall.h map.h Conn.h socket.h Fid.h Req9.h util.h Client.h Rpc.h CFid.h \
  Server.h timer.h
rpc.o: rpc.cc Rpc.h types.h Fcall.h qid.h stat.h Msg.h Client.h socket.h \
 util.h
server.o: server.cc Msg.h types.h qid.h stat.h Server.h Conn.h socket.h \
 timer.h Fcall.h
socket.o: socket.cc Msg.h types.h qid.h stat.h jyq.h Srv9.h Conn9.h \
 Fcall.h map.h Conn.h socket.h Fid.h Req9.h util.h Client.h Rpc.h CFid.h \
  Server.h timer.h
srv_util.o: srv_util.cc jyq_util.h jyq.h types.h Srv9.h Conn9.h qid.h \
 Fcall.h stat.h Msg.h map.h Conn.h socket.h Fid.h Req9.h util.h Client.h \
 Rpc.h CFid.h  Server.h timer.h
timer.o: timer.cc Msg.h types.h qid.h stat.h jyq.h Srv9.h Conn9.h Fcall.h \
 map.h Conn.h socket.h Fid.h Req9.h util.h Client.h Rpc.h CFid.h  \
 Server.h timer.h
transport.o: transport.cc Msg.h types.h qid.h stat.h jyq.h Srv9.h Conn9.h \
 Fcall.h map.h Conn.h socket.h Fid.h Req9.h util.h Client.h Rpc.h CFid.h \
  Server.h timer.h
util.o: util.cc util.h types.h 
