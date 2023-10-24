# Version 0.10.1
- restore previous xrun behaviour, #131
- type hints

# Version 0.10.0
- assorted improvements, (#123 from @ossilator)

# Version 0.9.2
- Fix alsamixer_getvolume (#112 from @stephensp)

# Version 0.9.1:
- Support decibel, percentage, and raw volumes in getvolume, setvolume, and getrange (#109 from @chrisdiamand)

# Version 0.9.0:
- Added keyword arguments for channels, format, rate and periodsize
- Deprecated `setchannel`, `setformat`, `setrate` and `setperiodsize`

# Version 0.8.6:
- Added four methods to the `PCM` class to allow users to get detailed information about the device:

  - `getformats()` returns a dictionary of name / value pairs, one for each of the card's
    supported formats - e.g. `{"U8": 1, "S16_LE": 2}`,
  - `getchannels()` returns a list of the supported channel numbers, e.g. `[1, 2]`,
  - `getrates()` returns supported sample rates for the device, e.g. `[48000]`,
  - `getratebounds()` returns the device's official minimum and maximum supported
    sample rates as a tuple, e.g. `(4000, 48000)`.

  (#82 contributed by @jdstmporter)

- Prevent hang on close after capturing audio (#80 contributed by @daym)

# Version 0.8.5:
- Return an empty string/bytestring when `read()` detects an
  overrun. Previously the returned data was undefined (contributed by @jcea)

- Unlimited setperiod buffer size when reading frames (contributed by @jcea)

# Version 0.8.4:
- Fix Python3 API usage broken in 0.8.3

# Version 0.8.3:
- Add DSD sample formats (contributed by @lintweaker)
- Add Mixer.handleevents() to acknowledge events identified by select.poll (contributed by @PaulSD)
- Add functions for listing cards and their names (contributed by @chrisdiamand)
- Add a method for setting enums (contributed by @chrisdiamand)

# Version 0.8.2:
- fix #3 (we cannot get the revision from git for pip installs)

# Version 0.8.1:
- document changes (this file)

# Version 0.8:
- `PCM()` has new `device` and `cardindex` keyword arguments.

  The keyword `device` allows to select virtual devices, `cardindex` can be
  used to select hardware cards by index (as with `mixers()` and `Mixer()`).

  The `card` keyword argument is still supported, but deprecated.

  The reason for this change is that the `card` keyword argument guessed
  a device name from the card name, but this only works sometimes, and breaks
  opening virtual devices.

- new function `pcms()` to list available PCM devices.

- `mixers()` and `Mixer()` take an additional `device` keyword argument.
  This allows to list or open virtual devices.

- The default behaviour of `Mixer()` without any arguments has changed.
  Now Mixer() will try to open the `default` Mixer instead of the Mixer
  that is associated with card 0.

- fix a memory leak under Python 3.x

- some more memory leaks in error conditions fixed.

# Version 0.7:
- fixed several memory leaks (patch 3372909), contributed by Erik Kulyk)

# Version 0.6:
- mostly reverted patch 2594366: alsapcm_setup did not do complete error
checking for good reasons; some ALSA functions in alsapcm_setup may fail without
rendering the device unusable

# Version 0.5:
- applied patch 2777035: Fixed setrec method in alsaaudio.c
  This included a mixertest with more features
- fixed/applied patch 2594366: alsapcm_setup does not do any error checking

# Version 0.4:
- API changes: mixers() and Mixer() now take a card index instead of a
  card name as optional parameter.
- Support for Python 3.0
- Documentation converted to reStructuredText; use Sphinx instead of LaTeX.
- added `cards()`
- added `PCM.close()`
- added `Mixer.close()`
- added `mixer.getenum()`

# Version 0.3:
- wrapped blocking calls with `Py_BEGIN_ALLOW_THREADS`/`Py_END_ALLOW_THREADS`
- added pause

# Version 0.2:
- Many bugfixes related to playback in particular
- Module documentation in the doc subdirectory

# Version 0.1:
- Initial version
