#!/usr/bin/env python

## playbacktest.py
##
## This is an example of a simple sound playback script.
##
## The script opens an ALSA pcm for sound playback. Set
## various attributes of the device. It then reads data
## from stdin and writes it to the device.
##
## To test it out do the following:
## python recordtest.py out.raw # talk to the microphone
## python playbacktest.py out.raw


# Footnote: I'd normally use print instead of sys.std(out|err).write,
# but we're in the middle of the conversion between python 2 and 3
# and this code runs on both versions without conversion

import sys
import time
import getopt
import alsaaudio

def usage():
    sys.stderr.write('usage: playbacktest.py [-c <card>] <file>\n')
    sys.exit(2)

if __name__ == '__main__':

    card = 'default'

    opts, args = getopt.getopt(sys.argv[1:], 'c:')
    for o, a in opts:
        if o == '-c':
            card = a

    if not args:
        usage()

    f = open(args[0], 'rb')

    # Open the device in playback mode. 
    out = alsaaudio.PCM(alsaaudio.PCM_PLAYBACK, card=card)

    # Set attributes: Mono, 44100 Hz, 16 bit little endian frames
    out.setchannels(1)
    out.setrate(44100)
    out.setformat(alsaaudio.PCM_FORMAT_S16_LE)

    # The period size controls the internal number of frames per period.
    # The significance of this parameter is documented in the ALSA api.
    out.setperiodsize(160)

    # Read data from stdin
    data = f.read(320)
    while data:
        out.write(data)
        data = f.read(320)
            
        
