PROGRAM = calc4
OBJS = Jit.o Main.o Common.o

CC = clang${LLVM_VERSION_SUFFIX}
CXX = clang++${LLVM_VERSION_SUFFIX}
LLVM_CONFIG = llvm-config${LLVM_VERSION_SUFFIX}

DEBUG_CXX_FLAGS = -g -O0
RELEASE_CXX_FLAGS = -Ofast -DNDEBUG

CONFIG = RELEASE
DEBUG_RELEASE_CXX_FLAGS = ${RELEASE_CXX_FLAGS}
ifeq (${CONFIG}, DEBUG)
	DEBUG_RELEASE_CXX_FLAGS = ${DEBUG_CXX_FLAGS}
endif

DEPS = $(OBJS:%.o=%.d)
LLVM_CONFIG_ARGS = `${LLVM_CONFIG} --cxxflags --ldflags --libs --system-libs`
CXX_FLAGS = ${LLVM_CONFIG_ARGS} -fexceptions -Wno-unused-command-line-argument -Wno-unknown-warning-option -MMD -MP -std=c++17 ${DEBUG_RELEASE_CXX_FLAGS}

${PROGRAM}: ${OBJS}
	${CXX} ${CXX_FLAGS} ${OBJS} -o ${PROGRAM}

.cpp.o:
	${CXX} ${CXX_FLAGS} -c $<

-include ${DEPS}

clean:
	rm -rf ${OBJS} ${DEPS} ${PROGRAM}
