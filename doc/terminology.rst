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
   loudest signal that can be resproduced.

Frame
   A frame consists of exactly one sample per channel. If there is only one 
   channel (Mono sound) a frame is simply a single sample. If the sound is 
   stereo, each frame consists of two samples, etc.

Frame size
   This is the size in bytes of each frame. This can vary a lot: if each sample
    is 8 bits, and we're handling mono sound, the frame size is one byte. 
	Similarly in 6 channel audio with 64 bit floating point samples, the frame 
	size is 48 bytes

Rate
   PCM sound consists of a flow of sound frames. The sound rate controls how 
   often the current frame is replaced. For example, a rate of 8000 Hz
   means that a new frame is played or captured 8000 times per second.

Data rate
   This is the number of bytes, which must be recorded or provided per
   second at a certain frame size and rate.

   8000 Hz mono sound with 8 bit (1 byte) samples has a data rate of
   8000  \* 1 \* 1 = 8 kb/s or 64kbit/s. This is typically used for telephony.

   At the other end of the scale, 96000 Hz, 6 channel sound with 64
   bit (8 bytes) samples has a data rate of 96000 \* 6 \* 8 = 4608
   kb/s (almost 5 Mb sound data per second)

Period
   When the hardware processes data this is done in chunks of frames. The time
   interval between each processing (A/D or D/A conversion) is known
   as the period.
   The size of the period has direct implication on the latency of the
   sound input or output. For low-latency the period size should be
   very small, while low CPU resource usage would usually demand
   larger period sizes. With ALSA, the CPU utilization is not impacted
   much by the period size, since the kernel layer buffers multiple
   periods internally, so each period generates an interrupt and a
   memory copy, but userspace can be slower and read or write multiple
   periods at the same time.

Period size
   This is the size of each period in Hz. *Not bytes, but Hz!.* In 
   :mod:`alsaaudio` the period size is set directly, and it is
   therefore important to understand the significance of this
   number. If the period size is configured to for example 32,
   each write should contain exactly 32 frames of sound data, and each
   read will return either 32 frames of data or nothing at all.

Once you understand these concepts, you will be ready to use the PCM API. Read
on.


