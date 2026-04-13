#!/bin/bash
# HAP Signing Script
# Function: Copy unsigned HAP file to signing tool directory, sign it, and copy signed file back to output directory

set -e  # Exit on error

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Define path variables
UNSIGNED_HAP="${SCRIPT_DIR}/out/wine_hap/entry-default-unsigned.hap"
SIGN_TOOL_DIR="${SCRIPT_DIR}/ohos/tools/signtool"
HAPS_DIR="${SIGN_TOOL_DIR}/haps"
SIGN_SCRIPT="${SIGN_TOOL_DIR}/sign_release.sh"
RESULTS_DIR="${SIGN_TOOL_DIR}/results_release"
OUTPUT_DIR="${SCRIPT_DIR}/out/wine_hap"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Print colored messages
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Check if file exists
check_file() {
    if [ ! -f "$1" ]; then
        print_error "File not found: $1"
        exit 1
    fi
}

# Check if directory exists, create if not
ensure_dir() {
    if [ ! -d "$1" ]; then
        print_info "Creating directory: $1"
        mkdir -p "$1"
    fi
}

# Cleanup function
cleanup() {
    if [ -d "${HAPS_DIR}" ]; then
        print_info "Cleaning temporary directory: ${HAPS_DIR}"
        rm -rf "${HAPS_DIR:?}"/* 2>/dev/null || true
    fi
    if [ -d "${RESULTS_DIR}" ]; then
        print_info "Cleaning signature result directory: ${RESULTS_DIR}"
        rm -rf "${RESULTS_DIR:?}"/* 2>/dev/null || true
    fi
}

# Main function
main() {
    print_info "Starting HAP signing process..."
    echo

    # 1. Check if unsigned HAP file exists
    print_info "Step 1: Check unsigned HAP file"
    check_file "${UNSIGNED_HAP}"
    print_info "Found unsigned file: ${UNSIGNED_HAP}"
    echo

    # 2. Create necessary directories
    print_info "Step 2: Create necessary directories"
    ensure_dir "${HAPS_DIR}"
    ensure_dir "${RESULTS_DIR}"
    echo

    # 3. Clean up previous temporary files
    print_info "Step 3: Clean up previous temporary files"
    cleanup
    echo

    # 4. Copy unsigned HAP file to signing tool directory
    print_info "Step 4: Copy unsigned HAP file to signing tool directory"
    cp "${UNSIGNED_HAP}" "${HAPS_DIR}/"
    print_info "Copy completed: ${UNSIGNED_HAP} -> ${HAPS_DIR}/"
    echo

    # 5. Check if signing script exists
    print_info "Step 5: Check signing script"
    check_file "${SIGN_SCRIPT}"
    print_info "Found signing script: ${SIGN_SCRIPT}"
    echo

    # 6. Execute signing
    print_info "Step 6: Execute HAP signing"
    echo "----------------------------------------"
    cd "${SIGN_TOOL_DIR}" || exit 1
    if ! bash sign_release.sh -d haps -o results_release; then
        print_error "Signing failed"
        exit 1
    fi
    echo "----------------------------------------"
    echo

    # 7. Check signing results
    print_info "Step 7: Check signing results"
    SIGNED_FILES=$(find "${RESULTS_DIR}" -name "*.hap" -type f 2>/dev/null || echo "")
    if [ -z "${SIGNED_FILES}" ]; then
        print_error "No signed HAP files found in result directory"
        exit 1
    fi
    print_info "Found signed HAP files:"
    echo "${SIGNED_FILES}"
    echo

    # 8. Copy signed files to output directory
    print_info "Step 8: Copy signed files to output directory"
    for signed_file in ${SIGNED_FILES}; do
        filename=$(basename "${signed_file}")
        # Replace 'unsigned' with 'signed' in filename
        new_filename=$(echo "${filename}" | sed 's/unsigned/signed/')
        cp "${signed_file}" "${OUTPUT_DIR}/${new_filename}"
        print_info "Copy completed: ${signed_file} -> ${OUTPUT_DIR}/${new_filename}"
    done
    echo

    # 9. Clean up temporary files
    print_info "Step 9: Clean up temporary files"
    cleanup
    echo

    # 10. Complete
    print_info "HAP signing process completed!"
    echo
    print_info "Signed files located at: ${OUTPUT_DIR}/"
    print_info "File list:"
    find "${OUTPUT_DIR}" -name "*.hap" -type f -exec ls -lh {} \;
}

# 执行主函数
main "$@"
