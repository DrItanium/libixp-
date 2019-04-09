include config.mk

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
LIBIXP_PTHREAD_THREAD_OBJS := thread_pthread.o
LIBIXP_TASK_OBJS := thread_task.o
I960JX_OBJS := opcodes.o Operand.o
I960JX_SIM_OBJS := sim960jx.o \
	core.o \
	dispatcher.o \
	ProcessControls.o \
	NormalRegister.o \
	QuadRegister.o \
	TripleRegister.o \
	DoubleRegister.o \
	ArithmeticControls.o \
	$(I960JX_OBJS)
I960JX_DEC_OBJS := decode960jx.o \
	$(I960JX_OBJS)
IXPC_OBJS := ixpc.o 

IXPC_PROG := ixpc
LIBIXP_ARCHIVE := libixp.a

OBJS := $(LIBIXP_CORE_OBJS) $(LIBIXP_TASK_OBJS) $(LIBIXP_PTHREAD_THREAD_OBJS) $(IXPC_OBJS)
PROGS := $(IXPC_PROG) $(LIBIXP_ARCHIVE)


all: $(PROGS)

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
	@${AR} c ${LIBIXP_ARCHIVE} ${LIBIXP_CORE_OBJS}

.cc.o :
	@echo CXX $<
	@${CXX} -I. ${CXXFLAGS} -c $< -o $@

clean: 
	@echo Cleaning...
	@rm -f ${OBJS} ${PROGS}



.PHONY: options

# generated via g++ -MM -std=c++17 *.cc *.h


client.o: client.cc ixp_local.h
convert.o: convert.cc ixp_local.h
error.o: error.cc ixp_local.h
ixpc.o: ixpc.cc
map.o: map.cc ixp_local.h
message.o: message.cc ixp_local.h
request.o: request.cc ixp_local.h
rpc.o: rpc.cc ixp_local.h
server.o: server.cc ixp_local.h
socket.o: socket.cc ixp_local.h
srv_util.o: srv_util.cc ixp_local.h ixp_srvutil.h
thread.o: thread.cc ixp_local.h
thread_pthread.o: thread_pthread.cc ixp_local.h
thread_task.o: thread_task.cc ixp_local.h
timer.o: timer.cc ixp_local.h
transport.o: transport.cc ixp_local.h
util.o: util.cc ixp_local.h
ixp.o: ixp.h
ixp_local.o: ixp_local.h
ixp_srvutil.o: ixp_srvutil.h

