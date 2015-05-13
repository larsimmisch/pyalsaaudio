# PyAlsaAudio

For documentation, see http://larsimmisch.github.io/pyalsaaudio/

> Author: Casper Wilstrup (cwi@aves.dk)  
> Maintainer: Lars Immisch (lars@ibp.de)

This package contains wrappers for accessing the ALSA api from Python. It
is currently fairly complete for PCM devices, and has some support for mixers.

If you find bugs in the wrappers please open an issue in the issue tracker.
Please don't send bug reports regarding ALSA specifically. There are several
bugs in this api, and those should be reported to the ALSA team - not
me.

This software is licensed under the PSF license - the same one used
by the majority of the python distribution. Basically you can use it
for anything you wish (even commercial purposes). There is no warranty
whatsoever.


# Installation

## PyPI

To install pyalsaaudio via `pip` (or `easy_install`):

```
  $ pip install pyalsaaudio
```

## Manual installation

*Note:* the wrappers need the ALSA library and headers.
The installation of these varies from distribution to distribution.

On Debian or Ubuntu, make sure to install `linasound2-dev`. On Arch,
install `alsa-lib`. When in doubt, search your distribution for a
package that contains `libasound.so` and `asoundlib.h`.

Naturally you also need to use a kernel with ALSA support.

First, get the sources and change to the source directory:
```
  $ git clone https://github.com/larsimmisch/pyalsaaudio.git
  $ cd pyalsaaudio
```
  
To install, execute the following:
```
  $ python setup.py build
```
And then as root:
```
  $ sudo python setup.py install
```

# Using the API
The API documentation is included in the doc subdirectory of the source
distribution; it is also online on [http://larsimmisch.github.io/pyalsaaudio/](http://larsimmisch.github.io/pyalsaaudio/).

There are some example programs included with the source:

* [playwav.py](https://github.com/larsimmisch/pyalsaaudio/blob/master/playwav.py) plays back a wav file
* [playbacktest.py](https://github.com/larsimmisch/pyalsaaudio/blob/master/playbacktest.py) plays back raw sound data read from stdin
* [recordtest.py](https://github.com/larsimmisch/pyalsaaudio/blob/master/recordtest.py) captures sound from the microphone and writes
it raw to stdout.
* [mixertest.py](https://github.com/larsimmisch/pyalsaaudio/blob/master/mixertest.py) can be used to manipulate the mixers.
