****************
:mod:`alsaaudio`
****************

.. module:: alsaaudio
   :platform: Linux

.. moduleauthor:: Casper Wilstrup <cwi@aves.dk>
.. moduleauthor:: Lars Immisch <lars@ibp.de>

The :mod:`alsaaudio` module defines functions and classes for using ALSA.

.. function:: pcms(pcmtype: int = PCM_PLAYBACK) ->list[str]

   List available PCM devices by name.

   Arguments are:

   * *pcmtype* - can be either :const:`PCM_CAPTURE` or :const:`PCM_PLAYBACK`
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

.. function:: cards() -> list[str]

   List the available ALSA cards by name. This function is only moderately
   useful. If you want to see a list of available PCM devices, use :func:`pcms`
   instead.

..
   Omitted by intention due to being superseded by cards():

   .. function:: card_indexes()
   .. function:: card_name()

.. function:: mixers(cardindex: int = -1, device: str = 'default') -> list[str]

   List the available mixers. The arguments are:

   * *cardindex* - the card index. If this argument is given, the device name
     is constructed as: 'hw:*cardindex*' and
     the `device` keyword argument is ignored. ``0`` is the first hardware sound
     card.

     **Note:** This should not be used, as it bypasses most of ALSA's configuration.

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

.. function:: asoundlib_version() -> str

   Return a Python string containing the ALSA version found.


.. _pcm-objects:

PCM Objects
-----------

PCM objects in :mod:`alsaaudio` can play or capture (record) PCM
sound through speakers or a microphone. The PCM constructor takes the
following arguments:

.. class:: PCM(type: int = PCM_PLAYBACK, mode: int = PCM_NORMAL, rate: int = 44100, channels: int = 2,
               format: int = PCM_FORMAT_S16_LE, periodsize: int = 32, periods: int = 4,
               device: str = 'default', cardindex: int = -1) -> PCM

   This class is used to represent a PCM device (either for playback or
   recording). The constructor's arguments are:

   * *type* - can be either :const:`PCM_CAPTURE` or :const:`PCM_PLAYBACK`
     (default).
   * *mode* - can be either :const:`PCM_NONBLOCK`, or :const:`PCM_NORMAL`
     (default).
   * *rate* - the sampling rate in Hz. Typical values are ``8000`` (mainly used for telephony), ``16000``, ``44100`` (default), ``48000`` and ``96000``.
   * *channels* - the number of channels. The default value is 2 (stereo).
   * *format* - the data format. This controls how the PCM device interprets data for playback, and how data is encoded in captures.
     The default value is :const:`PCM_FORMAT_S16_LE`.

   =========================  ===============
        Format                Description
   =========================  ===============
   ``PCM_FORMAT_S8``          Signed 8 bit samples for each channel
   ``PCM_FORMAT_U8``          Unsigned 8 bit samples for each channel
   ``PCM_FORMAT_S16_LE``      Signed 16 bit samples for each channel Little Endian byte order)
   ``PCM_FORMAT_S16_BE``      Signed 16 bit samples for each channel (Big Endian byte order)
   ``PCM_FORMAT_U16_LE``      Unsigned 16 bit samples for each channel (Little Endian byte order)
   ``PCM_FORMAT_U16_BE``      Unsigned 16 bit samples for each channel (Big Endian byte order)
   ``PCM_FORMAT_S24_LE``      Signed 24 bit samples for each channel (Little Endian byte order in 4 bytes)
   ``PCM_FORMAT_S24_BE``      Signed 24 bit samples for each channel (Big Endian byte order in 4 bytes)
   ``PCM_FORMAT_U24_LE``      Unsigned 24 bit samples for each channel (Little Endian byte order in 4 bytes)
   ``PCM_FORMAT_U24_BE``      Unsigned 24 bit samples for each channel (Big Endian byte order in 4 bytes)
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
   ``PCM_FORMAT_S24_3LE``     Signed 24 bit samples for each channel (Little Endian byte order in 3 bytes)
   ``PCM_FORMAT_S24_3BE``     Signed 24 bit samples for each channel (Big Endian byte order in 3 bytes)
   ``PCM_FORMAT_U24_3LE``     Unsigned 24 bit samples for each channel (Little Endian byte order in 3 bytes)
   ``PCM_FORMAT_U24_3BE``     Unsigned 24 bit samples for each channel (Big Endian byte order in 3 bytes)
   =========================  ===============

   * *periodsize* - the period size in frames.
     Make sure you understand :ref:`the meaning of periods <term-period>`.
     The default value is 32, which is below the actual minimum of most devices,
     and will therefore likely be larger in practice.
   * *periods* - the number of periods in the buffer. The default value is 4.
   * *device* - the name of the PCM device that should be used (for example
     a value from the output of :func:`pcms`). The default value is
     ``'default'``.
   * *cardindex* - the card index. If this argument is given, the device name
     is constructed as 'hw:*cardindex*' and
     the `device` keyword argument is ignored.
     ``0`` is the first hardware sound card.

     **Note:** This should not be used, as it bypasses most of ALSA's configuration.

   The defaults mentioned above are values passed by :mod:alsaaudio
   to ALSA, not anything internal to ALSA.

   **Note:** For default and non-default values alike, there is no
   guarantee that a PCM device supports the requested configuration,
   and ALSA may pick realizable values which it believes to be closest
   to the request. Therefore, after creating a PCM object, it is
   necessary to verify whether its realized configuration is acceptable.
   The :func:info method can be used to query it.

   *Changed in 0.10:*

   - Added the optional named parameter `periods`.

   *Changed in 0.9:*

   - Added the optional named parameters `rate`, `channels`, `format` and `periodsize`.

   *Changed in 0.8:*

   - The `card` keyword argument is still supported,
     but deprecated. Please use `device` instead.

   - The keyword argument `cardindex` was added.

   The `card` keyword is deprecated because it guesses the real ALSA
   name of the card. This was always fragile and broke some legitimate usecases.

