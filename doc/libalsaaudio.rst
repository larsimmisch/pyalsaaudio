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


********************
Module documentation
********************

:mod:`alsaaudio`
================

.. module:: alsaaudio
   :platform: Linux


.. % \declaremodule{builtin}{alsaaudio}	% standard library, in C
.. % not standard, in C

.. moduleauthor:: Casper Wilstrup <cwi@aves.dk>
.. moduleauthor:: Lars Immisch <lars@ibp.de>


.. % Author of the module code;



The :mod:`alsaaudio` module defines functions and classes for using ALSA.

.. % ---- 3.1. ----
.. % For each function, use a ``funcdesc'' block.  This has exactly two
.. % parameters (each parameters is contained in a set of curly braces):
.. % the first parameter is the function name (this automatically
.. % generates an index entry); the second parameter is the function's
.. % argument list.  If there are no arguments, use an empty pair of
.. % curly braces.  If there is more than one argument, separate the
.. % arguments with backslash-comma.  Optional parts of the parameter
.. % list are contained in \optional{...} (this generates a set of square
.. % brackets around its parameter).  Arguments are automatically set in
.. % italics in the parameter list.  Each argument should be mentioned at
.. % least once in the description; each usage (even inside \code{...})
.. % should be enclosed in \var{...}.


.. function:: mixers([cardname])

   List the available mixers. The optional *cardname* specifies which card 
   should be queried (this is only relevant if you have more than one sound 
   card). Omit to use the default sound card.


.. class:: PCM([type], [mode], [cardname])

   This class is used to represent a PCM device (both playback and capture
   devices). The arguments are:  ---  *type* - can be either PCM_CAPTURE or
   PCM_PLAYBACK (default).  ---  *mode* - can be either PCM_NONBLOCK,
   PCM_ASYNC, or PCM_NORMAL (the default). ---  *cardname* - specifies
   which card should be used (this is only relevant if you have more
   than one sound card). Omit to use the default sound card


.. class:: Mixer([control], [id], [cardname])

   This class is used to access a specific ALSA mixer. The arguments are:  ---
   *control* - Name of the chosen mixed (default is Master).  ---  *id* - id of
   mixer (default is 0) -- More explanation needed here ---  *cardname* 
   specifies which card should be used (this is only relevant if you have more 
   than one sound card). Omit to use the default sound card.


.. exception:: ALSAAudioError

   Exception raised when an operation fails for a ALSA specific reason. The
   exception argument is a string describing the reason of the failure.


.. _pcm-objects:

PCM Objects
-----------

The acronym PCM is short for Pulse Code Modulation and is the method used in
ALSA and many other places to handle playback and capture of sampled
sound data.

PCM objects in :mod:`alsaaudio` are used to do exactly that, either
play sample based sound or capture sound from some input source
(probably a microphone). The PCM object constructor takes the
following arguments:


.. class:: PCM([type], [mode], [cardname])

   *type* - can be either PCM_CAPTURE or PCM_PLAYBACK (default).

   *mode* - can be either PCM_NONBLOCK, PCM_ASYNC, or PCM_NORMAL (the
   default). In PCM_NONBLOCK mode, calls to read will return
   immediately independent of wether there is any actual data to
   read. Similarly, write calls will return immediately without
   actually writing anything to the playout buffer if the buffer is full.

   In the current version of :mod:`alsaaudio` PCM_ASYNC is useless,
   since it relies on a callback procedure, which can't be specified
   through this API yet.

   *cardname* - specifies which card should be used (this is only
   relevant if you have more than one sound card). Omit to use the
   default sound card.

   This will construct a PCM object with default settings:

   Sample format: PCM_FORMAT_S16_LE  ---  Rate: 8000 Hz  ---  Channels: 2  ---
   Period size: 32 frames  ---

PCM objects have the following methods:


.. method:: PCM.pcmtype()

   Returns the type of PCM object. Either PCM_CAPTURE or PCM_PLAYBACK.


