#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TEST_MODE=${1:-all}

cd "$ROOT_DIR"

case "$TEST_MODE" in
    unit)
        make unit-test
        ;;
    all)
        make test
        ;;
    sanitizer)
        make sanitizer-test
        ;;
    *)
        echo "Usage: $0 [unit|all|sanitizer]" >&2
        exit 2
        ;;
esac
