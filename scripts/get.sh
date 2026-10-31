#!/usr/bin/env bash
#
# RedBunny one-line installer for Linux and macOS.
#
#   curl -fsSL https://raw.githubusercontent.com/Mohamedboukerche22/redbunny/main/scripts/get.sh | bash
#
# Downloads the repository, installs the compiler, then compiles and runs
# examples/hello.redb. Installs under ~/.local by default; override with
# PREFIX=/some/path.
set -euo pipefail

REPO="${REDB_REPO:-Mohamedboukerche22/redbunny}"
BRANCH="${REDB_BRANCH:-main}"
PREFIX="${PREFIX:-$HOME/.local}"
ARCHIVE_URL="${REDB_ARCHIVE_URL:-https://github.com/$REPO/archive/refs/heads/$BRANCH.tar.gz}"

need() {
    command -v "$1" >/dev/null 2>&1
}

if ! need tar; then
    echo "error: tar is required" >&2
    exit 1
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Downloading RedBunny ($BRANCH)..."
if need curl; then
    curl -fsSL "$ARCHIVE_URL" -o "$TMP/redbunny.tar.gz"
elif need wget; then
    wget -qO "$TMP/redbunny.tar.gz" "$ARCHIVE_URL"
else
    echo "error: curl or wget is required" >&2
    exit 1
fi

mkdir -p "$TMP/src"
tar -xzf "$TMP/redbunny.tar.gz" -C "$TMP/src" --strip-components=1

echo "Installing to $PREFIX..."
PREFIX="$PREFIX" bash "$TMP/src/scripts/install.sh"

REDB="$PREFIX/bin/redb"
if [ ! -x "$REDB" ]; then
    echo "error: compiler not found at $REDB" >&2
    exit 1
fi

echo
echo "Compiling and running examples/hello.redb..."
"$REDB" "$TMP/src/examples/hello.redb" -o "$TMP/hello"
"$TMP/hello"

echo
echo "Installed: $REDB"
case ":$PATH:" in
    *":$PREFIX/bin:"*) ;;
    *) echo "Add it to PATH: export PATH=\"$PREFIX/bin:\$PATH\"" ;;
esac
