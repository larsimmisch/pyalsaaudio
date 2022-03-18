#!/usr/bin/env python

## mixertest.py
##
## This is an example of using the ALSA mixer API
##
## The script will set the volume or mute switch of the specified Mixer
## depending on command line options.
##
## Examples:
## python mixertest.py               # list available mixers
## python mixertest.py Master        # show Master mixer settings
## python mixertest.py Master 80     # set the master volume to 80%
## python mixertest.py Master 1,90   # set channel 1 volume to 90%
## python mixertest.py Master [un]mute   # [un]mute the master mixer
## python mixertest.py Capture [un]rec   # [dis/en]able capture
## python mixertest.py Master 0,[un]mute   # [un]mute channel 0
## python mixertest.py Capture 0,[un]rec   # [dis/en]able capture on channel 0

from __future__ import print_function

import sys
import getopt
import alsaaudio

def list_cards():
    print("Available sound cards:")
    for i in alsaaudio.card_indexes():
        (name, longname) = alsaaudio.card_name(i)
        print("  %d: %s (%s)" % (i, name, longname))

def list_mixers(kwargs):
    print("Available mixer controls:")
    for m in alsaaudio.mixers(**kwargs):
        print("  '%s'" % m)

def show_mixer(name, kwargs):
    # Demonstrates how mixer settings are queried.
    try:
        mixer = alsaaudio.Mixer(name, **kwargs)
    except alsaaudio.ALSAAudioError:
        print("No such mixer", file=sys.stderr)
        sys.exit(1)

    print("Mixer name: '%s'" % mixer.mixer())
    volcap = mixer.volumecap()
    print("Capabilities: %s %s" % (' '.join(volcap),
                                   ' '.join(mixer.switchcap())))

    if "Volume" in volcap or "Joined Volume" in volcap or "Playback Volume" in volcap:
        pmin, pmax = mixer.getrange(alsaaudio.PCM_PLAYBACK)
        pmin_keyword, pmax_keyword = mixer.getrange(pcmtype=alsaaudio.PCM_PLAYBACK, units=alsaaudio.VOLUME_UNITS_RAW)
        pmin_default, pmax_default = mixer.getrange()
        assert pmin == pmin_keyword
        assert pmax == pmax_keyword
        assert pmin == pmin_default
        assert pmax == pmax_default
        print("Raw playback volume range {}-{}".format(pmin, pmax))
        pmin_dB, pmax_dB = mixer.getrange(units=alsaaudio.VOLUME_UNITS_DB)
        print("dB playback volume range {}-{}".format(pmin_dB / 100.0, pmax_dB / 100.0))

    if "Capture Volume" in volcap or "Joined Capture Volume" in volcap:
        # Check that `getrange` works with keyword and positional arguments
        cmin, cmax = mixer.getrange(alsaaudio.PCM_CAPTURE)
        cmin_keyword, cmax_keyword = mixer.getrange(pcmtype=alsaaudio.PCM_CAPTURE, units=alsaaudio.VOLUME_UNITS_RAW)
        assert cmin == cmin_keyword
        assert cmax == cmax_keyword
        print("Raw capture volume range {}-{}".format(cmin, cmax))
        cmin_dB, cmax_dB = mixer.getrange(pcmtype=alsaaudio.PCM_CAPTURE, units=alsaaudio.VOLUME_UNITS_DB)
        print("dB capture volume range {}-{}".format(cmin_dB / 100.0, cmax_dB / 100.0))

    volumes = mixer.getvolume()
    volumes_dB = mixer.getvolume(units=alsaaudio.VOLUME_UNITS_DB)
    for i in range(len(volumes)):
        print("Channel %i volume: %i%% (%.1f dB)" % (i, volumes[i], volumes_dB[i] / 100.0))
        
    try:
        mutes = mixer.getmute()
        for i in range(len(mutes)):
            if mutes[i]:
                print("Channel %i is muted" % i)
    except alsaaudio.ALSAAudioError:
        # May not support muting
        pass

    try:
        recs = mixer.getrec()
        for i in range(len(recs)):
            if recs[i]:
                print("Channel %i is recording" % i)
    except alsaaudio.ALSAAudioError:
        # May not support recording
        pass

def set_mixer(name, args, kwargs):
    # Demonstrates how to set mixer settings
    try:
        mixer = alsaaudio.Mixer(name, **kwargs)
    except alsaaudio.ALSAAudioError:
        print("No such mixer", file=sys.stderr)
        sys.exit(1)

    if args.find(',') != -1:
        args_array = args.split(',')
        channel = int(args_array[0])
        args = ','.join(args_array[1:])
    else:
        channel = alsaaudio.MIXER_CHANNEL_ALL

    if args in ['mute', 'unmute']:
        # Mute/unmute the mixer
        if args == 'mute':
            mixer.setmute(1, channel)
        else:
            mixer.setmute(0, channel)
        
    elif args in ['rec','unrec']:
        # Enable/disable recording
        if args == 'rec':
            mixer.setrec(1, channel)
        else:
            mixer.setrec(0, channel)

    else:
        # Set volume for specified channel. MIXER_CHANNEL_ALL means set
        # volume for all channels
        volume = int(args)
        mixer.setvolume(volume, channel)

def usage():
    print('usage: mixertest.py [-c <card>] [ <control>[,<value>]]',
          file=sys.stderr)
    sys.exit(2)

if __name__ == '__main__':

    kwargs = {}
    opts, args = getopt.getopt(sys.argv[1:], 'c:d:?h')
    for o, a in opts:
        if o == '-c':
            kwargs = { 'cardindex': int(a) }
        elif o == '-d':
            kwargs = { 'device': a }
        else:
            usage()

    list_cards()

    if not len(args):
        list_mixers(kwargs)
    elif len(args) == 1:
        show_mixer(args[0], kwargs)
    else:
        set_mixer(args[0], args[1], kwargs)
