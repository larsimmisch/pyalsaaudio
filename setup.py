from __future__ import annotations

import sys
from setuptools import Extension, setup

ext_modules = []

if sys.platform.startswith("linux"):
    ext_modules = [
        Extension(
            name="alsaaudio._alsaaudio",
            sources=["src/alsaaudio.c"],
            libraries=["asound"],
        )
    ]

setup(ext_modules=ext_modules)