PCM objects have the following methods:

.. method:: PCM.info() -> dict

   Returns a dictionary containing the configuration of a PCM device.

   A small subset of properties reflects fixed parameters given by the
   user, stored within alsaaudio. To distinguish them from properties
   retrieved from ALSA when the call is made, they have their name
   prefixed with **" (call value) "**.

   Descriptions of properties which can be directly set during PCM object
   instantiation carry the prefix "PCM():", followed by the respective
   constructor parameter. Note that due to device limitations, the values
   may deviate from those originally requested.

   Yet another set of properties cannot be set, and derives directly from
   the hardware, possibly depending on other properties. Those properties'
   descriptions are prefixed with "hw:" below.

   ===========================  ====================================  ==================================================================
        Key                      Description (Reference)               Type
   ===========================  ====================================  ==================================================================
   name                         PCM():device                          string
   card_no                      *index of card*                       integer  (negative indicates device not associable with a card)
   device_no                    *index of PCM device*                 integer
   subdevice_no                 *index of PCM subdevice*              integer
   state                        *name of PCM state*                   string
   access_type                  *name of PCM access type*             string
   (call value) type            PCM():type                            integer
   (call value) type_name       PCM():type                            string
   (call value) mode            PCM():mode                            integer
   (call value) mode_name       PCM():mode                            string
   format                       PCM():format                          integer
   format_name                  PCM():format                          string
   format_description           PCM():format                          string
   subformat_name               *name of PCM subformat*               string
   subformat_description        *description of subformat*            string
   channels                     PCM():channels                        integer
   rate                         PCM():rate                            integer (Hz)
   period_time                  *period duration*                     integer (:math:`\mu s`)
   period_size                  PCM():period_size                     integer (frames)
   buffer_time                  *buffer time*                         integer (:math:`\mu s`) (negative indicates error)
   buffer_size                  *buffer size*                         integer (frames) (negative indicates error)
   get_periods                  *approx. periods in buffer*           integer (negative indicates error)
   rate_numden                  *numerator, denominator*              tuple (integer (Hz), integer (Hz))
   significant_bits             *significant bits in sample* [#tss]_  integer (negative indicates error)
   nominal_bits                 *nominal bits in sample* [#tss]_      integer (negative indicates error)
   physical_bits                *sample width in bits* [#tss]_        integer (negative indicates error)
   is_batch                     *hw: double buffering*                boolean (True: hardware supported)
   is_block_transfer            *hw: block transfer*                  boolean (True: hardware supported)
   is_double                    *hw: double buffering*                boolean (True: hardware supported)
   is_half_duplex               *hw: half-duplex*                     boolean (True: hardware supported)
   is_joint_duplex              *hw: joint-duplex*                    boolean (True: hardware supported)
   can_overrange                *hw: overrange detection*             boolean (True: hardware supported)
   can_mmap_sample_resolution   *hw: sample-resol. mmap*              boolean (True: hardware supported)
   can_pause                    *hw: pause*                           boolean (True: hardware supported)
   can_resume                   *hw: resume*                          boolean (True: hardware supported)
   can_sync_start               *hw: synchronized start*              boolean (True: hardware supported)
   ===========================  ====================================  ==================================================================
.. [#tss] More information in the :ref:`terminology section for sample size <term-sample-size>`

..

   The italicized descriptions give a summary of the "full" description
   as can be found in the
   `ALSA documentation <https://www.alsa-project.org/alsa-doc>`_.

   *New in 0.9.1*

.. method:: PCM.dumpinfo()

   Dumps the PCM object's configured parameters to stdout.

.. method:: PCM.pcmtype() -> int

   Returns the type of PCM object. Either :const:`PCM_CAPTURE` or
   :const:`PCM_PLAYBACK`.

.. method:: PCM.pcmmode() -> int

   Return the mode of the PCM object. One of :const:`PCM_NONBLOCK`,
   :const:`PCM_ASYNC`, or :const:`PCM_NORMAL`

.. method:: PCM.cardname() -> string

   Return the name of the sound card used by this PCM object.

..
   Omitted by intention due to not really fitting the c'tor-based setup concept:

   .. method:: PCM.getchannels()

   Returns list of the device's supported channel counts.

   .. method:: PCM.getratebounds()

   Returns the card's minimum and maximum supported sample rates as
   a tuple of integers.

   .. method:: PCM.getrates()

   Returns the sample rates supported by the device.
   The returned value can be of one of the following, depending on
   the card's properties:
   * Card supports only a single rate: returns the rate
   * Card supports a continuous range of rates: returns a tuple of
     the range's lower and upper bounds (inclusive)
   * Card supports a collection of well-known rates: returns a list of
     the supported rates

   .. method:: PCM.getformats()

   Returns a dictionary of supported format codes (integers) keyed by
   their standard ALSA names (strings).

.. method:: PCM.setchannels(nchannels: int) -> int

   .. deprecated:: 0.9 Use the `channels` named argument to :func:`PCM`.

.. method:: PCM.setrate(rate: int) -> int

   .. deprecated:: 0.9 Use the `rate` named argument to :func:`PCM`.

.. method:: PCM.setformat(format: int) -> int

   .. deprecated:: 0.9 Use the `format` named argument to :func:`PCM`.

.. method:: PCM.setperiodsize(period: int) -> int

   .. deprecated:: 0.9 Use the `periodsize` named argument to :func:`PCM`.

.. method:: PCM.state() -> int

   Returs the current state of the stream, which can be one of
   :const:`PCM_STATE_OPEN` (this should not actually happen),
   :const:`PCM_STATE_SETUP` (after :func:`drop` or :func:`drain`),
   :const:`PCM_STATE_PREPARED` (after construction),
   :const:`PCM_STATE_RUNNING`,
   :const:`PCM_STATE_XRUN`,
   :const:`PCM_STATE_DRAINING`,
   :const:`PCM_STATE_PAUSED`,
   :const:`PCM_STATE_SUSPENDED`, and
   :const:`PCM_STATE_DISCONNECTED`.

   *New in 0.10*

.. method:: PCM.avail() -> int

   For :const:`PCM_PLAYBACK` PCM objects, returns the number of writable
   (that is, free) frames in the buffer.

   For :const:`PCM_CAPTURE` PCM objects, returns the number of readable
   (that is, filled) frames in the buffer.

   An attempt to read/write more frames than indicated will block (in
   :const:`PCM_NORMAL` mode) or fail and return zero (in
   :const:`PCM_NONBLOCK` mode).

   *New in 0.11*

.. method:: PCM.read() -> tuple[int, bytes]

   In :const:`PCM_NORMAL` mode, this function blocks until a full period is
   available, and then returns a tuple (length,data) where *length* is
   the number of frames of captured data, and *data* is the captured
   sound frames as a string. The length of the returned data will be
   periodsize\*framesize bytes.

   In :const:`PCM_NONBLOCK` mode, the call will not block, but will return
   ``(0,'')`` if no new period has become available since the last
   call to read.

   In case of a buffer overrun, this function will return the negative
   size :const:`-EPIPE`, and no data is read.
   This indicates that data was lost. To resume capturing, just call read
   again, but note that the stream was already corrupted.
   To avoid the problem in the future, try using a larger period size
   and/or more periods, at the cost of higher latency.

.. method:: PCM.write(data: bytes) -> int

   Writes (plays) the sound in data. The length of data *must* be a
   multiple of the frame size, and *should* be exactly the size of a
   period. If less than 'period size' frames are provided, the actual
   playout will not happen until more data is written.

   If the data was successfully written, the call returns the size of the
   data *in frames*.

   If the device is not in :const:`PCM_NONBLOCK` mode, this call will block
   if the kernel buffer is full, and until enough sound has been played
   to allow the sound data to be buffered.

   In :const:`PCM_NONBLOCK` mode, the call will return immediately, with a
   return value of zero, if the buffer is full. In this case, the data
   should be written again at a later time.

   In case of a buffer underrun, this function will return the negative
   size :const:`-EPIPE`, and no data is written.
   At this point, the playback was already corrupted. If you want to play
   the data nonetheless, call write again with the same data.
   To avoid the problem in the future, try using a larger period size
   and/or more periods, at the cost of higher latency.

   Note that this call completing means only that the samples were buffered
   in the kernel, and playout will continue afterwards. Make sure that the
   stream is drained before discarding the PCM handle.

.. method:: PCM.pause([enable: int = True]) -> int

   If *enable* is :const:`True`, playback or capture is paused.
   Otherwise, playback/capture is resumed.

.. method:: PCM.drop() -> int

   Stop the stream and drop residual buffered frames.

   *New in 0.9*

.. method:: PCM.drain() -> int

   For :const:`PCM_PLAYBACK` PCM objects, play residual buffered frames
   and then stop the stream. In :const:`PCM_NORMAL` mode,
   this function blocks until all pending playback is drained.

   For :const:`PCM_CAPTURE` PCM objects, this function is not very useful.

   *New in 0.10*

.. method:: PCM.close() -> None

   Closes the PCM device.

   For :const:`PCM_PLAYBACK` PCM objects in :const:`PCM_NORMAL` mode,
   this function blocks until all pending playback is drained.

.. method:: PCM.polldescriptors() -> list[tuple[int, int]]

   Returns a list of tuples of *(file descriptor, eventmask)* that can be
   used to wait for changes on the PCM with *select.poll*.

   The *eventmask* value is compatible with `poll.register`__ in the Python
   :const:`select` module.

.. method:: PCM.polldescriptors_revents(descriptors: list[tuple[int, int]]) -> int

   Processes the descriptor list returned by :func:`polldescriptors` after
   using it with *select.poll*, and returns a single *eventmask* value that
   is meaningful for deciding whether :func:`read` or :func:`write` should
   be called.

   *New in 0.11*

.. method:: PCM.set_tstamp_mode([mode: int = PCM_TSTAMP_ENABLE])

   Set the ALSA timestamp mode on the device. The mode argument can be set to
   either :const:`PCM_TSTAMP_NONE` or :const:`PCM_TSTAMP_ENABLE`.

.. method:: PCM.get_tstamp_mode() -> int

   Return the integer value corresponding to the ALSA timestamp mode. The
   return value can be either :const:`PCM_TSTAMP_NONE` or :const:`PCM_TSTAMP_ENABLE`.

.. method:: PCM.set_tstamp_type([type: int = PCM_TSTAMP_TYPE_GETTIMEOFDAY]) -> None

   Set the ALSA timestamp mode on the device. The type argument
   can be set to either :const:`PCM_TSTAMP_TYPE_GETTIMEOFDAY`,
   :const:`PCM_TSTAMP_TYPE_MONOTONIC` or :const:`PCM_TSTAMP_TYPE_MONOTONIC_RAW`.

.. method:: PCM.get_tstamp_type() -> int

   Return the integer value corresponding to the ALSA timestamp type. The
   return value can be either :const:`PCM_TSTAMP_TYPE_GETTIMEOFDAY`,
   :const:`PCM_TSTAMP_TYPE_MONOTONIC` or :const:`PCM_TSTAMP_TYPE_MONOTONIC_RAW`.

.. method:: PCM.htimestamp() -> tuple[int, int, int]

   Return a Python tuple *(seconds, nanoseconds, frames_available_in_buffer)*.

   The type of output is controlled by the tstamp_type, as described in the table below.

   =================================  ===========================================
            Timestamp Type                             Description
   =================================  ===========================================
   ``PCM_TSTAMP_TYPE_GETTIMEOFDAY``   System-wide realtime clock with seconds
                                      since epoch.
   ``PCM_TSTAMP_TYPE_MONOTONIC``      Monotonic time from an unspecified starting
                                      time. Progress is NTP synchronized.
   ``PCM_TSTAMP_TYPE_MONOTONIC_RAW``  Monotonic time from an unspecified starting
                                      time using only the system clock.
   =================================  ===========================================

   The timestamp mode is controlled by the tstamp_mode, as described in the table below.

   =================================  ===========================================
            Timestamp Mode                             Description
   =================================  ===========================================
   ``PCM_TSTAMP_NONE``                No timestamp.
   ``PCM_TSTAMP_ENABLE``              Update timestamp at every hardware position
                                      update.
   =================================  ===========================================

**A few hints on using PCM devices for playback**

The most common reason for problems with playback of PCM audio is that writes
to PCM devices must *exactly* match the data rate of the device.

If too little data is written to the device, it will underrun, and
ugly clicking sounds will occur. Conversely, if too much data is
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

.. class:: Mixer(control: str = 'Master', id: int = 0, cardindex: int = -1, device: str = 'default') -> Mixer

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

.. method:: Mixer.cardname() -> str

   Return the name of the sound card used by this Mixer object

.. method:: Mixer.mixer() -> str

   Return the name of the specific mixer controlled by this object, For example
   ``'Master'`` or ``'PCM'``

.. method:: Mixer.mixerid() -> int

   Return the ID of the ALSA mixer controlled by this object.

.. method:: Mixer.switchcap() -> int

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

.. method:: Mixer.volumecap() -> int

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

.. method:: Mixer.getenum() -> tuple[str, list[str]]

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

.. method:: Mixer.setenum(index: int) -> None

   For enumerated controls, sets the currently selected item.
   *index* is an index into the list of available enumerated items returned
   by :func:`getenum`.

.. method:: Mixer.getrange(pcmtype: int = PCM_PLAYBACK, units: int = VOLUME_UNITS_RAW) -> tuple[int, int]

   Return the volume range of the ALSA mixer controlled by this object.
   The value is a tuple of integers whose meaning is determined by the
   *units* argument.

   The optional *pcmtype* argument can be either :const:`PCM_PLAYBACK` or
   :const:`PCM_CAPTURE`, which is relevant if the mixer can control both
   playback and capture volume.  The default value is :const:`PCM_PLAYBACK`
   if the mixer has playback channels, otherwise it is :const:`PCM_CAPTURE`.

   The optional *units* argument can be one of :const:`VOLUME_UNITS_PERCENTAGE`,
   :const:`VOLUME_UNITS_RAW`, or :const:`VOLUME_UNITS_DB`.

.. method:: Mixer.getvolume(pcmtype: int = PCM_PLAYBACK, units: int = VOLUME_UNITS_PERCENTAGE) -> int

   Returns a list with the current volume settings for each channel. The list
   elements are integers whose meaning is determined by the *units* argument.

   The optional *pcmtype* argument can be either :const:`PCM_PLAYBACK` or
   :const:`PCM_CAPTURE`, which is relevant if the mixer can control both
   playback and capture volume. The default value is :const:`PCM_PLAYBACK`
   if the mixer has playback channels, otherwise it is :const:`PCM_CAPTURE`.

   The optional *units* argument can be one of :const:`VOLUME_UNITS_PERCENTAGE`,
   :const:`VOLUME_UNITS_RAW`, or :const:`VOLUME_UNITS_DB`.

.. method:: Mixer.setvolume(volume: int, pcmtype: int = PCM_PLAYBACK, units: int = VOLUME_UNITS_PERCENTAGE, channel: (int | None) = None) -> None

   Change the current volume settings for this mixer. The *volume* argument
   is an integer whose meaning is determined by the *units* argument.

   If the optional argument *channel* is present, the volume is set
   only for this channel. This assumes that the mixer can control the
   volume for the channels independently.

   The optional *pcmtype* argument can be either :const:`PCM_PLAYBACK` or
   :const:`PCM_CAPTURE`, which is relevant if the mixer can control both
   playback and capture volume. The default value is :const:`PCM_PLAYBACK`
   if the mixer has playback channels, otherwise it is :const:`PCM_CAPTURE`.

   The optional *units* argument can be one of :const:`VOLUME_UNITS_PERCENTAGE`,
   :const:`VOLUME_UNITS_RAW`, or :const:`VOLUME_UNITS_DB`.

.. method:: Mixer.getmute() -> list[int]

   Return a list indicating the current mute setting for each channel.
   0 means not muted, 1 means muted.

   This method will fail if the mixer has no playback switch capabilities.

.. method:: Mixer.setmute(mute: bool, channel: (int | None) = None) -> None

   Sets the mute flag to a new value. The *mute* argument is either 0 for not
   muted, or 1 for muted.

   The optional *channel* argument controls which channel is
   muted. The default is to set the mute flag for all channels.

   This method will fail if the mixer has no playback mute capabilities

.. method:: Mixer.getrec() -> list[int]

   Return a list indicating the current record mute setting for each channel.
   0 means not recording, 1 means recording.

   This method will fail if the mixer has no capture switch capabilities.

.. method:: Mixer.setrec(capture: int, channel: (int | None) = None) -> None

   Sets the capture mute flag to a new value. The *capture* argument
   is either 0 for no capture, or 1 for capture.

   The optional *channel* argument controls which channel is
   changed. The default is to set the capture flag for all channels.

   This method will fail if the mixer has no capture switch capabilities.

.. method:: Mixer.polldescriptors() -> list[tuple[int, int]]

   Returns a list of tuples of *(file descriptor, eventmask)* that can be
   used to wait for changes on the mixer with *select.poll*.

   The *eventmask* value is compatible with `poll.register`__ in the Python
   :const:`select` module.

.. method:: Mixer.handleevents() -> int

   Acknowledge events on the :func:`polldescriptors` file descriptors
   to prevent subsequent polls from returning the same events again.
   Returns the number of events that were acknowledged.

.. method:: Mixer.close() -> None

   Closes the Mixer device.

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
