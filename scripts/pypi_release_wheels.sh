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
  TWINE_USERNAME        Twine username (default: __token__)
  TWINE_PASSWORD        Twine token/password
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

echo "Built wheel(s):"
ls -1 dist/watpocket-"${VERSION}"-*.whl

if [[ "${BUILD_ONLY}" -eq 1 ]]; then
  echo "Build complete; skipping upload (--build-only)."
  exit 0
fi

if [[ -z "${TWINE_PASSWORD:-}" ]]; then
  echo "TWINE_PASSWORD is not set; refusing to upload." >&2
  exit 1
fi

TWINE_USERNAME="${TWINE_USERNAME:-__token__}" \
  "${PYTHON_BIN}" -m twine upload \
  --repository "${REPOSITORY}" \
  --non-interactive \
  --skip-existing \
  dist/watpocket-"${VERSION}"-*.whl

echo "Uploaded wheels for ${VERSION} (${SHORT_SHA}) to ${REPOSITORY}."
