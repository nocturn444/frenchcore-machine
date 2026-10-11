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
    libasound2-dev libjack-jackd2-dev libcurl4-openssl-dev
    libfreetype-dev libfontconfig1-dev
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev
    libxinerama-dev libxrandr-dev libxrender-dev
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

# Share the CPM download cache (JUCE, Catch2) across builds in this session.
if [ -n "${CLAUDE_ENV_FILE:-}" ]; then
    echo "export CPM_SOURCE_CACHE=\"$HOME/.cache/CPM\"" >> "$CLAUDE_ENV_FILE"
fi

exit 0
