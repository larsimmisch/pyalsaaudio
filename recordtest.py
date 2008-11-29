#!/usr/bin/env python

## recordtest.py
##
## This is an example of a simple sound capture script.
##
## The script opens an ALSA pcm forsound capture. Set
## various attributes of the capture, and reads in a loop,
## writing the data to standard out.
##
## To test it out do the following:
## python recordtest.py out.raw # talk to the microphone
## aplay -r 8000 -f S16_LE -c 1 out.raw


# Footnote: I'd normally use print instead of sys.std(out|err).write,
# but we're in the middle of the conversion between python 2 and 3
# and this code runs on both versions without conversion

import sys
import time
import getopt
import alsaaudio

def usage():
    sys.stderr.write('usage: recordtest.py [-c <card>] <file>\n')
    sys.exit(2)

if __name__ == '__main__':

    card = 'default'

    opts, args = getopt.getopt(sys.argv[1:], 'c:')
    for o, a in opts:
        if o == '-c':
            card = a

    if not args:
        usage()

    f = open(args[0], 'wb')

    # Open the device in nonblocking capture mode. The last argument could
    # just as well have been zero for blocking mode. Then we could have
    # left out the sleep call in the bottom of the loop
    inp = alsaaudio.PCM(alsaaudio.PCM_CAPTURE, alsaaudio.PCM_NONBLOCK, card)

    # Set attributes: Mono, 44100 Hz, 16 bit little endian samples
    inp.setchannels(1)
    inp.setrate(44100)
    inp.setformat(alsaaudio.PCM_FORMAT_S16_LE)

    # The period size controls the internal number of frames per period.
    # The significance of this parameter is documented in the ALSA api.
    # For our purposes, it is suficcient to know that reads from the device
    # will return this many frames. Each frame being 2 bytes long.
    # This means that the reads below will return either 320 bytes of data
    # or 0 bytes of data. The latter is possible because we are in nonblocking
    # mode.
    inp.setperiodsize(160)
    
    loops = 1000000
    while loops > 0:
        loops -= 1
        # Read data from device
        l, data = inp.read()
      
        if l:
            f.write(data)
            time.sleep(.001)
