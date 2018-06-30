LLVM_VERSION_SUFFIX=-7

#DEBUG="-g -O0"
RELEASE="-Ofast -DNDEBUG"
FLAGS=${RELEASE}

clang++${LLVM_VERSION_SUFFIX} `llvm-config${LLVM_VERSION_SUFFIX} --cxxflags --ldflags --libs --system-libs` -std=c++14 -fexceptions ${FLAGS} ./Main.cpp
