************
Introduction
************

:Author: Casper Wilstrup
:Author: Lars Immisch

.. |release| replace:: 0.4

.. % At minimum, give your name and an email address.  You can include a
.. % snail-mail address if you like.

.. % This makes the Abstract go on a separate page in the HTML version;
.. % if a copyright notice is used, it should go immediately after this.
.. % 


.. _front:

This software is licensed under the PSF license - the same one used by the
majority of the python distribution. Basically you can use it for anything you
wish (even commercial purposes). There is no warranty whatsoever.

.. % Copyright statement should go here, if needed.

.. % The abstract should be a paragraph or two long, and describe the
.. % scope of the document.


.. topic:: Abstract

   This package contains wrappers for accessing the ALSA API from Python. It is
   currently fairly complete for PCM devices and Mixer access. MIDI sequencer
   support is low on our priority list, but volunteers are welcome.

   If you find bugs in the wrappers please use the SourceForge bug tracker. 
   Please don't send bug reports regarding ALSA specifically. There are several
   bugs in this API, and those should be reported to the ALSA team - not me.


************
What is ALSA
************

The Advanced Linux Sound Architecture (ALSA) provides audio and MIDI
functionality to the Linux operating system.

Logically ALSA consists of these components:

* A set of kernel drivers. ---  These drivers are responsible for handling the
  physical sound  hardware from within the Linux kernel, and have been the
  standard sound implementation in Linux since kernel version 2.5

* A kernel level API for manipulating the ALSA devices.

* A user-space C library for simplified access to the sound hardware from
  userspace applications. This library is called *libasound* and is required by
  all ALSA capable applications.

More information about ALSA may be found on the project homepage
`<http://www.alsa-project.org>`_


ALSA and Python
===============

The older Linux sound API (OSS) which is now deprecated is well supported from
the standard Python library, through the ossaudiodev module. No native ALSA
support exists in the standard library.

There are a few other "ALSA for Python" projects available, including at least
two different projects called pyAlsa. Neither of these seem to be under active
development at the time - and neither are very feature complete.

I wrote PyAlsaAudio to fill this gap. My long term goal is to have the module
included in the standard Python library, but that looks currently unlikely.

PyAlsaAudio hass full support for sound capture, playback of sound, as well as
the ALSA Mixer API.

MIDI support is not available, and since I don't own any MIDI hardware, it's
difficult for me to implement it. Volunteers to work on this would be greatly
appreciated.


************
Installation
************

Note: the wrappers link with the alsasound library (from the alsa-lib package)
and need the ALSA headers for compilation.  Verify that you have
/usr/lib/libasound.so and /usr/include/alsa (or similar paths) before building.

*On Debian (and probably Ubuntu), install libasound2-dev.*

Naturally you also need to use a kernel with proper ALSA support. This is the
default in Linux kernel 2.6 and later. If you are using kernel version 2.4 you
may need to install the ALSA patches yourself - although most distributions 
ship with ALSA kernels.

To install, execute the following:  ---   ::

   $ python setup.py build

And then as root:  ---   ::

   # python setup.py install

*******
Testing
*******

First of all, run::
   
   $ python test.py

This is a small test suite that mostly performs consistency tests. If
it fails, please file a `bug report
<http://sourceforge.net/tracker/?group_id=120651>`_.

To test PCM recordings (on your default soundcard), verify your
microphone works, then do::

   $ python recordtest.py <filename>

Speak into the microphone, and interrupt the recording at any time
with ``Ctl-C``.

Play back the recording with::

   $ python playbacktest.py <filename>



