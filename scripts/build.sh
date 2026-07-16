#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_TYPE=${1:-debug}

cd "$ROOT_DIR"

case "$BUILD_TYPE" in
    debug | release)
        make "$BUILD_TYPE"
        ;;
    *)
        echo "Usage: $0 [debug|release]" >&2
        exit 2
        ;;
esac
