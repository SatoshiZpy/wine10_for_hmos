#!/bin/bash

# Wine OHOS GStreamer 交叉编译脚本
# 此脚本用于为 OHOS ARM64 架构交叉编译 GStreamer

set -e

# 项目根目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}"
GSTREAMER_DIR="${ROOT_DIR}/third_party/gstreamer"

BUILD_DIR="${GSTREAMER_DIR}/build_ohos"
INSTALL_DIR="${GSTREAMER_DIR}/install_ohos"
CROSS_FILE="${GSTREAMER_DIR}/ohos-aarch64.txt"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

echo_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

echo_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 生成交叉编译配置文件
gen_cross_file() {
    echo_info "生成交叉编译配置文件..."

    local gen_script="${GSTREAMER_DIR}/gen_cross_file.py"

    if [ ! -f "${gen_script}" ]; then
        echo_error "交叉编译配置生成脚本不存在: ${gen_script}"
        exit 1
    fi

    python3 "${gen_script}"

    if [ ! -f "${CROSS_FILE}" ]; then
        echo_error "交叉编译配置文件生成失败: ${CROSS_FILE}"
        exit 1
    fi

    echo_info "交叉编译配置文件已生成: ${CROSS_FILE}"
}

# 检查依赖
check_dependencies() {
    echo_info "检查构建依赖..."

    if ! command -v meson &> /dev/null; then
        echo_error "Meson 未安装，请先安装 Meson"
        exit 1
    fi

    if [ ! -d "${GSTREAMER_DIR}" ]; then
        echo_error "GStreamer 源码目录不存在: ${GSTREAMER_DIR}"
        exit 1
    fi

    # 动态生成交叉编译配置文件
    gen_cross_file

    echo_info "依赖检查通过"
}

# 清理旧的构建
clean_build() {
    echo_info "清理旧的构建目录..."
    if [ -d "${BUILD_DIR}" ]; then
        rm -rf "${BUILD_DIR}"
        echo_info "已删除: ${BUILD_DIR}"
    fi

    if [ -d "${INSTALL_DIR}" ]; then
        rm -rf "${INSTALL_DIR}"
        echo_info "已删除: ${INSTALL_DIR}"
    fi
}

# 配置 Meson
configure_meson() {
    echo_info "配置 Meson 交叉编译..."

    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"

    meson setup --cross-file="${CROSS_FILE}" \
        -Dlibav=enabled \
        -Dbase=enabled \
        -Dgood=enabled \
        -Dbad=disabled \
        -Dugly=disabled \
        -Dlibnice=disabled \
        -Dtls=disabled \
        -Dtests=disabled \
        -Dglib:selinux=disabled \
        -Dglib:libelf=disabled \
        --default-library=shared

    echo_info "Meson 配置完成"
}

# 编译 GStreamer
build_gstreamer() {
    echo_info "开始编译 GStreamer..."

    cd "${BUILD_DIR}"
    meson compile

    echo_info "GStreamer 编译完成"
}

# 安装 GStreamer
install_gstreamer() {
    echo_info "安装 GStreamer 到 ${INSTALL_DIR}..."

    cd "${BUILD_DIR}"
    meson install

    echo_info "GStreamer 安装完成"
}

# 验证安装
verify_installation() {
    echo_info "验证安装..."

    if [ ! -d "${INSTALL_DIR}" ]; then
        echo_error "安装目录不存在: ${INSTALL_DIR}"
        exit 1
    fi

    # 检查核心库文件
    local required_libs=(
        "libgstreamer-1.0.so"
        "libgstbase-1.0.so"
    )

    for lib in "${required_libs[@]}"; do
        if [ ! -f "${INSTALL_DIR}/lib/${lib}" ]; then
            echo_error "缺少必需的库文件: ${lib}"
            exit 1
        fi
        echo_info "找到库文件: ${lib}"
    done

    # 检查架构
    local machine=$(readelf -h "${INSTALL_DIR}/lib/libgstreamer-1.0.so" 2>/dev/null | grep "Machine:" | awk '{print $2}')
    if [[ "${machine}" != *"AArch64"* ]]; then
        echo_error "库文件架构不正确，期望 AArch64，实际: ${machine}"
        exit 1
    fi

    echo_info "架构验证通过: AArch64"
    echo_info "安装验证通过"
}

# 显示安装信息
show_install_info() {
    echo ""
    echo_info "========================================"
    echo_info "GStreamer OHOS 编译完成"
    echo_info "========================================"
    echo ""
    echo "安装目录: ${INSTALL_DIR}"
    echo ""
    echo "库文件位置:"
    ls -lh "${INSTALL_DIR}/lib/"*.so 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
    echo ""
    echo "头文件位置:"
    ls -d "${INSTALL_DIR}/include"/* 2>/dev/null | xargs -I {} basename {} | while read dir; do
        echo "  ${dir}"
    done
    echo ""
    echo_info "现在可以运行 ./build.sh 来编译 Wine，GStreamer 支持将自动启用"
    echo ""
}

# 主函数
main() {
    echo_info "========================================"
    echo_info "Wine OHOS GStreamer 交叉编译脚本"
    echo_info "========================================"
    echo ""

    # 解析命令行参数
    CLEAN_BUILD=0
    SKIP_BUILD=0
    SKIP_INSTALL=0

    while [[ $# -gt 0 ]]; do
        case $1 in
            --clean)
                CLEAN_BUILD=1
                shift
                ;;
            --skip-build)
                SKIP_BUILD=1
                shift
                ;;
            --skip-install)
                SKIP_INSTALL=1
                shift
                ;;
            -h|--help)
                echo "用法: $0 [选项]"
                echo ""
                echo "选项:"
                echo "  --clean          清理旧的构建目录"
                echo "  --skip-build     跳过编译步骤"
                echo "  --skip-install   跳过安装步骤"
                echo "  -h, --help       显示此帮助信息"
                echo ""
                echo "示例:"
                echo "  $0              # 完整编译流程"
                echo "  $0 --clean      # 清理并重新编译"
                echo "  $0 --skip-build # 只执行安装"
                exit 0
                ;;
            *)
                echo_error "未知选项: $1"
                echo "使用 -h 或 --help 查看帮助"
                exit 1
                ;;
        esac
    done

    # 执行构建流程
    check_dependencies

    if [ ${CLEAN_BUILD} -eq 1 ]; then
        clean_build
    fi

    configure_meson

    if [ ${SKIP_BUILD} -eq 0 ]; then
        build_gstreamer
    else
        echo_warn "跳过编译步骤"
    fi

    if [ ${SKIP_INSTALL} -eq 0 ]; then
        install_gstreamer
        verify_installation
        show_install_info
    else
        echo_warn "跳过安装步骤"
    fi
}

# 运行主函数
main "$@"
