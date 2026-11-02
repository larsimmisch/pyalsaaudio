"""Actual default-VCS consumer gate; hardware absence is reported, never called PASS."""

from __future__ import annotations

import base64
from collections.abc import Callable
import concurrent.futures
import csv
import ctypes
import ctypes.util
import hashlib
import importlib.metadata
import io
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import threading
from typing import cast
import urllib.request
import zipfile

import alsaaudio as alsa


def gil_enabled() -> bool:
    # Bind CPython's actual runtime diagnostic; do not infer state from build flags.
    diagnostic = cast(Callable[[], bool], getattr(sys, "_is_gil_enabled"))
    return diagnostic()


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    out = root / "pyalsaaudio-ci-evidence"
    out.mkdir(exist_ok=False)
    assert sys.flags.gil is None and not gil_enabled()
    assert not any(
        os.environ.get(key)
        for key in (
            "PYTHON_GIL",
            "ALSA_CONFIG_PATH",
            "ALSA_CONFIG_DIR",
            "ALSA_PLUGIN_DIR",
        )
    )
    dist = importlib.metadata.distribution("pyalsaaudio")
    assert dist.version == "0.11.0+stablekite.1"
    direct = json.loads(dist.read_text("direct_url.json") or "null")
    assert direct["url"] == "https://github.com/StableKite/pyalsaaudio.git"
    assert direct["vcs_info"]["commit_id"] == os.environ["GITHUB_SHA"]
    assert not direct["vcs_info"].get("requested_revision")
    record = dist.read_text("RECORD")
    assert record
    verified = 0
    for name, checksum, size in csv.reader(record.splitlines()):
        if checksum:
            located = dist.locate_file(name)
            assert isinstance(located, Path)
            path = located.resolve(strict=True)
            assert path.is_relative_to(Path(sys.prefix).resolve())
            algorithm, expected = checksum.split("=", 1)
            digest = hashlib.file_digest(path.open("rb"), algorithm).digest()
            assert base64.urlsafe_b64encode(digest).rstrip(b"=").decode() == expected
            assert path.stat().st_size == int(size)
            verified += 1
    runtime = Path(alsa.__file__).parent / "pyalsaaudio_runtime"
    libraries = list(runtime.glob("libasound*.so*"))
    assert len(libraries) <= 1
    native = str(libraries[0]) if libraries else ctypes.util.find_library("asound")
    assert native
    sdk = ctypes.CDLL(native)
    sdk.snd_config_topdir.argtypes = []
    sdk.snd_config_topdir.restype = ctypes.c_char_p
    config = sdk.snd_config_topdir()
    assert isinstance(config, bytes)
    config_path = Path(config.decode())
    assert (config_path / "alsa.conf").is_file()
    assert "/tmp/vendor" not in str(config_path)
    if libraries:
        assert config_path == runtime / "alsa-config"
    sdk.snd_asoundlib_version.argtypes = []
    sdk.snd_asoundlib_version.restype = ctypes.c_char_p
    sdk_version = sdk.snd_asoundlib_version()
    assert isinstance(sdk_version, bytes)
    assert alsa.asoundlib_version() == sdk_version.decode()
    resources = json.loads((runtime / "CONFIGURATION_SHA256.json").read_text())
    for name, expected in resources["files"].items():
        assert (
            hashlib.file_digest((runtime / name).open("rb"), "sha256").hexdigest()
            == expected
        )
    original_files = 0
    if libraries:
        manifest = json.loads((root / "binary_wheels.json").read_text())
        asset = manifest["artifact"]
        with urllib.request.urlopen(asset["url"], timeout=60) as response:
            content = response.read()
        assert hashlib.sha256(content).hexdigest() == asset["sha256"]
        with zipfile.ZipFile(io.BytesIO(content)) as package:
            assert package.testzip() is None
            original_record = next(
                n for n in package.namelist() if n.endswith(".dist-info/RECORD")
            )
            for name, checksum, size in csv.reader(
                package.read(original_record).decode().splitlines()
            ):
                if checksum:
                    original = package.read(name)
                    installed = dist.locate_file(name)
                    assert isinstance(installed, Path)
                    assert installed.read_bytes() == original, name
                    original_files += 1

    barrier = threading.Barrier(4)

    def worker(index: int) -> tuple[int, int]:
        barrier.wait(timeout=15)
        frames = 0
        for iteration in range(16):
            pcm = alsa.PCM(
                type=alsa.PCM_CAPTURE,
                device="null" if iteration % 2 else "plug:null",
                rate=48000,
                channels=2,
                format=alsa.PCM_FORMAT_S16_LE,
                periodsize=64,
            )
            try:
                for _attempt in range(8):
                    count, data = pcm.read()
                    assert count == 64 and len(data) == 256
                    frames += count
            finally:
                pcm.close()
        return threading.get_native_id(), frames

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(worker, range(4)))
    assert len({item[0] for item in results}) == 4
    assert sum(item[1] for item in results) == 32768
    result: dict[str, object] = {
        "python": sys.version,
        "architecture": platform.machine(),
        "GIL": gil_enabled(),
        "gil_flag": sys.flags.gil,
        "direct_url": direct,
        "version": dist.version,
        "all_installed_RECORD": verified,
        "original_wheel_files_identical": original_files,
        "native_backend": "bundled SDK"
        if libraries
        else "original source backend with distribution SDK",
        "SDK_version": sdk_version.decode(),
        "SDK_config_path": str(config_path),
        "resource_files": len(resources["files"]),
        "native_workers": results,
    }
    for name, path in (
        ("PCM_lifetime", root / "tests/test_pcm_lifetime.py"),
        ("all_original_tests", root / "tests/test.py"),
    ):
        completed = subprocess.run(
            [sys.executable, "-I", "-B", str(path)],
            capture_output=True,
            timeout=120,
            check=False,
        )
        (out / (name + ".stdout.log")).write_bytes(completed.stdout)
        (out / (name + ".stderr.log")).write_bytes(completed.stderr)
        result[name + "_exit_code"] = completed.returncode
        if name == "PCM_lifetime":
            assert completed.returncode == 0, completed.stderr.decode(errors="replace")
        elif completed.returncode:
            errors = re.findall(
                r"^alsaaudio\.ALSAAudioError: (.+)$",
                completed.stderr.decode(errors="replace"),
                flags=re.MULTILINE,
            )
            assert not alsa.card_indexes()
            assert len(errors) == 9 and set(errors) == {
                "No such file or directory [default]"
            }
            assert "Ran 11 tests" in completed.stderr.decode()
            result["original_hardware_status"] = (
                "UNAVAILABLE: 2 PASS, 9 hardware errors; no default sound card"
            )
        else:
            result["original_hardware_status"] = "PASS"
    assert sys.flags.gil is None and not gil_enabled()
    result["scoped_virtual_PCM_and_default_status"] = "PASS"
    (out / "RESULT.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    main()
