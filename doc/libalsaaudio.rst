****************
:mod:`alsaaudio`
****************

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


.. function:: pcms([type=PCM_PLAYBACK])

   List available PCM devices by name.
   
   Arguments are:

   * *type* - can be either :const:`PCM_CAPTURE` or :const:`PCM_PLAYBACK`
     (default).  

   **Note:**

   For :const:`PCM_PLAYBACK`, the list of device names should be equivalent
   to the list of device names that ``aplay -L`` displays on the commandline::

      $ aplay -L

   For :const:`PCM_CAPTURE`, the list of device names should be equivalent
   to the list of device names that  ``arecord -L`` displays on the
   commandline::

      $ arecord -L

   *New in 0.8*

.. function:: cards()

   List the available ALSA cards by name. This function is only moderately
   useful. If you want to see a list of available PCM devices, use :func:`pcms`
   instead.
   

.. function:: mixers(cardindex=-1, device='default')

   List the available mixers. The arguments are:

   * *cardindex* - the card index. If this argument is given, the device name
     is constructed as: 'hw:*cardindex*' and
     the `device` keyword argument is ignored. ``0`` is the first hardware sound
     card.

   * *device* - the name of the device on which the mixer resides. The default
     is ``'default'``.

   **Note:** For a list of available controls, you can also use ``amixer`` on
   the commandline::
      
      $ amixer

   To elaborate the example, calling :func:`mixers` with the argument
   ``cardindex=0`` should give the same list of Mixer controls as::

      $ amixer -c 0

   And calling :func:`mixers` with the argument ``device='foo'`` should give
   the same list of Mixer controls as::

      $ amixer -D foo

   *Changed in 0.8*:
   
   - The keyword argument `device` is new and can be used to
     select virtual devices. As a result, the default behaviour has subtly
     changed. Since 0.8, this functions returns the mixers for the default
     device, not the mixers for the first card.


.. _pcm-objects:

PCM Objects
-----------

PCM objects in :mod:`alsaaudio` can play or capture (record) PCM
sound through speakers or a microphone. The PCM constructor takes the
following arguments:

.. class:: PCM(type=PCM_PLAYBACK, mode=PCM_NORMAL, device='default', cardindex=-1)

   This class is used to represent a PCM device (either for playback and
   recording). The arguments are:

   * *type* - can be either :const:`PCM_CAPTURE` or :const:`PCM_PLAYBACK`
     (default).  
   * *mode* - can be either :const:`PCM_NONBLOCK`, or :const:`PCM_NORMAL`
     (default). 
   * *device* - the name of the PCM device that should be used (for example
     a value from the output of :func:`pcms`). The default value is
     ``'default'``.
   * *cardindex* - the card index. If this argument is given, the device name
     is constructed as 'hw:*cardindex*' and
     the `device` keyword argument is ignored.
     ``0`` is the first hardware sound card.

   This will construct a PCM object with these default settings:

   * Sample format: :const:`PCM_FORMAT_S16_LE`
   * Rate: 44100 Hz 
   * Channels: 2
   * Period size: 32 frames

   *Changed in 0.8:*
   
   - The `card` keyword argument is still supported,
     but deprecated. Please use `device` instead.

   - The keyword argument `cardindex` was added.

   The `card` keyword is deprecated because it guesses the real ALSA
   name of the card. This was always fragile and broke some legitimate usecases.
   
     
PCM objects have the following methods:


.. method:: PCM.pcmtype()

   Returns the type of PCM object. Either :const:`PCM_CAPTURE` or
   :const:`PCM_PLAYBACK`.


.. method:: PCM.pcmmode()

   Return the mode of the PCM object. One of :const:`PCM_NONBLOCK`,
   :const:`PCM_ASYNC`, or :const:`PCM_NORMAL`


.. method:: PCM.cardname()

   Return the name of the sound card used by this PCM object.


.. method:: PCM.setchannels(nchannels)

   Used to set the number of capture or playback channels. Common
   values are: ``1`` = mono, ``2`` = stereo, and ``6`` = full 6 channel audio.
   Few sound cards support more than 2 channels


.. method:: PCM.setrate(rate)

   Set the sample rate in Hz for the device. Typical values are ``8000``
   (mainly used for telephony), ``16000``, ``44100`` (CD quality),
   ``48000`` and ``96000``.


