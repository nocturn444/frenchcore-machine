#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-only
# Prepares a Claude Code cloud session to build Frenchcore Machine on Linux.
# Runs only in remote (cloud) sessions. Never fails the session start.
set -uo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
    exit 0
fi

PACKAGES=(
    build-essential cmake ninja-build pkg-config clang-format
    libasound2-dev libjack-jackd2-dev
    libfreetype-dev libfontconfig1-dev
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev
    libxinerama-dev libxrandr-dev libxrender-dev libxi-dev
    libglu1-mesa-dev mesa-common-dev
)

missing=()
for p in "${PACKAGES[@]}"; do
    dpkg -s "$p" >/dev/null 2>&1 || missing+=("$p")
done

if [ ${#missing[@]} -gt 0 ]; then
    SUDO=""
    [ "$(id -u)" -ne 0 ] && SUDO="sudo"
    echo "[session-start] Installing build dependencies: ${missing[*]}"
    $SUDO apt-get update -qq >/dev/null 2>&1 || true
    DEBIAN_FRONTEND=noninteractive $SUDO apt-get install -y -qq --no-install-recommends "${missing[@]}" >/dev/null 2>&1 \
        || echo "[session-start] Warning: some packages failed to install; the build may not configure."
fi

# Say it loudly when the toolchain is still incomplete, so a session does not fail later with an
# obscure CMake error. The hook itself still never fails the session start.
for tool in cmake ninja clang-format; do
    command -v "$tool" >/dev/null 2>&1 \
        || echo "[session-start] WARNING: '$tool' is not available. Builds or the format check will fail until it is installed."
done

# Share the CPM download cache (JUCE, Catch2) across builds in this session.
if [ -n "${CLAUDE_ENV_FILE:-}" ]; then
    echo "export CPM_SOURCE_CACHE=\"$HOME/.cache/CPM\"" >> "$CLAUDE_ENV_FILE"
fi

exit 0
