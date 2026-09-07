#!/bin/bash
#
# krb5 交叉编译脚本 - OpenHarmony (aarch64-linux-ohos)
#
# 使用方法: ./build_krb5.sh
#
# 说明:
#   本脚本为 MIT Kerberos 5 (krb5 1.23) 交叉编译到 OpenHarmony 平台
#   目标架构: aarch64-linux-ohos (musl libc)
#   输出目录: krb5/krb5/out
#
(

set -ex

# ============================================================
# 1. 环境变量设置 - 工具链
# ============================================================
export AR="${OHOS_TOOLCHAIN}/bin/llvm-ar"
export AS="${OHOS_TOOLCHAIN}/bin/llvm-as"
export CC="${OHOS_TOOLCHAIN}/bin/clang --target=aarch64-linux-ohos"
export CXX="${OHOS_TOOLCHAIN}/bin/clang++ --target=aarch64-linux-ohos"
export LD="${OHOS_TOOLCHAIN}/bin/ld.lld"
export NM="${OHOS_TOOLCHAIN}/bin/llvm-nm"
export OBJDUMP="${OHOS_TOOLCHAIN}/bin/llvm-objdump"
export OBJCOPY="${OHOS_TOOLCHAIN}/bin/llvm-objcopy"
export RANLIB="${OHOS_TOOLCHAIN}/bin/llvm-ranlib"
export STRIP="${OHOS_TOOLCHAIN}/bin/llvm-strip"

export CFLAGS="-fPIC -D__MUSL__=1 --sysroot=${OHOS_SYSROOT} -I${OHOS_SYSROOT}/usr/include/aarch64-linux-ohos"
export CXXFLAGS="-fPIC -D__MUSL__=1 --sysroot=${OHOS_SYSROOT} -I${OHOS_SYSROOT}/usr/include/aarch64-linux-ohos"
export LDFLAGS="--sysroot=${OHOS_SYSROOT} -L${OHOS_SYSROOT}/usr/lib/aarch64-linux-ohos"

# ============================================================
# 2. 项目路径设置
# ============================================================

KRB5_ROOT=${ROOT_DIR}/third_party/krb5
KRB5_DIR=${KRB5_ROOT}/src
BUILD_DIR=${KRB5_ROOT}/build
KRB5_INSTALL_DIR=${KRB5_ROOT}/out

# ============================================================
# 3. 补丁: 为 shlib.conf 添加 OpenHarmony 支持
# ============================================================
SHLIB_CONF=${KRB5_DIR}/config/shlib.conf

if ! grep -q 'linux-ohos' "${SHLIB_CONF}"; then
    echo ">>> 为 shlib.conf 添加 OpenHarmony (linux-ohos) 平台支持..."
    # 使用 Python 在 *-*-linux* 模式块之前插入 *-*-linux-ohos* 模式
    python3 -c "
import re
with open('${SHLIB_CONF}', 'r') as f:
    content = f.read()
ohos_block = '''*-*-linux-ohos*)
	PICFLAGS=-fPIC
	SHLIBVEXT='.so.\$(LIBMAJOR).\$(LIBMINOR)'
	SHLIBSEXT='.so.\$(LIBMAJOR)'
	SHLIBEXT=.so
	LDCOMBINE='\$(CC) -shared -fPIC -Wl,-z,nodelete -Wl,-h,\$(LIBPREFIX)\$(LIBBASE)\$(SHLIBSEXT)'
	UNDEF_CHECK='-Wl,--no-undefined'
	LDCOMBINE_TAIL='-Wl,--version-script binutils.versions \$(EXPORT_CHECK)'
	SHLIB_EXPORT_FILE_DEP=binutils.versions
	RPATH_FLAG='-Wl,--enable-new-dtags -Wl,-rpath -Wl,'
	SHLIB_RPATH_FLAGS='\$(RPATH_FLAG)\$(SHLIB_RDIRS)'
	SHLIB_EXPFLAGS='\$(SHLIB_RPATH_FLAGS) \$(SHLIB_DIRS) \$(SHLIB_EXPLIBS)'
	PROFFLAGS=-pg
	PROG_RPATH_FLAGS='\$(RPATH_FLAG)\$(PROG_RPATH)'
	CC_LINK_SHARED='\$(CC) \$(PROG_LIBPATH) \$(PROG_RPATH_FLAGS) \$(CFLAGS) \$(LDFLAGS)'
	CC_LINK_STATIC='\$(CC) \$(PROG_LIBPATH) \$(CFLAGS) \$(LDFLAGS)'
	CXX_LINK_SHARED='\$(CXX) \$(PROG_LIBPATH) \$(PROG_RPATH_FLAGS) \$(CXXFLAGS) \$(LDFLAGS)'
	CXX_LINK_STATIC='\$(CXX) \$(PROG_LIBPATH) \$(CXXFLAGS) \$(LDFLAGS)'
	RUN_ENV='LD_LIBRARY_PATH=\`echo \$(PROG_LIBPATH) | sed -e \"s/-L//g\" -e \"s/ /:/g\"\`'
	RUN_VARS='LD_LIBRARY_PATH'
	lib_unload_prevented=yes

	;;
'''
pattern = r'(\*-\*-\*-linux\* \| \*-\*-\*-gnu\* \| \*-\*-\*-k\*bsd\*-\*-gnu\))'
content = re.sub(pattern, ohos_block + r'\n\1', content)
with open('${SHLIB_CONF}', 'w') as f:
    f.write(content)
print('Patch applied successfully.')
"
    echo ">>> shlib.conf 补丁已应用"
else
    echo ">>> shlib.conf 已包含 linux-ohos 支持, 跳过补丁"
fi

# ============================================================
# 4. 生成 configure 脚本 (如果不存在)
# ============================================================
if [ ! -f "${KRB5_DIR}/configure" ]; then
    echo ">>> 运行 autoreconf 生成 configure..."
    cd "${KRB5_DIR}"
    autoreconf -f -i
    echo ">>> configure 脚本已生成"
fi

# ============================================================
# 5. 创建构建目录并运行 configure
# ============================================================
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

echo ">>> 运行 configure (交叉编译 aarch64-linux-ohos)..."

# 交叉编译时 AC_RUN_IFELSE 会失败, 需要通过缓存变量预置结果:
#   - ac_cv_printf_positional=yes   (configure.ac:962 printf 位置参数测试)
#   - krb5_cv_attr_constructor_destructor=yes,yes (aclocal.m4:1315 构造/析构属性测试)
#   - 其他常见的 musl libc 交叉编译缓存变量

${KRB5_DIR}/configure \
    --host=aarch64-linux-ohos \
    --build=x86_64-pc-linux-gnu \
    --prefix=${KRB5_INSTALL_DIR} \
    --disable-rpath \
    --disable-pkinit \
    --disable-nls \
    --disable-aesni \
    --without-keyutils \
    --without-libedit \
    --with-crypto-impl=builtin \
    --with-tls-impl=no \
    --with-netlib="-lresolv" \
    \
    ac_cv_printf_positional=yes \
    krb5_cv_attr_constructor_destructor=yes,yes \
    krb5_cv_system_ss_okay=assumed \
    ac_cv_func_regcomp=yes \
    ac_cv_func_regexec=yes \
    ac_cv_func_regfree=yes \
    ac_cv_func_strchr=yes \
    ac_cv_func_strstr=yes \
    ac_cv_func_memmove=yes \
    ac_cv_func_mkstemp=yes \
    ac_cv_func_strlcpy=no \
    ac_cv_func_strlcat=no \
    ac_cv_func_fnmatch=yes \
    ac_cv_func_vasprintf=yes \
    ac_cv_func_secure_getenv=no \
    ac_cv_func_gettimeofday=yes \
    ac_cv_func_getpwnam_r=yes \
    ac_cv_func_getgrnam_r=yes \
    ac_cv_func_gai_strerror=yes \
    ac_cv_func_getnameinfo=yes \
    ac_cv_func_getaddrinfo=yes \
    ac_cv_func_inet_ntop=yes \
    ac_cv_func_inet_pton=yes \
    ac_cv_func_strerror_r=yes \
    ac_cv_func_pthread_once=yes \
    ac_cv_func_pthread_rwlock_init=yes \
    ac_cv_search_pthread_create=-lpthread \
    ax_cv_pthread_flags="-lpthread"

echo ">>> configure 完成"

# ============================================================
# 6. 编译
# ============================================================
echo ">>> 开始编译..."
make -j$(nproc)

echo ">>> 编译完成"

# ============================================================
# 7. 安装
# ============================================================
echo ">>> 安装到 ${KRB5_INSTALL_DIR}..."
make install

# ============================================================
# 8. 生成静态库 (.a) — wine-preloader 用 -static 链接,需要 .a 而非 .so
# ============================================================
# krb5 的 configure 是二选一 (--enable-static 编纯 .a 禁 .so, 默认编纯 .so);
# 但 wine 运行时要 .so、preloader 的 -static 要 .a, 两者都要。
# shared 模式只生成 OBJS.SH (列出 .so PIC object); 为每个有 OBJS.SH 的目录
# 建 OBJS.ST -> OBJS.SH 软链, 让 config/lib.in:30 的 .a 规则用同一批 .o
# (PIC) 打出 .a。.a 里是 PIC .o, 链接进静态 preloader 功能正常。
echo ">>> 生成静态库 (.a) 供 wine-preloader -static 链接..."
for dir in $(find . -name OBJS.SH | sed 's|/OBJS.SH||'); do
    ln -sf OBJS.SH "$dir/OBJS.ST"
done
(cd lib/krb5   && make libkrb5.a)   || true
(cd lib/crypto && make libk5crypto.a) || true
(cd util/et    && make libcom_err.a)  || true
cp lib/krb5/libkrb5.a lib/crypto/libk5crypto.a util/et/libcom_err.a \
   "${KRB5_INSTALL_DIR}/lib/" 2>/dev/null || true
echo ">>> 静态库生成完成"

echo ">>> 安装完成"
echo ""
echo "输出目录: ${KRB5_INSTALL_DIR}"
echo "  头文件: ${KRB5_INSTALL_DIR}/include"
echo "  库文件: ${KRB5_INSTALL_DIR}/lib"
echo "  可执行: ${KRB5_INSTALL_DIR}/bin"
echo ""
echo "=== 动态库 ==="
ls -la "${KRB5_INSTALL_DIR}/lib/"*.so* 2>/dev/null || true
echo ""
echo "=== 静态库 ==="
ls -la "${KRB5_INSTALL_DIR}/lib/"*.a 2>/dev/null || true
echo ""
echo "=== 可执行文件 ==="
ls "${KRB5_INSTALL_DIR}/bin/" 2>/dev/null || true

)