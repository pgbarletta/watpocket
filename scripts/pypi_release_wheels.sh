#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/pypi_release_wheels.sh [--testpypi] [--build-only]

Builds and uploads wheel artifacts only (no sdist) using the version from the
exact git tag at HEAD (vX.Y.Z or X.Y.Z).

Environment:
  PYTHON_BIN            Python executable to use (default: .venv/bin/python,
                        then python3, then python)
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
REPAIR_DIR=""

cleanup() {
  if [[ -n "${REPAIR_DIR}" && -d "${REPAIR_DIR}" ]]; then
    rm -rf "${REPAIR_DIR}"
  fi
}

trap cleanup EXIT

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

resolve_python_bin() {
  local candidate="${1:-}"

  if [[ -n "${candidate}" ]]; then
    if [[ "${candidate}" = /* ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi

    if [[ "${candidate}" == */* ]]; then
      local resolved="${ROOT_DIR}/${candidate}"
      if [[ -x "${resolved}" ]]; then
        printf '%s\n' "${resolved}"
        return 0
      fi
      printf '%s\n' "${resolved}"
      return 0
    fi

    command -v "${candidate}"
    return 0
  fi

  if [[ -x "${ROOT_DIR}/.venv/bin/python" ]]; then
    printf '%s\n' "${ROOT_DIR}/.venv/bin/python"
    return 0
  fi

  if command -v python3 >/dev/null 2>&1; then
    command -v python3
    return 0
  fi

  command -v python
}

PYTHON_BIN="$(resolve_python_bin "${PYTHON_BIN:-}")"
REPOSITORY="${TWINE_REPOSITORY:-pypi}"
if [[ "${TESTPYPI}" -eq 1 ]]; then
  REPOSITORY="testpypi"
fi

python_has_modules() {
  local modules=("$@")
  local code="import importlib.util, sys; missing=[name for name in sys.argv[1:] if importlib.util.find_spec(name) is None]; raise SystemExit(1 if missing else 0)"
  "${PYTHON_BIN}" -c "${code}" "${modules[@]}"
}

echo "Release tag: ${TAG}"
echo "Version: ${VERSION}"
echo "Git SHA: ${GIT_SHA}"

BUILD_CMAKE_ARGS="-DWATPOCKET_VERSION=${VERSION} -DGIT_SHA=${GIT_SHA}"
if [[ -n "${CMAKE_ARGS:-}" ]]; then
  BUILD_CMAKE_ARGS="${BUILD_CMAKE_ARGS} ${CMAKE_ARGS}"
fi

if [[ -z "${CMAKE_GENERATOR:-}" ]] && ! command -v ninja >/dev/null 2>&1; then
  export CMAKE_GENERATOR="Unix Makefiles"
fi

if ! python_has_modules build; then
  echo "Selected Python (${PYTHON_BIN}) needs the 'build' package." >&2
  echo "Install them with: uv pip install -e \".[dev]\" --no-progress" >&2
  exit 1
fi

if [[ "${BUILD_ONLY}" -ne 1 ]] && ! python_has_modules twine; then
  echo "Selected Python (${PYTHON_BIN}) needs the 'twine' package for upload." >&2
  echo "Install it with: uv pip install -e \".[dev]\" --no-progress" >&2
  exit 1
fi

if [[ "$(uname -s)" == "Linux" ]]; then
  if ! python_has_modules auditwheel && ! command -v auditwheel >/dev/null 2>&1; then
    echo "Linux releases require auditwheel to repair linux_x86_64 wheels into manylinux wheels." >&2
    echo "Install it with: uv pip install -e \".[dev]\" --no-progress" >&2
    exit 1
  fi

  if ! command -v patchelf >/dev/null 2>&1; then
    echo "Linux releases require patchelf on PATH for auditwheel repair." >&2
    echo "Add patchelf to PATH before running this script." >&2
    exit 1
  fi
fi

WATPOCKET_VERSION="${VERSION}" CMAKE_ARGS="${BUILD_CMAKE_ARGS}" \
  "${PYTHON_BIN}" -m build --wheel --no-isolation --outdir "${ROOT_DIR}/dist" "${ROOT_DIR}"

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

    REPAIRED_WHEELS=("${REPAIR_DIR}"/watpocket-"${VERSION}"-*.whl)
    if [[ ! -e "${REPAIRED_WHEELS[0]}" ]]; then
      echo "auditwheel did not produce repaired wheels for ${VERSION}." >&2
      exit 1
    fi

    cp "${REPAIRED_WHEELS[@]}" dist/
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