.. method:: PCM.setformat(format)

   The sound *format* of the device. Sound format controls how the PCM device
   interpret data for playback, and how data is encoded in captures.

   The following formats are provided by ALSA:

   =========================  ===============
          Format                Description
   =========================  ===============
   ``PCM_FORMAT_S8``          Signed 8 bit samples for each channel
   ``PCM_FORMAT_U8``          Signed 8 bit samples for each channel
   ``PCM_FORMAT_S16_LE``      Signed 16 bit samples for each channel Little Endian byte order)
   ``PCM_FORMAT_S16_BE``      Signed 16 bit samples for each channel (Big Endian byte order)
   ``PCM_FORMAT_U16_LE``      Unsigned 16 bit samples for each channel (Little Endian byte order)
   ``PCM_FORMAT_U16_BE``      Unsigned 16 bit samples for each channel (Big Endian byte order)
   ``PCM_FORMAT_S24_LE``      Signed 24 bit samples for each channel (Little Endian byte order)
   ``PCM_FORMAT_S24_BE``      Signed 24 bit samples for each channel (Big Endian byte order)}
   ``PCM_FORMAT_U24_LE``      Unsigned 24 bit samples for each channel (Little Endian byte order)
   ``PCM_FORMAT_U24_BE``      Unsigned 24 bit samples for each channel (Big Endian byte order)
   ``PCM_FORMAT_S32_LE``      Signed 32 bit samples for each channel (Little Endian byte order)
   ``PCM_FORMAT_S32_BE``      Signed 32 bit samples for each channel (Big Endian byte order)
   ``PCM_FORMAT_U32_LE``      Unsigned 32 bit samples for each channel (Little Endian byte order)
   ``PCM_FORMAT_U32_BE``      Unsigned 32 bit samples for each channel (Big Endian byte order)
   ``PCM_FORMAT_FLOAT_LE``    32 bit samples encoded as float (Little Endian byte order)
   ``PCM_FORMAT_FLOAT_BE``    32 bit samples encoded as float (Big Endian byte order)
   ``PCM_FORMAT_FLOAT64_LE``  64 bit samples encoded as float (Little Endian byte order)
   ``PCM_FORMAT_FLOAT64_BE``  64 bit samples encoded as float (Big Endian byte order)
   ``PCM_FORMAT_MU_LAW``      A logarithmic encoding (used by Sun .au files and telephony)
   ``PCM_FORMAT_A_LAW``       Another logarithmic encoding
   ``PCM_FORMAT_IMA_ADPCM``   A 4:1 compressed format defined by the Interactive Multimedia Association.
   ``PCM_FORMAT_MPEG``        MPEG encoded audio?
   ``PCM_FORMAT_GSM``         9600 bits/s constant rate encoding for speech
   =========================  ===============
   

.. method:: PCM.setperiodsize(period)

   Sets the actual period size in frames. Each write should consist of
   exactly this number of frames, and each read will return this
   number of frames (unless the device is in :const:`PCM_NONBLOCK` mode, in
   which case it may return nothing at all)


.. method:: PCM.read()

   In :const:`PCM_NORMAL` mode, this function blocks until a full period is
   available, and then returns a tuple (length,data) where *length* is
   the number of frames of captured data, and *data* is the captured
   sound frames as a string. The length of the returned data will be 
   periodsize\*framesize bytes.

   In :const:`PCM_NONBLOCK` mode, the call will not block, but will return
   ``(0,'')`` if no new period has become available since the last
   call to read.


.. method:: PCM.write(data)

   Writes (plays) the sound in data. The length of data *must* be a
   multiple of the frame size, and *should* be exactly the size of a
   period. If less than 'period size' frames are provided, the actual
   playout will not happen until more data is written.

   If the device is not in :const:`PCM_NONBLOCK` mode, this call will block if
   the kernel buffer is full, and until enough sound has been played
   to allow the sound data to be buffered. The call always returns the
   size of the data provided.

   In :const:`PCM_NONBLOCK` mode, the call will return immediately, with a
   return value of zero, if the buffer is full. In this case, the data
   should be written at a later time.


.. method:: PCM.pause([enable=True])

   If *enable* is :const:`True`, playback or capture is paused.
   Otherwise, playback/capture is resumed.

**A few hints on using PCM devices for playback**

The most common reason for problems with playback of PCM audio is that writes 
to PCM devices must *exactly* match the data rate of the device.

