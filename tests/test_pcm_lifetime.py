"""Real null/file PCM lifetime tests; no SDK stand-ins or ALSA/GIL overrides."""
from __future__ import annotations

import ctypes
import ctypes.util
import os
from pathlib import Path
from queue import SimpleQueue
import sys
import tempfile
import threading
import time
import unittest
import uuid
import warnings

import alsaaudio as alsa


class PCMLifetimeTest(unittest.TestCase):
    def pcm(self, playback: bool = False, device: str = "null") -> alsa.PCM:
        return alsa.PCM(
            type=alsa.PCM_PLAYBACK if playback else alsa.PCM_CAPTURE,
            mode=alsa.PCM_NORMAL,
            device=device,
            rate=48000,
            channels=2,
            format=alsa.PCM_FORMAT_S16_LE,
            periodsize=64,
        )

    def assert_closed(self, pcm: alsa.PCM) -> None:
        with self.assertRaises(alsa.ALSAAudioError):
            pcm.state()
        pcm.close()

    def test_argument_callback_can_close_its_real_PCM(self) -> None:
        pcm = self.pcm()

        class Index:
            def __index__(self) -> int:
                pcm.close()
                return 1

        try:
            with warnings.catch_warnings(record=True):
                warnings.simplefilter("always", DeprecationWarning)
                self.assertEqual(pcm.setchannels(Index()), 1)
            self.assert_closed(pcm)
        finally:
            pcm.close()

    def test_argument_exception_preserved_after_reentrant_close(self) -> None:
        pcm = self.pcm()

        class MarkerError(Exception):
            pass

        class Index:
            def __index__(self) -> int:
                pcm.close()
                raise MarkerError("actual argument callback")

        with self.assertRaises(MarkerError):
            pcm.setchannels(Index())
        self.assert_closed(pcm)

    def test_warning_callback_can_close_its_real_PCM(self) -> None:
        pcm = self.pcm()
        callbacks = []

        def warning(*args: object, **kwargs: object) -> None:
            callbacks.append(True)
            pcm.close()

        try:
            with warnings.catch_warnings():
                warnings.simplefilter("always", DeprecationWarning)
                warnings.showwarning = warning
                self.assertEqual(pcm.setchannels(1), 1)
            self.assertTrue(callbacks)
            self.assert_closed(pcm)
        finally:
            pcm.close()

    def test_buffer_export_callback_can_close_its_real_PCM(self) -> None:
        pcm = self.pcm(playback=True)
        payload = bytes(64 * 4)

        class Buffer:
            def __buffer__(self, flags: int) -> memoryview:
                pcm.close()
                return memoryview(payload)

        try:
            self.assertEqual(pcm.write(Buffer()), 64)
            self.assert_closed(pcm)
        finally:
            pcm.close()

    def test_shared_handle_read_config_query_and_multiple_close(self) -> None:
        for cycle in range(16):
            pcm = self.pcm()
            start = threading.Barrier(7)
            live = [threading.Event() for _ in range(4)]
            errors: SimpleQueue[BaseException] = SimpleQueue()
            frames: SimpleQueue[int] = SimpleQueue()

            def reader(index: int) -> None:
                try:
                    start.wait(timeout=10)
                    for iteration in range(128):
                        try:
                            count, data = pcm.read()
                        except alsa.ALSAAudioError:
                            break
                        if count > 0:
                            self.assertIn(len(data), (count * 2, count * 4))
                            frames.put(count)
                            live[index].set()
                except BaseException as error:
                    errors.put(error)
                finally:
                    live[index].set()

            def configure() -> None:
                try:
                    start.wait(timeout=10)
                    for iteration in range(32):
                        try:
                            pcm.setchannels(1 + iteration % 2)
                            pcm.setperiodsize(32 + (iteration % 2) * 32)
                            pcm.info()
                        except alsa.ALSAAudioError:
                            break
                except BaseException as error:
                    errors.put(error)

            def closer() -> None:
                try:
                    start.wait(timeout=10)
                    for event in live:
                        self.assertTrue(event.wait(10))
                    for iteration in range(8):
                        pcm.close()
                except BaseException as error:
                    errors.put(error)

            threads = [threading.Thread(target=reader, args=(i,)) for i in range(4)]
            threads += [threading.Thread(target=configure), threading.Thread(target=closer)]
            for thread in threads:
                thread.start()
            start.wait(timeout=10)
            for thread in threads:
                thread.join(15)
                self.assertFalse(thread.is_alive(), "real PCM operation did not finish")
            pcm.close()
            if not errors.empty():
                raise errors.get()
            self.assertFalse(frames.empty(), "no actual native read completed")
            self.assert_closed(pcm)

    def install_file_PCM(self, name: str, fifo: Path, playback: bool = False) -> None:
        # Use ALSA's genuine public configuration API in this test process.
        # The original bundled defaults are loaded first and are retained.
        root = Path(alsa.__file__).parent / "pyalsaaudio_runtime"
        libraries = list(root.glob("libasound*.so*"))
        self.assertLessEqual(len(libraries), 1)
        # The original source backend links the platform's development SDK.
        # Exercise that real library as well as the bundled binary path.
        native = str(libraries[0]) if libraries else ctypes.util.find_library("asound")
        self.assertIsNotNone(native, "the real linked ALSA library was not found")
        library = ctypes.CDLL(native)
        library.snd_config_update.argtypes = []
        library.snd_config_update.restype = ctypes.c_int
        self.assertGreaterEqual(library.snd_config_update(), 0)
        library.snd_config_load_string.argtypes = [
            ctypes.POINTER(ctypes.c_void_p), ctypes.c_char_p, ctypes.c_size_t
        ]
        library.snd_config_load_string.restype = ctypes.c_int
        library.snd_config_merge.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]
        library.snd_config_merge.restype = ctypes.c_int
        files = f'file "{fifo}"' if playback else f'file "/dev/null" infile "{fifo}"'
        text = f'pcm.{name} {{ type file slave.pcm "null" {files} format "raw" }}'.encode()
        extra = ctypes.c_void_p()
        self.assertEqual(library.snd_config_load_string(ctypes.byref(extra), text, len(text)), 0)
        current = ctypes.c_void_p.in_dll(library, "snd_config")
        # Merge consumes extra. override=0 retains all existing PCM definitions.
        self.assertEqual(library.snd_config_merge(current, extra, 0), 0)

    def test_close_keeps_handle_alive_through_real_blocked_native_read(self) -> None:
        for cycle in range(8):
            with tempfile.TemporaryDirectory(prefix="pyalsa-native-fifo-") as directory:
                fifo = Path(directory) / "actual-audio-input"
                os.mkfifo(fifo)
                writer = os.open(fifo, os.O_RDWR | os.O_NONBLOCK)
                name = "codex_fifo_" + uuid.uuid4().hex
                self.install_file_PCM(name, fifo)
                pcm = self.pcm(device=name)
                results: SimpleQueue[object] = SimpleQueue()
                native_id: SimpleQueue[int] = SimpleQueue()

                def reader() -> None:
                    native_id.put(threading.get_native_id())
                    try:
                        results.put(pcm.read())
                    except BaseException as error:
                        results.put(error)

                thread = threading.Thread(target=reader)
                thread.start()
                tid = native_id.get(timeout=5)
                deadline = time.monotonic() + 5
                observed = ""
                try:
                    while time.monotonic() < deadline:
                        observed = Path(f"/proc/self/task/{tid}/wchan").read_text().strip()
                        if "pipe" in observed:
                            break
                        self.assertTrue(results.empty(), "native read unexpectedly finished")
                        time.sleep(0.001)
                    self.assertIn("pipe", observed, "native FIFO read was not observed")
                    started = time.monotonic()
                    pcm.close()
                    self.assertLess(time.monotonic() - started, 1)
                    self.assert_closed(pcm)
                    payload = b"\x15\x00\x16\x00" * 64
                    self.assertEqual(os.write(writer, payload), len(payload))
                    thread.join(5)
                    self.assertFalse(thread.is_alive())
                    result = results.get(timeout=2)
                    if isinstance(result, BaseException):
                        raise result
                    self.assertEqual(result, (64, payload))
                    remaining = []
                    for path in Path("/proc/self/fd").iterdir():
                        try:
                            if os.readlink(path) == str(fifo):
                                remaining.append(int(path.name))
                        except FileNotFoundError:
                            pass
                    self.assertEqual(remaining, [writer], "native FIFO handle leaked")
                finally:
                    if thread.is_alive():
                        os.write(writer, bytes(256))
                        thread.join(5)
                    pcm.close()
                    os.close(writer)

    def test_close_keeps_handle_alive_through_real_blocked_native_write(self) -> None:
        with tempfile.TemporaryDirectory(prefix="pyalsa-native-output-") as directory:
            fifo = Path(directory) / "actual-audio-output"
            os.mkfifo(fifo)
            descriptor = os.open(fifo, os.O_RDWR | os.O_NONBLOCK)
            name = "codex_fifo_" + uuid.uuid4().hex
            self.install_file_PCM(name, fifo, playback=True)
            pcm = self.pcm(playback=True, device=name)
            padding = bytearray()
            while True:
                try:
                    count = os.write(descriptor, b"x" * 4096)
                    padding.extend(b"x" * count)
                except BlockingIOError:
                    break
            payload = b"\x21\x00\x22\x00" * 65536
            results: SimpleQueue[object] = SimpleQueue()
            native_id: SimpleQueue[int] = SimpleQueue()

            def writer() -> None:
                native_id.put(threading.get_native_id())
                try:
                    results.put(pcm.write(payload))
                except BaseException as error:
                    results.put(error)

            thread = threading.Thread(target=writer)
            thread.start()
            tid = native_id.get(timeout=5)
            output = bytearray()
            try:
                deadline = time.monotonic() + 5
                observed = ""
                while time.monotonic() < deadline:
                    observed = Path(f"/proc/self/task/{tid}/wchan").read_text().strip()
                    if "pipe" in observed:
                        break
                    self.assertTrue(results.empty(), "native write unexpectedly finished")
                    time.sleep(0.001)
                self.assertIn("pipe", observed, "native FIFO write was not observed")
                started = time.monotonic()
                pcm.close()
                self.assertLess(time.monotonic() - started, 1)
                self.assert_closed(pcm)
                deadline = time.monotonic() + 5
                while thread.is_alive() and time.monotonic() < deadline:
                    try:
                        output.extend(os.read(descriptor, 65536))
                    except BlockingIOError:
                        time.sleep(0.001)
                thread.join(1)
                self.assertFalse(thread.is_alive())
                while True:
                    try:
                        output.extend(os.read(descriptor, 65536))
                    except BlockingIOError:
                        break
                result = results.get(timeout=2)
                if isinstance(result, BaseException):
                    raise result
                self.assertEqual(result, 65536)
                self.assertEqual(output, padding + payload)
            finally:
                deadline = time.monotonic() + 5
                while thread.is_alive() and time.monotonic() < deadline:
                    try:
                        os.read(descriptor, 65536)
                    except BlockingIOError:
                        time.sleep(0.001)
                pcm.close()
                thread.join(1)
                os.close(descriptor)


if __name__ == "__main__":
    assert sys.flags.gil is None and not sys._is_gil_enabled()
    assert not any(os.environ.get(n) for n in (
        "PYTHON_GIL", "ALSA_CONFIG_PATH", "ALSA_CONFIG_DIR", "ALSA_PLUGIN_DIR"
    ))
    unittest.main(verbosity=2)