.. method:: PCM.pcmmode()

   Return the mode of the PCM object. One of PCM_NONBLOCK, PCM_ASYNC,
   or PCM_NORMAL


.. method:: PCM.cardname()

   Return the name of the sound card used by this PCM object.


.. method:: PCM.setchannels(nchannels)

   Used to set the number of capture or playback channels. Common
   values are: 1 = mono, 2 = stereo, and 6 = full 6 channel audio. Few
   sound cards support more than 2 channels


.. method:: PCM.setrate(rate)

   Set the sample rate in Hz for the device. Typical values are 8000
   (mainly used for telephony), 16000, 44100 (CD quality), and 96000.


.. method:: PCM.setformat(format)

   The sound *format* of the device. Sound format controls how the PCM device
   interpret data for playback, and how data is encoded in captures.

   The following formats are provided by ALSA:

   =====================  ===============
          Format            Description
   =====================  ===============
   PCM_FORMAT_S8		  Signed 8 bit samples for each channel
   PCM_FORMAT_U8		  Signed 8 bit samples for each channel
   PCM_FORMAT_S16_LE	  Signed 16 bit samples for each channel Little Endian byte order)
   PCM_FORMAT_S16_BE	  Signed 16 bit samples for each channel (Big Endian byte order)
   PCM_FORMAT_U16_LE	  Unsigned 16 bit samples for each channel (Little Endian byte order)
   PCM_FORMAT_U16_BE	  Unsigned 16 bit samples for each channel (Big Endian byte order)
   PCM_FORMAT_S24_LE	  Signed 24 bit samples for each channel (Little Endian byte order)
   PCM_FORMAT_S24_BE	  Signed 24 bit samples for each channel (Big Endian byte order)}
   PCM_FORMAT_U24_LE	  Unsigned 24 bit samples for each channel (Little Endian byte order)
   PCM_FORMAT_U24_BE	  Unsigned 24 bit samples for each channel (Big Endian byte order)
   PCM_FORMAT_S32_LE	  Signed 32 bit samples for each channel (Little Endian byte order)
   PCM_FORMAT_S32_BE	  Signed 32 bit samples for each channel (Big Endian byte order)
   PCM_FORMAT_U32_LE	  Unsigned 32 bit samples for each channel (Little Endian byte order)
   PCM_FORMAT_U32_BE	  Unsigned 32 bit samples for each channel (Big Endian byte order)
   PCM_FORMAT_FLOAT_LE	  32 bit samples encoded as float (Little Endian byte order)
   PCM_FORMAT_FLOAT_BE	  32 bit samples encoded as float (Big Endian byte order)
   PCM_FORMAT_FLOAT64_LE  64 bit samples encoded as float (Little Endian byte order)
   PCM_FORMAT_FLOAT64_BE  64 bit samples encoded as float (Big Endian byte order)
   PCM_FORMAT_MU_LAW	  A logarithmic encoding (used by Sun .au files and telephony)
   PCM_FORMAT_A_LAW		  Another logarithmic encoding
   PCM_FORMAT_IMA_ADPCM	  A 4:1 compressed format defined by the Interactive Multimedia Association.
   PCM_FORMAT_MPEG		  MPEG encoded audio?
   PCM_FORMAT_GSM		  9600 bits/s constant rate encoding for speech
   =====================  ===============
   

.. method:: PCM.setperiodsize(period)

   Sets the actual period size in frames. Each write should consist of
   exactly this number of frames, and each read will return this
   number of frames (unless the device is in PCM_NONBLOCK mode, in
   which case it may return nothing at all)


.. method:: PCM.read()

   In PCM_NORMAL mode, this function blocks until a full period is
   available, and then returns a tuple (length,data) where *length* is
   the number of frames of captured data, and *data* is the captured
   sound frames as a string. The length of the returned data will be 
   periodsize\*framesize bytes.

   In PCM_NONBLOCK mode, the call will not block, but will return
   ``(0,'')`` if no new period has become available since the last
   call to read.


