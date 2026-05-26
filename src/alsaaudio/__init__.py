import sys

if sys.platform.startswith("linux"):
    from ._alsaaudio import *
else:
    raise ImportError(
        "pyalsaaudio runtime is available only on Linux with ALSA; "
        "this installation contains typing information only."
    )