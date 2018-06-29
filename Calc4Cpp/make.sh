LLVM_VERSION=7
OPTIMIZE=-Ofast
DEBUG=-DNDEBUG
#DEBUG=-g

clang++-${LLVM_VERSION} `llvm-config-${LLVM_VERSION} --cxxflags --ldflags --libs --system-libs` -std=c++14 ${OPTIMIZE} ${DEBUG} ./Main.cpp