.. method:: PCM.write(data)

   Writes (plays) the sound in data. The length of data *must* be a
   multiple of the frame size, and *should* be exactly the size of a
   period. If less than 'period size' frames are provided, the actual
   playout will not happen until more data is written.

   If the device is not in PCM_NONBLOCK mode, this call will block if
   the kernel buffer is full, and until enough sound has been played
   to allow the sound data to be buffered. The call always returns the
   size of the data provided.

   In PCM_NONBLOCK mode, the call will return immediately, with a
   return value of zero, if the buffer is full. In this case, the data
   should be written at a later time.


.. method:: PCM.pause([enable=1])

   If *enable* is 1, playback or capture is paused. If *enable* is 0,
   playback/capture is resumed.

**A few hints on using PCM devices for playback**

The most common reason for problems with playback of PCM audio, is that the
people don't properly understand that writes to PCM devices must match
*exactly* the data rate of the device.

If too little data is written to the device, it will underrun, and
ugly clicking sounds will occur. Conversely, of too much data is
written to the device, the write function will either block
(PCM_NORMAL mode) or return zero (PCM_NONBLOCK mode).

If your program does nothing, but play sound, the easiest way is to put the
device in PCM_NORMAL mode, and just write as much data to the device as
possible. This strategy can also be achieved by using a separate
thread with the sole task of playing out sound.

In GUI programs, however, it may be a better strategy to setup the device,
preload the buffer with a few periods by calling write a couple of times, and
then use some timer method to write one period size of data to the device every
period. The purpose of the preloading is to avoid underrun clicks if the used
timer doesn't expire exactly on time.

Also note, that most timer APIs that you can find for Python will
acummulate time delays: If you set the timer to expire after 1/10'th
of a second, the actual timeout will happen slightly later, which will
accumulate to quite a lot after a few seconds. Hint: use time.time()
to check how much time has really passed, and add extra writes as nessecary.


.. _mixer-objects:

Mixer Objects
-------------

Mixer objects provides access to the ALSA mixer API.


.. class:: Mixer([control], [id],  [cardname])

   *control* - specifies which control to manipulate using this mixer
   object. The list of available controls can be found with the 
   :mod:`alsaaudio`.\ :func:`mixers` function.  The default value is
   'Master' - other common controls include 'Master Mono', 'PCM', 'Line', etc.

   *id* - the id of the mixer control. Default is 0

   *cardname* - specifies which card should be used (this is only
   relevant if you have more than one sound card). Omit to use the
   default sound card.

Mixer objects have the following methods:

.. method:: Mixer.cardname()

   Return the name of the sound card used by this Mixer object


.. method:: Mixer.mixer()

   Return the name of the specific mixer controlled by this object, For example
   'Master' or 'PCM'


.. method:: Mixer.mixerid()

   Return the ID of the ALSA mixer controlled by this object.


.. method:: Mixer.switchcap()

   Returns a list of the switches which are defined by this specific mixer.
   Possible values in this list are:

   ====================  ================
   Switch                Description
   ====================  ================
   Mute                  This mixer can mute
   Joined Mute           This mixer can mute all channels at the same time
   Playback Mute         This mixer can mute the playback output
   Joined Playback Mute  Mute playback for all channels at the same time}
   Capture Mute          Mute sound capture 
   Joined Capture Mute   Mute sound capture for all channels at a time}
   Capture Exclusive     Not quite sure what this is
   ====================  ================
   
   To manipulate these swithes use the :meth:`setrec` or
   :meth:`setmute` methods


.. method:: Mixer.volumecap()

   Returns a list of the volume control capabilities of this
   mixer. Possible values in the list are:

   ======================  ================
   Capability              Description
   ======================  ================
   Volume                  This mixer can control volume
   Joined Volume           This mixer can control volume for all channels at the same time
   Playback Volume         This mixer can manipulate the playback output
   Joined Playback Volume  Manipulate playback volumne for all channels at the same time
   Capture Volume          Manipulate sound capture volume
   Joined Capture Volume   Manipulate sound capture volume for all channels at a time
   ======================  ================
   
