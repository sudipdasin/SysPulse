#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline update admission policy. No network, key enrollment or installation."""
import re

_VERSION = re.compile(r"(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})(?:-(alpha|beta)\.(0|[1-9][0-9]{0,5}))?", re.ASCII)


def parse_version(value):
    match = _VERSION.fullmatch(value) if isinstance(value, str) else None
    if match is None:
        raise ValueError("unsupported release version")
    major, minor, patch, stage, sequence = match.groups()
    return (int(major), int(minor), int(patch), {"alpha": 0, "beta": 1, None: 2}[stage], int(sequence or 0))


def admit(installed, candidate, channel):
    """Require a strictly newer release, with explicit opt-in to previews.

    Call only after authenticating the candidate manifest and installed version.
    This check is not persistent replay protection and does not authenticate inputs.
    """
    if channel not in ("stable", "preview"):
        raise ValueError("unsupported update channel")
    old, new = parse_version(installed), parse_version(candidate)
    if channel == "stable" and new[3] != 2:
        raise ValueError("preview release requires preview channel")
    if new <= old:
        raise ValueError("release is not newer")
    return candidate
