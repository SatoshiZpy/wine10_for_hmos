set -x
LLVM_DIR="${ROOT_DIR}/third_party/llvm-project"

cd ${LLVM_DIR}

cmake -S llvm -B build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DLLVM_TARGETS_TO_BUILD="AArch64;X86" \
  -DLLVM_ENABLE_ASSERTIONS=ON \
  -DCMAKE_INSTALL_PREFIX=./install

cmake --build build --target install -j $(nproc)