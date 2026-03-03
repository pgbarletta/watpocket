#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path
import tempfile

import watpocket


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run watpocket Python integration test against 1.nc")
    parser.add_argument("--topology", required=True, type=Path)
    parser.add_argument("--trajectory", required=True, type=Path)
    parser.add_argument("--benchmark", required=True, type=Path)
    parser.add_argument(
        "--resnums",
        default="164,128,160,55,167,61,42,65,66",
        help="Selector list matching integration test configuration",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    selectors = watpocket.parse_residue_selectors(args.resnums)

    with tempfile.TemporaryDirectory(prefix="watpocket-py-integration-") as tmpdir:
        output_csv = Path(tmpdir) / "output.csv"

        summary = watpocket.analyze_trajectory_files(
            str(args.topology),
            str(args.trajectory),
            selectors,
            str(output_csv),
            None,
        )

        if summary.frames_processed == 0:
            raise AssertionError("expected at least one processed frame")

        generated = output_csv.read_text(encoding="utf-8")
        expected = args.benchmark.read_text(encoding="utf-8")
        if generated != expected:
            raise AssertionError("generated CSV does not match benchmark")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
