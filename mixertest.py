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
## python mixertest.py Master mute   # mute the master mixer
## python mixertest.py Master unmute # unmute the master mixer


# Footnote: I'd normally use print instead of sys.std(out|err).write,
# but we're in the middle of the conversion between python 2 and 3
# and this code runs on both versions without conversion

import sys
import getopt
import alsaaudio

def list_mixers(idx=0):
    sys.stdout.write("Available mixer controls:\n")
    for m in alsaaudio.mixers(idx):
        sys.stdout.write("  '%s'\n" % m)
    
def show_mixer(name, idx=0):
    # Demonstrates how mixer settings are queried.
    try:
        mixer = alsaaudio.Mixer(name, cardindex=idx)
    except alsaaudio.ALSAAudioError:
        sys.stderr.write("No such mixer\n")
        sys.exit(1)

    sys.stdout.write("Mixer name: '%s'\n" % mixer.mixer())
    sys.stdout.write("Capabilities: %s %s\n" % (' '.join(mixer.volumecap()),
                                                ' '.join(mixer.switchcap())))
    volumes = mixer.getvolume()
    for i in range(len(volumes)):
        sys.stdout.write("Channel %i volume: %i%%\n" % (i,volumes[i]))
        
    try:
        mutes = mixer.getmute()
        for i in range(len(mutes)):
            if mutes[i]:
                sys.stdout.write("Channel %i is muted\n" % i)
    except alsaaudio.ALSAAudioError:
        # May not support muting
        pass

    try:
        recs = mixer.getrec()
        for i in range(len(recs)):
            if recs[i]:
                sys.stdout.write("Channel %i is recording\n" % i)
    except alsaaudio.ALSAAudioError:
        # May not support recording
        pass

def set_mixer(name, args, idx=0):
    # Demonstrates how to set mixer settings
    try:
        mixer = alsaaudio.Mixer(name, cardindex=idx)
    except alsaaudio.ALSAAudioError:
        sys.stderr.write("No such mixer")
        sys.exit(1)

    if args in ['mute','unmute']:
        # Mute/unmute the mixer
        if args == 'mute':
            mixer.setmute(1)
        else:
            mixer.setmute(0)
        sys.exit(0)
        
    if args in ['rec','unrec']:
        # Enable/disable recording
        if args == 'rec':
            mixer.setrec(1)
        else:
            mixer.setrec(0)
        sys.exit(0)
            
    if args.find(',') != -1:
        channel,volume = map(int,args.split(','))
    else:
        channel = alsaaudio.MIXER_CHANNEL_ALL
        volume = int(args)
    # Set volume for specified channel. MIXER_CHANNEL_ALL means set
    # volume for all channels
    mixer.setvolume(volume, channel)

def usage():
    sys.stderr.write('usage: mixertest.py [-c <card>] ' \
                     '[ <control>[,<value>]]\n')
    sys.exit(2)

if __name__ == '__main__':

    cardindex = 0
    opts, args = getopt.getopt(sys.argv[1:], 'c:')
    for o, a in opts:
        if o == '-c':
            cardindex = int(a)

    if not len(args):
        list_mixers(cardindex)
    elif len(args) == 1:
        show_mixer(args[0], cardindex)
    else:
        set_mixer(args[0], args[1], cardindex)




