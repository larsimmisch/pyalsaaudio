PyAlsaAudio
===========

Author: Casper Wilstrup (cwi@aves.dk)

This package contains wrappers for accessing the ALSA api from Python. It
is currently fairly complete for PCM devices. My next goal is to have
complete mixer supports as well. MIDI sequencer support is low on my
priority list, but volunteers are welcome.

If you find bugs in the wrappers please notify me on email. Please
don't send bug reports regarding ALSA specifically. There are several
bugs in this api, and those should be reported to the ALSA team - not
me.

This software is licensed under the PSF license - the same one used
by the majority of the python distribution. Basically you can use it
for anything you wish (even commercial purposes). There is no warranty
whatsoever.


Installation
============

Note: the wrappers link with the alsasound library alsa (from the alsa-lib
package). Verify that this is installed by looking for /usr/lib/libasound.so
before building. The libasound development files are also neccesary. On debian
and derivatives, this is achieved by installing the alsalib-dev package.

Naturally you also need to use a kernel with proper ALSA
support. This is the default in Linux kernel 2.6 and later. If you are using
kernel version 2.4 you may need to install the ALSA patches yourself - although
most distributions ship with ALSA kernels.

To install, execute the following:
  $ python setup.py build

And then as root:
  # python setup.py install


Using the API
=============
There is a reasonably useful API documentation included in the module
documentation, which can be found in the doc subdirectory of the source
distribution.

There are also three example programs included with the source:
'playbacktest.py' which plays back raw sound data read from
stdin

'recordtest.py' which captures sound from the microphone at writes
it raw to stdout.

'mixertest.py' which can be used to manipulate the mixers.
