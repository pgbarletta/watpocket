#!/usr/bin/env python3
"""Dynamic version provider for scikit-build-core.

Derives the package version from an exact git tag on HEAD.
Accepted tags: vX.Y.Z or X.Y.Z
"""

from __future__ import annotations

import re
import os
import subprocess
from pathlib import Path


TAG_PATTERN = re.compile(r"^v?([0-9]+\.[0-9]+\.[0-9]+)$")


def dynamic_metadata(field: str, settings: dict[str, object] | None = None) -> str:
    if field != "version":
        raise ValueError("Only the 'version' field is supported")

    if settings:
        raise ValueError("This provider does not accept inline settings")

    env_version = os.environ.get("WATPOCKET_VERSION", "").strip()
    if env_version:
        match = TAG_PATTERN.fullmatch(env_version)
        if match is None:
            raise ValueError(
                f"Invalid WATPOCKET_VERSION '{env_version}'. Expected X.Y.Z."
            )
        return match.group(1)

    repo_root = Path(__file__).resolve().parents[1]

    try:
        tag = subprocess.check_output(
            ["git", "-C", str(repo_root), "describe", "--tags", "--exact-match"],
            text=True,
        ).strip()
    except subprocess.CalledProcessError as exc:
        raise ValueError(
            "Unable to derive package version: HEAD is not on an exact git tag. "
            "Create or check out a release tag (vX.Y.Z or X.Y.Z)."
        ) from exc

    match = TAG_PATTERN.fullmatch(tag)
    if match is None:
        raise ValueError(
            f"Invalid release tag '{tag}'. Expected vX.Y.Z or X.Y.Z."
        )

    return match.group(1)


def get_requires_for_dynamic_metadata(
    _settings: dict[str, object] | None = None,
) -> list[str]:
    return []
