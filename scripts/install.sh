#!/usr/bin/env bash
#
# RedBunny compiler installer for Linux and macOS.
#
set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"
BUILD_DIR="${BUILD_DIR:-build}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

need() {
  command -v "$1" >/dev/null 2>&1
}

install_deps_linux() {
  if need apt-get; then
    sudo apt-get update
    sudo apt-get install -y llvm-dev clang cmake build-essential
  elif need dnf; then
    sudo dnf install -y llvm-devel clang cmake gcc make
  elif need pacman; then
    sudo pacman -S --needed --noconfirm llvm clang cmake base-devel
  elif need zypper; then
    sudo zypper install -y llvm-devel clang cmake gcc make
  else
    echo "error: no supported package manager found" >&2
    echo "install LLVM development files, clang and cmake manually" >&2
    exit 1
  fi
}

install_deps_macos() {
  if ! need brew; then
    echo "error: Homebrew is required on macOS (https://brew.sh)" >&2
    exit 1
  fi
  brew install llvm cmake
  LLVM_PREFIX="$(brew --prefix llvm)"
  export PATH="$LLVM_PREFIX/bin:$PATH"
  export LLVM_DIR="$LLVM_PREFIX/lib/cmake/llvm"
}

if ! need cmake || ! need clang || ! need llvm-config; then
  case "$(uname -s)" in
  Darwin) install_deps_macos ;;
  Linux) install_deps_linux ;;
  *)
    echo "error: unsupported operating system: $(uname -s)" >&2
    exit 1
    ;;
  esac
fi

if ! need llvm-config; then
  echo "error: llvm-config is still not available" >&2
  exit 1
fi

echo "Using LLVM $(llvm-config --version)"
echo "Building RedBunny in $ROOT/$BUILD_DIR"

cmake -S "$ROOT" -B "$ROOT/$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX"

cmake --build "$ROOT/$BUILD_DIR" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"

mkdir -p "$PREFIX" 2>/dev/null || true

if [ -w "$PREFIX" ]; then
  cmake --install "$ROOT/$BUILD_DIR"
else
  sudo cmake --install "$ROOT/$BUILD_DIR"
fi

echo
echo "RedBunny installed."
echo "  compiler: $PREFIX/bin/redb"
echo "  runtime:  $PREFIX/lib/libredb_runtime.a"
echo "  mathlib:  $PREFIX/share/redb/lib/mathlib.redb"
echo
echo "Try it:"
echo "  redb --version"
echo "  redb examples/hello.redb -o hello && ./hello"
