## recordtest.py
##
## This is an example of a simple sound playback script.
##
## The script opens an ALSA pcm for sound playback. Set
## various attributes of the device. It then reads data
## from stdin and writes it to the device.
##
## To test it out do the following:
## python recordtest.py > out.raw # talk to the microphone
## python playbacktest.py < out.raw

import alsaaudio
import sys
import time

# Open the device in playback mode. 
out = alsaaudio.openpcm(alsaaudio.PCM_PLAYBACK)

# Set attributes: Mono, 8000 Hz, 16 bit little endian frames
out.setchannels(1)
out.setrate(8000)
out.setformat(alsaaudio.PCM_FORMAT_S16_LE)

# The period size controls the internal number of frames per period.
# The significance of this parameter is documented in the ALSA api.
out.setperiodsize(160)

loops = 10000
while loops > 0:
  loops -= 1
  # Read data from stdin
  data = sys.stdin.read(320)
  out.write(data)
