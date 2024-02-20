****************************
PCM Terminology and Concepts
****************************

In order to use PCM devices it is useful to be familiar with some concepts and
terminology.

Sample
   PCM audio, whether it is input or output, consists of *samples*.
   A single sample represents the amplitude of one channel of sound
   at a certain point in time. A lot of individual samples are
   necessary to represent actual sound; for CD audio, 44100 samples
   are taken every second.

   Samples can be of many different sizes, ranging from 8 bit to 64
   bit precision. The specific format of each sample can also vary -
   they can be big endian byte integers, little endian byte integers, or
   floating point numbers.

   Musically, the sample size determines the dynamic range. The
   dynamic range is the difference between the quietest and the
   loudest signal that can be reproduced.

Frame
   A frame consists of exactly one sample per channel. If there is only one
   channel (Mono sound) a frame is simply a single sample. If the sound is
   stereo, each frame consists of two samples, etc.

Frame size
   This is the size in bytes of each frame. This can vary a lot: if each sample
   is 8 bits, and we're handling mono sound, the frame size is one byte.
   For six channel audio with 64 bit floating point samples, the frame size
   is 48 bytes.

Rate
   PCM sound consists of a flow of sound frames. The sound rate controls how
   often the current frame is replaced. For example, a rate of 8000 Hz
   means that a new frame is played or captured 8000 times per second.

Data rate
   This is the number of bytes which must be consumed or provided per
   second at a certain frame size and rate.

   8000 Hz mono sound with 8 bit (1 byte) samples has a data rate of
   8000  \* 1 \* 1 = 8 kb/s or 64kbit/s. This is typically used for telephony.

   At the other end of the scale, 96000 Hz, 6 channel sound with 64
   bit (8 bytes) samples has a data rate of 96000 \* 6 \* 8 = 4608
   kb/s (almost 5 MB sound data per second).

   If the data rate requirement is not met, an overrun (on capture) or
   underrun (on playback) occurs; the term "xrun" is used to refer to
   either event.

.. _term-period:

Period
   The CPU processes sample data in chunks of frames, so-called periods
   (also called fragments by some systems). The operating system kernel's
   sample buffer must hold at least two periods (at any given time, one
   is processed by the sound hardware, and one by the CPU).

   The completion of a *period* triggers a CPU interrupt, which causes
   processing and context switching overhead. Therefore, a smaller period
   size causes higher CPU resource usage at a given data rate.

   A bigger size of the *buffer* improves the system's resilience to xruns.
   The buffer being split into a bigger number of smaller periods also does
   that, as it allows it to be drained / topped up sooner.

   On the other hand, a bigger size of the *buffer* also increases the
   playback latency, that is, the time it takes for a frame from being
   sent out by the application to being actually audible.

   Similarly, a bigger *period* size increases the capture latency.

   The trade-off between latency, xrun resilience, and resource usage
   must be made depending on the application.

Period size
   This is the size of each period in frames. *Not bytes, but frames!*
   In :mod:`alsaaudio` the period size is set directly, and it is
   therefore important to understand the significance of this
   number. If the period size is configured to for example 32,
   each write should contain exactly 32 frames of sound data, and each
   read will return either 32 frames of data or nothing at all.

.. _term-sample-size:

Sample size
   Each sample takes *physical_bits* of space. *nominal_bits* tells
   how many least significant bits are used. This is the bit depth
   in the format name and sometimes called just *sample bits* or
   *format width*. *significant_bits* tells how many most significant
   bits of the *nominal_bits* are used by the sample. This can be thought
   of as the *sample resolution*. This is visualized as follows::

    MSB |00000000 XXXXXXXX XXXXXXXX 00000000| LSB
                  |--significant--|
                  |---------nominal---------|
        |-------------physical--------------|

Once you understand these concepts, you will be ready to use the PCM API. Read
on.