.. method:: Mixer.getenum()

   For enumerated controls, return the currently selected item and  the list of
   items available.

   Returns a tuple *(string, list of strings)*.

   For example, my soundcard has a Mixer called *Mono Output Select*. Using
   *amixer*, I get::

      $ amixer get "Mono Output Select"
      Simple mixer control 'Mono Output Select',0
        Capabilities: enum
        Items: 'Mix' 'Mic'
        Item0: 'Mix'

   Using :mod:`alsaaudio`, one could do::

      >>> import alsaaudio
      >>> m = alsaaudio.Mixer('Mono Output Select')
      >>> m.getenum()
      ('Mix', ['Mix', 'Mic'])

   This method will return an empty tuple if the mixer is not an  enumerated
   control.


.. method:: Mixer.getmute()

   Return a list indicating the current mute setting for each
   channel. 0 means not muted, 1 means muted.

   This method will fail if the mixer has no playback switch capabilities.


.. method:: Mixer.getrange([direction])

   Return the volume range of the ALSA mixer controlled by this object.

   The optional *direction* argument can be either 'playback' or
   'capture', which is relevant if the mixer can control both playback
   and capture volume.  The default value is 'playback' if the mixer
   has this capability, otherwise 'capture'


.. method:: Mixer.getrec()

   Return a list indicating the current record mute setting for each channel. 0
   means not recording, 1 means recording.

   This method will fail if the mixer has no capture switch capabilities.


.. method:: Mixer.getvolume([direction])

   Returns a list with the current volume settings for each channel. The list
   elements are integer percentages.

   The optional *direction* argument can be either 'playback' or
   'capture', which is relevant if the mixer can control both playback
   and capture volume. The default value is 'playback' if the mixer
   has this capability, otherwise 'capture'


.. method:: Mixer.setvolume(volume,[channel], [direction])

   Change the current volume settings for this mixer. The *volume* argument
   controls the new volume setting as an integer percentage.

   If the optional argument *channel* is present, the volume is set
   only for this channel. This assumes that the mixer can control the
   volume for the channels independently.

   The optional *direction* argument can be either 'playback' or 'capture' is
   relevant if the mixer has independent playback and capture volume
   capabilities, and controls which of the volumes if changed. The
   default is 'playback' if the mixer has this capability, otherwise 'capture'.


.. method:: Mixer.setmute(mute, [channel])

   Sets the mute flag to a new value. The *mute* argument is either 0 for not
   muted, or 1 for muted.

   The optional *channel* argument controls which channel is
   muted. The default is to set the mute flag for all channels.

   This method will fail if the mixer has no playback mute capabilities


.. method:: Mixer.setrec(capture,[channel])

   Sets the capture mute flag to a new value. The *capture* argument
   is either 0 for no capture, or 1 for capture.

   The optional *channel* argument controls which channel is
   changed. The default is to set the capture flag for all channels.

   This method will fail if the mixer has no capture switch capabilities.

**A Note on the ALSA Mixer API**

The ALSA mixer API is extremely complicated - and hardly documented at all.
:mod:`alsaaudio` implements a much simplified way to access this API. In
designing the API I've had to make some choices which may limit what can and
cannot be controlled through the API. However, If I had chosen to implement the
full API, I would have reexposed the horrible complexity/documentation ratio of
the underlying API.  At least the :mod:`alsaaudio` API is easy to
understand and use.

If my design choises prevents you from doing something that the underlying API
would have allowed, please let me know, so I can incorporate these needs into
future versions.

If the current state of affairs annoys you, the best you can do is to write a
HOWTO on the API and make this available on the net. Until somebody does this,
the availability of ALSA mixer capable devices will stay quite limited.

Unfortunately, I'm not able to create such a HOWTO myself, since I only
understand half of the API, and that which I do understand has come from a
painful trial and error process.

.. % ==== 4. ====


.. _pcm-example:

ALSA Examples
-------------

For now, the only examples available are the 'playbacktest.py' and the
'recordtest.py' programs included.  This will change in a future version.

