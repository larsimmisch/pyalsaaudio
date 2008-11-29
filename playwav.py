#!/usr/bin/env python

# Simple test script that plays (some) wav files


# Footnote: I'd normally use print instead of sys.std(out|err).write,
# but we're in the middle of the conversion between python 2 and 3
# and this code runs on both versions without conversion

import sys
import wave
import getopt
import alsaaudio

def play(device, f):    

    # Set attributes
    device.setchannels(f.getnchannels())
    device.setrate(f.getframerate())

    # We assume signed data, little endian
    if f.getsampwidth() == 1:
        device.setformat(alsaaudio.PCM_FORMAT_S8)
    elif f.getsampwidth() == 2:
        device.setformat(alsaaudio.PCM_FORMAT_S16_LE)
    elif f.getsampwidth() == 3:
        device.setformat(alsaaudio.PCM_FORMAT_S24_LE)
    elif f.getsampwidth() == 4:
        device.setformat(alsaaudio.PCM_FORMAT_S32_LE)
    else:
        raise ValueError('Unsupported format')
    
    data = f.readframes(320)
    while data:
        # Read data from stdin
        device.write(data)
        data = f.readframes(320)


def usage():
    sys.stderr.write('usage: playwav.py [-c <card>] <file>\n')
    sys.exit(2)

if __name__ == '__main__':

    card = 'default'

    opts, args = getopt.getopt(sys.argv[1:], 'c:')
    for o, a in opts:
        if o == '-c':
            card = a

    if not args:
        usage()
        
    f = wave.open(args[0], 'rb')
    device = alsaaudio.PCM(card=card)

    play(device, f)

    f.close()
