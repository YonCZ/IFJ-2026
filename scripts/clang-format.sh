#!/bin/sh
set -eu

case "$#:${1:-}" in
    0:) mode=apply ;;
    1:--check) mode=check ;;
    *)
        printf 'Usage: %s [--check]\n' "$0" >&2
        exit 2
        ;;
esac

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
formatter=${CLANG_FORMAT:-}

if [ -z "$formatter" ]; then
    for candidate in clang-format clang-format-19 clang-format-23; do
        if command -v "$candidate" >/dev/null 2>&1; then
            formatter=$candidate
            break
        fi
    done
fi

if [ -z "$formatter" ] && command -v brew >/dev/null 2>&1; then
    if llvm_prefix=$(brew --prefix llvm 2>/dev/null); then
        formatter="$llvm_prefix/bin/clang-format"
    fi
fi

if [ -z "$formatter" ] || ! command -v "$formatter" >/dev/null 2>&1; then
    printf 'clang-format not found. Set CLANG_FORMAT to its executable path.\n' >&2
    exit 1
fi

format_sources() {
    find "$repo_root/src" -type f \( -name '*.c' -o -name '*.h' \) \
        -exec "$formatter" "--style=file:$repo_root/.clang-format" "$@" {} +
}

if [ "$mode" = apply ]; then
    format_sources -i
    if format_sources --dry-run --Werror >/dev/null 2>&1; then
        exit 0
    fi
    format_sources -i
fi

format_sources --dry-run --Werror
