include config.mk

VERSION := 0.1
COPYRIGHT = ©2019 Joshua Scoggins 
CXXFLAGS += '-DVERSION="$(VERSION)"' \
			'-DCOPYRIGHT="$(COPYRIGHT)"'

LIBIXP_CORE_OBJS := client.o \
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
LIBIXP_PTHREAD_OBJS := thread_pthread.o
IXPC_OBJS := ixpc.o 

IXPC_PROG := ixpc
LIBIXP_ARCHIVE := libixp.a
LIBIXP_PTHREAD_ARCHIVE := libixp_pthread.a

OBJS := $(LIBIXP_CORE_OBJS) $(LIBIXP_PTHREAD_OBJS) $(IXPC_OBJS)
PROGS := $(IXPC_PROG) $(LIBIXP_ARCHIVE) $(LIBIXP_PTHREAD_ARCHIVE)


all: options $(PROGS)

options:
	@echo Build Options
	@echo ------------------
	@echo CXXFLAGS = ${CXXFLAGS}
	@echo LDFLAGS = ${LDFLAGS}
	@echo ------------------


$(IXPC_PROG): $(IXPC_OBJS) $(LIBIXP_ARCHIVE)
	@echo LD ${IXPC_PROG}
	@${LD} ${LDFLAGS} -o ${IXPC_PROG} ${IXPC_OBJS} ${LIBIXP_ARCHIVE}

$(LIBIXP_ARCHIVE): $(LIBIXP_CORE_OBJS)
	@echo AR ${LIBIXP_ARCHIVE}
	@${AR} rcs ${LIBIXP_ARCHIVE} ${LIBIXP_CORE_OBJS}

$(LIBIXP_PTHREAD_ARCHIVE): $(LIBIXP_PTHREAD_OBJS)
	@echo AR ${LIBIXP_PTHREAD_ARCHIVE} 
	@${AR} rcs ${LIBIXP_PTHREAD_ARCHIVE} ${LIBIXP_PTHREAD_OBJS}

.cc.o :
	@echo CXX $<
	@${CXX} -I. ${CXXFLAGS} -c $< -o $@

clean: 
	@echo Cleaning...
	@rm -f ${OBJS} ${PROGS}



.PHONY: options

# generated via g++ -MM -std=c++17 *.cc *.h


client.o: client.cc ixp_local.h ixp.h
convert.o: convert.cc ixp_local.h ixp.h
error.o: error.cc ixp_local.h ixp.h
ixpc.o: ixpc.cc ixp.h
map.o: map.cc ixp_local.h ixp.h
message.o: message.cc ixp_local.h ixp.h
request.o: request.cc ixp_local.h ixp.h
rpc.o: rpc.cc ixp_local.h ixp.h
server.o: server.cc ixp_local.h ixp.h
socket.o: socket.cc ixp_local.h ixp.h
srv_util.o: srv_util.cc ixp_local.h ixp_srvutil.h ixp.h
thread.o: thread.cc ixp_local.h ixp.h
thread_pthread.o: thread_pthread.cc ixp_local.h ixp.h
timer.o: timer.cc ixp_local.h ixp.h
transport.o: transport.cc ixp_local.h ixp.h
util.o: util.cc ixp_local.h ixp.h

