LLVM_VERSION_SUFFIX=-7
OPTIMIZE=-Ofast
DEBUG=-DNDEBUG
#DEBUG=-g

clang++${LLVM_VERSION_SUFFIX} `llvm-config${LLVM_VERSION_SUFFIX} --cxxflags --ldflags --libs --system-libs` -std=c++14 -fexceptions ${OPTIMIZE} ${DEBUG} ./Main.cpp
