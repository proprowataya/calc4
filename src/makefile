PROGRAM = calc4
OBJS = Common.o Jit.o Main.o Optimizer.o SyntaxAnalysis.o Test.o

CC = clang${LLVM_VERSION_SUFFIX}
CXX = clang++${LLVM_VERSION_SUFFIX}
LLVM_CONFIG = llvm-config${LLVM_VERSION_SUFFIX}

DEBUG_CXX_FLAGS = -g -O0
RELEASE_CXX_FLAGS = -Ofast -DNDEBUG

DEBUG_RELEASE_CXX_FLAGS = ${RELEASE_CXX_FLAGS}
ifeq (${CONFIG}, DEBUG)
	DEBUG_RELEASE_CXX_FLAGS = ${DEBUG_CXX_FLAGS}
endif

DEPS = $(OBJS:%.o=%.d)
CXX_FLAGS = -fexceptions -Wno-unused-command-line-argument -Wno-unknown-warning-option -MMD -MP -std=c++17 ${DEBUG_RELEASE_CXX_FLAGS}

${PROGRAM}: ${OBJS}
	${CXX} ${OBJS} -o ${PROGRAM} `${LLVM_CONFIG} --ldflags --libs --system-libs` ${CXX_FLAGS} -lgmpxx -lgmp

.cpp.o:
	${CXX} -c $< `${LLVM_CONFIG} --cxxflags` ${CXX_FLAGS}

-include ${DEPS}

clean:
	rm -rf ${OBJS} ${DEPS} ${PROGRAM}
