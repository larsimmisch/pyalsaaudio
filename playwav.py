#!/usr/bin/env python

# Simple test script that plays (some) wav files

import sys
import struct
import alsaaudio

# this is all a bit simplified, and won't cope with any wav extensions
# or multiple data chunks, but it's good enough here

WAV_FORMAT_PCM = 1
WAV_FORMAT_ALAW = 6
WAV_FORMAT_MULAW = 7
WAV_HEADER = '<4sl4s4slhhllhh4sl'
WAV_HEADER_SIZE = struct.calcsize(WAV_HEADER)

def _b(s):
    'Helper for 3.0 compatibility'
    
    if sys.version_info[0] >= 3:
        return bytes(s, 'UTF-8')

    return s

def wav_header_unpack(data):
    (riff, riffsize, wave, fmt, fmtsize, format, nchannels, framerate, 
     datarate, blockalign, bitspersample, data, datalength) \
     = struct.unpack(WAV_HEADER, data)

    print(data)

    if riff != _b('RIFF') or fmtsize != 16 or fmt != _b('fmt ') \
           or data != _b('data'):
        raise ValueError('wav header too complicated')
    
    return (format, nchannels, framerate, bitspersample, datalength)

def play(device, f):    
    header = f.read(WAV_HEADER_SIZE)
    format, nchannels, framerate, bitspersample, datalength \
            = wav_header_unpack(header)

    # Set attributes
    device.setchannels(nchannels)
    device.setrate(framerate)

    # We assume signed data, little endian
    if format == WAV_FORMAT_PCM:
        if bitspersample == 8:
            device.setformat(alsaaudio.PCM_FORMAT_S8)
        elif bitspersample == 16:
            device.setformat(alsaaudio.PCM_FORMAT_S16_LE)
        elif bitspersample == 24:
            device.setformat(alsaaudio.PCM_FORMAT_S24_LE)
        elif bitspersample == 32:
            device.setformat(alsaaudio.PCM_FORMAT_S32_LE)
    elif format == WAV_FORMAT_ALAW:
        device.setformat(alsaaudio.PCM_FORMAT_A_LAW)
    elif format == WAV_FORMAT_MULAW:
        device.setformat(alsaaudio.PCM_FORMAT_MU_LAW)
    else:
        raise ValueError('Unsupported format %d' % format)
    
    # The period size controls the internal number of frames per period.
    # The significance of this parameter is documented in the ALSA api.

    # rs = framerate / 25
    # out.setperiodsize(rs)

    data = f.read()
    while data:
        # Read data from stdin
        device.write(data)
        data = f.read()


if __name__ == '__main__':

    if len(sys.argv) < 2:
        print('usage: playwav.py <file>')
        sys.exit(2)

    f = open(sys.argv[1], 'rb')
    device = alsaaudio.PCM(alsaaudio.PCM_PLAYBACK)

    play(device, f)

    f.close()