If too little data is written to the device, it will underrun, and
ugly clicking sounds will occur. Conversely, of too much data is
written to the device, the write function will either block
(:const:`PCM_NORMAL` mode) or return zero (:const:`PCM_NONBLOCK` mode).

If your program does nothing but play sound, the best strategy is to put the
device in :const:`PCM_NORMAL` mode, and just write as much data to the device as
possible. This strategy can also be achieved by using a separate
thread with the sole task of playing out sound.

In GUI programs, however, it may be a better strategy to setup the device,
preload the buffer with a few periods by calling write a couple of times, and
then use some timer method to write one period size of data to the device every
period. The purpose of the preloading is to avoid underrun clicks if the used
timer doesn't expire exactly on time.

Also note, that most timer APIs that you can find for Python will
accummulate time delays: If you set the timer to expire after 1/10'th
of a second, the actual timeout will happen slightly later, which will
accumulate to quite a lot after a few seconds. Hint: use time.time()
to check how much time has really passed, and add extra writes as nessecary.


.. _mixer-objects:

Mixer Objects
-------------

Mixer objects provides access to the ALSA mixer API.


.. class:: Mixer(control='Master', id=0, cardindex=-1, device='default')

   Arguments are:
   
   * *control* - specifies which control to manipulate using this mixer
     object. The list of available controls can be found with the 
     :mod:`alsaaudio`.\ :func:`mixers` function.  The default value is
     ``'Master'`` - other common controls may be ``'Master Mono'``, ``'PCM'``,
     ``'Line'``, etc.

   * *id* - the id of the mixer control. Default is ``0``.

   * *cardindex* - specifies which card should be used. If this argument
     is given, the device name is constructed like this: 'hw:*cardindex*' and
     the `device` keyword argument is ignored. ``0`` is the
     first sound card. 

   * *device* - the name of the device on which the mixer resides. The default
     value is ``'default'``.
   
   *Changed in 0.8*:
   
   - The keyword argument `device` is new and can be used to select virtual
     devices.
  
Mixer objects have the following methods:

.. method:: Mixer.cardname()

   Return the name of the sound card used by this Mixer object


.. method:: Mixer.mixer()

   Return the name of the specific mixer controlled by this object, For example
   ``'Master'`` or ``'PCM'``


.. method:: Mixer.mixerid()

   Return the ID of the ALSA mixer controlled by this object.


.. method:: Mixer.switchcap()

   Returns a list of the switches which are defined by this specific mixer.
   Possible values in this list are:

   ======================  ================
   Switch                  Description
   ======================  ================
   'Mute'                  This mixer can mute
   'Joined Mute'           This mixer can mute all channels at the same time
   'Playback Mute'         This mixer can mute the playback output
   'Joined Playback Mute'  Mute playback for all channels at the same time}
   'Capture Mute'          Mute sound capture 
   'Joined Capture Mute'   Mute sound capture for all channels at a time}
   'Capture Exclusive'     Not quite sure what this is
   ======================  ================

   To manipulate these switches use the :meth:`setrec` or
   :meth:`setmute` methods


.. method:: Mixer.volumecap()

   Returns a list of the volume control capabilities of this
   mixer. Possible values in the list are:

   ========================  ================
   Capability                Description
   ========================  ================
   'Volume'                  This mixer can control volume
   'Joined Volume'           This mixer can control volume for all channels at the same time
   'Playback Volume'         This mixer can manipulate the playback output
   'Joined Playback Volume'  Manipulate playback volumne for all channels at the same time
   'Capture Volume'          Manipulate sound capture volume
   'Joined Capture Volume'   Manipulate sound capture volume for all channels at a time
   ========================  ================
   
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

   The optional *direction* argument can be either :const:`PCM_PLAYBACK` or
   :const:`PCM_CAPTURE`, which is relevant if the mixer can control both
   playback and capture volume.  The default value is :const:`PCM_PLAYBACK`
   if the mixer has playback channels, otherwise it is :const:`PCM_CAPTURE`.


.. method:: Mixer.getrec()

   Return a list indicating the current record mute setting for each channel. 0
   means not recording, 1 means recording.

   This method will fail if the mixer has no capture switch capabilities.


