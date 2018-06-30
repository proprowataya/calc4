LLVM_VERSION_SUFFIX=-7

#DEBUG="-g -O0"
RELEASE="-Ofast -DNDEBUG"
FLAGS=${RELEASE}
LLVM_CONFIG=`llvm-config${LLVM_VERSION_SUFFIX} --cxxflags --ldflags --libs --system-libs | sed "s/-Wno-maybe-uninitialized//g"`

clang++${LLVM_VERSION_SUFFIX} ${LLVM_CONFIG} -std=c++17 -fexceptions ${FLAGS} ./*.cpp
