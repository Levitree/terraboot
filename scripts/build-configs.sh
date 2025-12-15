#!/bin/bash
# Utility to build every configuration in a directory and store artifacts per config.
# Usage: build-configs.sh [config_dir] [artifact_dir] [config_name]
set -euo pipefail
shopt -s nullglob

CONFIG_DIR=${1:-configs}
ARTIFACT_DIR=${2:-build-artifacts}
REQUESTED_CONFIG=${3:-}

if [[ -n "${REQUESTED_CONFIG}" ]]; then
    REQUESTED_CONFIG=${REQUESTED_CONFIG%.config}
    CONFIG_FILES=("${CONFIG_DIR}/${REQUESTED_CONFIG}.config")
    if [[ ! -f "${CONFIG_FILES[0]}" ]]; then
        echo "Configuration ${REQUESTED_CONFIG} not found in ${CONFIG_DIR}" >&2
        exit 1
    fi
else
    CONFIG_FILES=("${CONFIG_DIR}"/*.config)
fi

if (( ${#CONFIG_FILES[@]} == 0 )); then
    echo "No .config files found in ${CONFIG_DIR}" >&2
    exit 1
fi

mkdir -p "${ARTIFACT_DIR}"

if command -v nproc >/dev/null 2>&1; then
    JOBS=$(nproc)
elif command -v sysctl >/dev/null 2>&1; then
    JOBS=$(sysctl -n hw.ncpu)
elif command -v getconf >/dev/null 2>&1; then
    JOBS=$(getconf _NPROCESSORS_ONLN)
else
    JOBS=1
fi

for CONFIG_PATH in "${CONFIG_FILES[@]}"; do
    NAME=$(basename "${CONFIG_PATH}" .config)
    echo "::group::Terraboot build (${NAME})"
    make distclean
    cp "${CONFIG_PATH}" .config
    make olddefconfig
    make -j"${JOBS}"
    DEST="${ARTIFACT_DIR}/${NAME}"
    rm -rf "${DEST}"
    mkdir -p "${DEST}"
    cp -R out/. "${DEST}/"
    echo "::endgroup::"
done
