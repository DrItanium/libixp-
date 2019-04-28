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


client.o: client.cc jyq.h types.h PrintFunctions.h thread.h util.h
convert.o: convert.cc jyq.h types.h PrintFunctions.h thread.h util.h
error.o: error.cc PrintFunctions.h thread.h types.h
jyqc.o: jyqc.cc
map.o: map.cc jyq.h types.h PrintFunctions.h thread.h util.h
message.o: message.cc jyq.h types.h PrintFunctions.h thread.h util.h
request.o: request.cc jyq.h types.h PrintFunctions.h thread.h util.h
rpc.o: rpc.cc jyq.h types.h PrintFunctions.h thread.h util.h
server.o: server.cc jyq.h types.h PrintFunctions.h thread.h util.h
socket.o: socket.cc jyq.h types.h PrintFunctions.h thread.h util.h
srv_util.o: srv_util.cc jyq.h types.h PrintFunctions.h thread.h util.h \
 jyq_srvutil.h
thread.o: thread.cc jyq.h types.h PrintFunctions.h thread.h util.h
thread_pthread.o: thread_pthread.cc jyq.h types.h PrintFunctions.h \
 thread.h util.h thread_pthread.h
timer.o: timer.cc jyq.h types.h PrintFunctions.h thread.h util.h
transport.o: transport.cc jyq.h types.h PrintFunctions.h thread.h util.h
util.o: util.cc PrintFunctions.h util.h types.h
