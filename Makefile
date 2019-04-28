include config.mk

VERSION := 0.1
COPYRIGHT = ©2019 Joshua Scoggins 
CXXFLAGS += '-DVERSION="$(VERSION)"' \
			'-DCOPYRIGHT="$(COPYRIGHT)"'

LIBJYQ_CORE_OBJS := client.o \
					convert.o \
					error.o \
					map.o \
					message.o \
					request.o \
					rpc.o \
					server.o \
					socket.o \
					srv_util.o \
					thread.o \
					timer.o \
					transport.o \
					util.o 
LIBJYQ_PTHREAD_OBJS := thread_pthread.o
JYQC_OBJS := jyqc.o 

JYQC_PROG := jyqc
LIBJYQ_ARCHIVE := libjyq.a
LIBJYQ_PTHREAD_ARCHIVE := libjyq_pthread.a

OBJS := $(LIBJYQ_CORE_OBJS) $(LIBJYQ_PTHREAD_OBJS) $(JYQC_OBJS)
PROGS := $(JYQC_PROG) $(LIBJYQ_ARCHIVE) $(LIBJYQ_PTHREAD_ARCHIVE)


all: options $(PROGS)

options:
	@echo Build Options
	@echo ------------------
	@echo CXXFLAGS = ${CXXFLAGS}
	@echo LDFLAGS = ${LDFLAGS}
	@echo ------------------


$(JYQC_PROG): $(JYQC_OBJS) $(LIBJYQ_ARCHIVE)
	@echo LD ${JYQC_PROG}
	@${LD} ${LDFLAGS} -o ${JYQC_PROG} ${JYQC_OBJS} ${LIBJYQ_ARCHIVE}

$(LIBJYQ_ARCHIVE): $(LIBJYQ_CORE_OBJS)
	@echo AR ${LIBJYQ_ARCHIVE}
	@${AR} rcs ${LIBJYQ_ARCHIVE} ${LIBJYQ_CORE_OBJS}

$(LIBJYQ_PTHREAD_ARCHIVE): $(LIBJYQ_PTHREAD_OBJS)
	@echo AR ${LIBJYQ_PTHREAD_ARCHIVE} 
	@${AR} rcs ${LIBJYQ_PTHREAD_ARCHIVE} ${LIBJYQ_PTHREAD_OBJS}

.cc.o :
	@echo CXX $<
	@${CXX} -I. ${CXXFLAGS} -c $< -o $@

clean: 
	@echo Cleaning...
	@rm -f ${OBJS} ${PROGS}



.PHONY: options

# generated via g++ -MM -std=c++17 *.cc *.h


client.o: client.cc qid.h types.h Msg.h jyq.h PrintFunctions.h thread.h \
 Srv9.h Req9.h Fcall.h stat.h map.h Fid.h util.h
convert.o: convert.cc qid.h types.h Msg.h jyq.h PrintFunctions.h thread.h \
 Srv9.h Req9.h Fcall.h stat.h map.h Fid.h util.h
error.o: error.cc PrintFunctions.h thread.h types.h
jyqc.o: jyqc.cc argv.h types.h
map.o: map.cc map.h types.h thread.h util.h
message.o: message.cc Msg.h qid.h types.h jyq.h PrintFunctions.h thread.h \
 Srv9.h Req9.h Fcall.h stat.h map.h Fid.h util.h argv.h
request.o: request.cc Msg.h qid.h types.h jyq.h PrintFunctions.h thread.h \
 Srv9.h Req9.h Fcall.h stat.h map.h Fid.h util.h argv.h
rpc.o: rpc.cc Msg.h qid.h types.h jyq.h PrintFunctions.h thread.h Srv9.h \
 Req9.h Fcall.h stat.h map.h Fid.h util.h
server.o: server.cc Msg.h qid.h types.h jyq.h PrintFunctions.h thread.h \
 Srv9.h Req9.h Fcall.h stat.h map.h Fid.h util.h
socket.o: socket.cc Msg.h qid.h types.h jyq.h PrintFunctions.h thread.h \
 Srv9.h Req9.h Fcall.h stat.h map.h Fid.h util.h
srv_util.o: srv_util.cc Msg.h qid.h types.h jyq.h PrintFunctions.h \
 thread.h Srv9.h Req9.h Fcall.h stat.h map.h Fid.h util.h jyq_srvutil.h
thread.o: thread.cc thread.h types.h
thread_pthread.o: thread_pthread.cc thread_pthread.h thread.h types.h \
 util.h
timer.o: timer.cc Msg.h qid.h types.h jyq.h PrintFunctions.h thread.h \
 Srv9.h Req9.h Fcall.h stat.h map.h Fid.h util.h timer.h
transport.o: transport.cc Msg.h qid.h types.h jyq.h PrintFunctions.h \
 thread.h Srv9.h Req9.h Fcall.h stat.h map.h Fid.h util.h
util.o: util.cc PrintFunctions.h util.h types.h
