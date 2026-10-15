"""Tuna documentation screenshot toolkit.

A self-contained, standard-library-only Python package that stages an isolated
Dolphin profile, drives the emulator through its own TAS input controls and
captures cropped PNG screenshots for the project documentation.

No PowerShell, no shell scripts, no third-party packages, no literal
machine-specific paths: every host path is supplied as an argument.
"""

__all__ = ["win32", "uia", "image", "dolphin"]
