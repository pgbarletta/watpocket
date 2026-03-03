#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "Usage: $0 <watpocket_bin> <data_dir> <output_dir>" >&2
  exit 2
fi

watpocket_bin="$1"
data_dir="$2"
output_dir="$3"
resnums="164,128,160,55,167,61,42,65,66"

if [[ ! -x "${watpocket_bin}" ]]; then
  echo "Error: watpocket binary is missing or not executable: ${watpocket_bin}" >&2
  exit 1
fi

if [[ ! -d "${data_dir}" ]]; then
  echo "Error: data directory does not exist: ${data_dir}" >&2
  exit 1
fi

mkdir -p "${output_dir}"

(
  cd "${data_dir}"
  "${watpocket_bin}" 0complex_wcn.pdb -d "${output_dir}/0complex_wcn.py" --resnums "${resnums}"
  "${watpocket_bin}" 1complex_wcn.pdb -d "${output_dir}/1complex_wcn.py" --resnums "${resnums}"
)
