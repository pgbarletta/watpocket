#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/pypi_release_wheels.sh [--testpypi] [--build-only]

Builds and uploads wheel artifacts only (no sdist) using the version from the
exact git tag at HEAD (vX.Y.Z or X.Y.Z).

Environment:
  PYTHON_BIN            Python executable to use (default: python)
  TWINE_REPOSITORY      Override repository name (default: pypi, or testpypi with --testpypi)
  TWINE_USERNAME        Optional Twine username override
  TWINE_PASSWORD        Optional Twine password/token override
                        If unset, Twine can read credentials from ~/.pypirc or keyring.
                        On Linux, auditwheel is used to repair linux_x86_64 wheels
                        into manylinux wheels before upload.
EOF
}

TESTPYPI=0
BUILD_ONLY=0

while (($# > 0)); do
  case "$1" in
  --testpypi)
    TESTPYPI=1
    shift
    ;;
  --build-only)
    BUILD_ONLY=1
    shift
    ;;
  -h | --help)
    usage
    exit 0
    ;;
  *)
    echo "Unknown argument: $1" >&2
    usage >&2
    exit 2
    ;;
  esac
done

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

if ! git diff --quiet || ! git diff --cached --quiet; then
  echo "Refusing to release from a dirty working tree." >&2
  exit 1
fi

TAG="$(git describe --tags --exact-match)"
VERSION="$(sed -E 's/^v//' <<<"${TAG}")"
if [[ ! "${VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Release tag '${TAG}' is invalid. Expected vX.Y.Z or X.Y.Z." >&2
  exit 1
fi

GIT_SHA="$(git rev-parse HEAD)"
SHORT_SHA="$(git rev-parse --short=8 HEAD)"

PYTHON_BIN="${PYTHON_BIN:-python}"
REPOSITORY="${TWINE_REPOSITORY:-pypi}"
if [[ "${TESTPYPI}" -eq 1 ]]; then
  REPOSITORY="testpypi"
fi

echo "Release tag: ${TAG}"
echo "Version: ${VERSION}"
echo "Git SHA: ${GIT_SHA}"

BUILD_CMAKE_ARGS="-DWATPOCKET_VERSION=${VERSION} -DGIT_SHA=${GIT_SHA}"
if [[ -n "${CMAKE_ARGS:-}" ]]; then
  BUILD_CMAKE_ARGS="${BUILD_CMAKE_ARGS} ${CMAKE_ARGS}"
fi

WATPOCKET_VERSION="${VERSION}" CMAKE_ARGS="${BUILD_CMAKE_ARGS}" "${PYTHON_BIN}" -m build --wheel

WHEELS=(dist/watpocket-"${VERSION}"-*.whl)
if [[ ! -e "${WHEELS[0]}" ]]; then
  echo "No wheels found for version ${VERSION} in dist/." >&2
  exit 1
fi

if [[ "$(uname -s)" == "Linux" ]]; then
  NEED_REPAIR=()
  for wheel in "${WHEELS[@]}"; do
    if [[ "${wheel}" == *linux_x86_64.whl ]]; then
      NEED_REPAIR+=("${wheel}")
    fi
  done

  if [[ "${#NEED_REPAIR[@]}" -gt 0 ]]; then
    echo "Repairing Linux wheel tag(s) with auditwheel to produce manylinux wheel(s)..."
    REPAIR_DIR="$(mktemp -d)"
    trap 'rm -rf "${BUILD_RUN_DIR}" "${REPAIR_DIR}"' EXIT

    if "${PYTHON_BIN}" -c "import auditwheel" >/dev/null 2>&1; then
      AUDITWHEEL_RUN=("${PYTHON_BIN}" -m auditwheel)
    elif command -v auditwheel >/dev/null 2>&1; then
      AUDITWHEEL_RUN=(auditwheel)
    else
      echo "auditwheel is required on Linux to publish to PyPI (linux_x86_64 is rejected)." >&2
      echo "Install it (for example: ${PYTHON_BIN} -m pip install auditwheel) and retry." >&2
      exit 1
    fi

    for wheel in "${NEED_REPAIR[@]}"; do
      "${AUDITWHEEL_RUN[@]}" repair --strip -w "${REPAIR_DIR}" "${wheel}"
      rm -f "${wheel}"
    done

    cp "${REPAIR_DIR}"/watpocket-"${VERSION}"-*.whl dist/
    WHEELS=(dist/watpocket-"${VERSION}"-*.whl)
  fi
fi

echo "Built wheel(s):"
ls -1 "${WHEELS[@]}"

if [[ "${BUILD_ONLY}" -eq 1 ]]; then
  echo "Build complete; skipping upload (--build-only)."
  exit 0
fi

TWINE_ARGS=()
if [[ -n "${TWINE_USERNAME:-}" ]]; then
  TWINE_ARGS+=(--username "${TWINE_USERNAME}")
fi
if [[ -n "${TWINE_PASSWORD:-}" ]]; then
  TWINE_ARGS+=(--password "${TWINE_PASSWORD}")
fi

"${PYTHON_BIN}" -m twine upload \
  --repository "${REPOSITORY}" \
  --non-interactive \
  --skip-existing \
  "${TWINE_ARGS[@]}" \
  "${WHEELS[@]}"

echo "Uploaded wheels for ${VERSION} (${SHORT_SHA}) to ${REPOSITORY}."
