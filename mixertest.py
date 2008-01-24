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

import alsaaudio
import sys

if len(sys.argv) == 1:
    # Demonstrates how to read the available mixers
    print "Available mixer controls:"
    for m in alsaaudio.mixers():
        print "  '%s'" % m

if len(sys.argv) == 2:
    # Demonstrates how mixer settings are queried.
    name = sys.argv[1]
    try:
        mixer = alsaaudio.Mixer(name)
    except alsaaudio.ALSAAudioError:
        print "No such mixer"
        sys.exit(1)

    print "Mixer name: '%s'"%mixer.mixer()
    print "Capabilities",mixer.volumecap()+mixer.switchcap()
    volumes = mixer.getvolume()
    for i in range(len(volumes)):
        print "Channel %i volume: %i%%"%(i,volumes[i])
        
    try:
        mutes = mixer.getmute()
        for i in range(len(mutes)):
            if mutes[i]: print "Channel %i is muted"%i
    except alsaaudio.ALSAAudioError:
        # May not support muting
        pass

    try:
        recs = mixer.getrec()
        for i in range(len(recs)):
            if recs[i]: print "Channel %i is recording"%i
    except alsaaudio.ALSAAudioError:
        # May not support recording
        pass

if (len(sys.argv)) == 3:
    # Demonstrates how to set mixer settings
    name = sys.argv[1]
    try:
        mixer = alsaaudio.Mixer(name)
    except alsaaudio.ALSAAudioError:
        print "No such mixer"
        sys.exit(1)

    args = sys.argv[2]
    if args in ['mute','unmute']:
        # Mute/unmute the mixer
        if args == 'mute': mixer.setmute(1)
        else: mixer.setmute(0)
        sys.exit(0)
    if args in ['rec','unrec']:
        # Enable/disable recording
        if args == 'rec': mixer.setrec(1)
        else: mixer.setrec(0)
        sys.exit(0)
        
            
    if args.find(',')!=-1:
        channel,volume = map(int,args.split(','))
    else:
        channel = alsaaudio.MIXER_CHANNEL_ALL
        volume = int(args)
    # Set volume for specified channel. MIXER_CHANNEL_ALL means set
    # volume for all channels
    mixer.setvolume(volume,channel)



