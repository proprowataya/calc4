PROGRAM = calc4
OBJS = Common.o Main.o Optimizer.o StackMachine.o SyntaxAnalysis.o Test.o
JITOBJS = Jit.o
ALLOBJS = ${OBJS} ${JITOBJS}
DEPS = $(ALLOBJS:%.o=%.d)

DEBUG_CXX_FLAGS = -g -O0
RELEASE_CXX_FLAGS = -Ofast -DNDEBUG

DEBUG_RELEASE_CXX_FLAGS = ${RELEASE_CXX_FLAGS}
ifeq (${CONFIG}, DEBUG)
	DEBUG_RELEASE_CXX_FLAGS = ${DEBUG_CXX_FLAGS}
endif

CXX_FLAGS = -Wall -fexceptions -Wno-unknown-warning-option -Wno-unused-command-line-argument\
	-MMD -MP -std=c++17 -DENABLE_INT128 ${DEBUG_RELEASE_CXX_FLAGS}

ifdef ENABLE_JIT
	CC = clang${LLVM_VERSION_SUFFIX}
	CXX = clang++${LLVM_VERSION_SUFFIX}
	LLVM_CONFIG = llvm-config${LLVM_VERSION_SUFFIX}
	LLVM_CXX_FLAGS = $(shell ${LLVM_CONFIG} --cxxflags) -DENABLE_JIT
	LLVM_LD_FLAGS = $(shell ${LLVM_CONFIG} --ldflags --libs --system-libs)
	OBJS += ${JITOBJS}
else
	LLVM_CXX_FLAGS =
	LLVM_LD_FLAGS =
endif

ifdef ENABLE_GMP
	GMP_CXX_FLAGS = -DENABLE_GMP
	GMP_LD_FLAGS = -lgmpxx -lgmp
else
	GMP_CXX_FLAGS =
	GMP_LD_FLAGS =
endif

${PROGRAM}: ${OBJS}
	${CXX} ${OBJS} -o ${PROGRAM} ${LLVM_LD_FLAGS} ${GMP_LD_FLAGS}

.cpp.o:
	${CXX} -c $< ${LLVM_CXX_FLAGS} ${CXX_FLAGS} ${GMP_CXX_FLAGS}

-include ${DEPS}

clean:
	rm -rf ${ALLOBJS} ${DEPS} ${PROGRAM}