.. method:: Mixer.getvolume([direction])

   Returns a list with the current volume settings for each channel. The list
   elements are integer percentages.

   The optional *direction* argument can be either :const:`PCM_PLAYBACK` or
   :const:`PCM_CAPTURE`, which is relevant if the mixer can control both
   playback and capture volume. The default value is :const:`PCM_PLAYBACK`
   if the mixer has playback channels, otherwise it is :const:`PCM_CAPTURE`.


.. method:: Mixer.setvolume(volume, [channel], [direction])

   Change the current volume settings for this mixer. The *volume* argument
   controls the new volume setting as an integer percentage.

   If the optional argument *channel* is present, the volume is set
   only for this channel. This assumes that the mixer can control the
   volume for the channels independently.

   The optional *direction* argument can be either :const:`PCM_PLAYBACK` or
   :const:`PCM_CAPTURE`, which is relevant if the mixer can control both
   playback and capture volume. The default value is :const:`PCM_PLAYBACK`
   if the mixer has playback channels, otherwise it is :const:`PCM_CAPTURE`.

.. method:: Mixer.setmute(mute, [channel])

   Sets the mute flag to a new value. The *mute* argument is either 0 for not
   muted, or 1 for muted.

   The optional *channel* argument controls which channel is
   muted. The default is to set the mute flag for all channels.

   This method will fail if the mixer has no playback mute capabilities


.. method:: Mixer.setrec(capture, [channel])

   Sets the capture mute flag to a new value. The *capture* argument
   is either 0 for no capture, or 1 for capture.

   The optional *channel* argument controls which channel is
   changed. The default is to set the capture flag for all channels.

   This method will fail if the mixer has no capture switch capabilities.

.. method:: Mixer.polldescriptors()

   Returns a tuple of (file descriptor, eventmask) that can be used to
   wait for changes on the mixer with *select.poll*.

**A rant on the ALSA Mixer API**

The ALSA mixer API is extremely complicated - and hardly documented at all.
:mod:`alsaaudio` implements a much simplified way to access this API. In
designing the API I've had to make some choices which may limit what can and
cannot be controlled through the API. However, if I had chosen to implement the
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

Examples
--------

The following example are provided:

* `playwav.py`
* `recordtest.py`
* `playbacktest.py`
* `mixertest.py`

All examples (except `mixertest.py`) accept the commandline option 
*-c <cardname>*.

To determine a valid card name, use the commandline ALSA player::

   $ aplay -L

or::

   $ python

   >>> import alsaaudio
   >>> alsaaudio.pcms()

mixertest.py accepts the commandline options *-d <device>* and
*-c <cardindex>*. 

playwav.py
~~~~~~~~~~

**playwav.py** plays a wav file. 

To test PCM playback (on your default soundcard), run::

   $ python playwav.py <wav file>

recordtest.py and playbacktest.py
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
**recordtest.py** and **playbacktest.py** will record and play a raw
sound file in CD quality.

To test PCM recordings (on your default soundcard), run::

   $ python recordtest.py <filename>

Speak into the microphone, and interrupt the recording at any time
with ``Ctl-C``.

Play back the recording with::

   $ python playbacktest.py <filename>

mixertest.py
~~~~~~~~~~~~

Without arguments, **mixertest.py** will list all available *controls* on the
default soundcard.

The output might look like this::
   
  $ ./mixertest.py
  Available mixer controls:
     'Master'
     'Master Mono'
     'Headphone'
     'PCM'
     'Line'
     'Line In->Rear Out'
     'CD'
     'Mic'
     'PC Speaker'
     'Aux'
     'Mono Output Select'
     'Capture'
     'Mix'
     'Mix Mono'

With a single argument - the *control*, it will display the settings of 
that control; for example::

  $ ./mixertest.py Master
  Mixer name: 'Master'
  Capabilities: Playback Volume Playback Mute
  Channel 0 volume: 61%
  Channel 1 volume: 61%

With two arguments, the *control* and a *parameter*, it will set the 
parameter on the mixer::

  $ ./mixertest.py Master mute

This will mute the Master mixer.

Or::

  $ ./mixertest.py Master 40

This sets the volume to 40% on all channels.

To select a different soundcard, use either the *device* or *cardindex*
argument::

  $ ./mixertest.py -c 0 Master
  Mixer name: 'Master'
  Capabilities: Playback Volume Playback Mute
  Channel 0 volume: 61%
  Channel 1 volume: 61%

.. rubric:: Footnotes

.. [#f1]   ALSA also allows ``PCM_ASYNC``, but this is not supported yet.
