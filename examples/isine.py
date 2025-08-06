''' Use this example from an interactive python session, for example:

>>> from isine import change
>>> change(880)
'''

from __future__ import print_function

import sys
import time
from threading import Thread
from multiprocessing import Queue

if sys.version_info[0] < 3:
    from Queue import Empty
else:
    from queue import Empty

from math import pi, sin, ceil
import struct
import itertools
import alsaaudio

sampling_rate = 48000

format = alsaaudio.PCM_FORMAT_S16_LE
pack_format = 'h'  # short int, matching S16
channels = 2

def nearest_frequency(frequency):
    # calculate the nearest frequency where the wave form fits into the buffer
    # in other words, select f so that sampling_rate/f is an integer
    return float(sampling_rate)/int(sampling_rate/frequency)

def generate(frequency, duration = 0.125):
    # generate a buffer with a sine wave of `frequency`
    # that is approximately `duration` seconds long

    # the length of a full sine wave at the frequency
    cycle_size = int(sampling_rate / frequency)

    # number of full cycles we can fit into the duration
    factor = int(ceil(duration * frequency))

    # total number of frames
    size = cycle_size * factor

    sine = [ int(32767 * sin(2 * pi * frequency * i / sampling_rate)) \
             for i in range(size)]

    if channels > 1:
        sine = list(itertools.chain.from_iterable(itertools.repeat(x, channels) for x in sine))

    return struct.pack(str(size * channels) + pack_format, *sine)


class SinePlayer(Thread):

    def __init__(self, frequency = 440.0):
        Thread.__init__(self, daemon=True)
        self.device = alsaaudio.PCM(channels=channels, format=format, rate=sampling_rate)
        self.queue = Queue()
        self.change(frequency)

    def change(self, frequency):
        '''This is called outside of the player thread'''
        # we generate the buffer in the calling thread for less
        # latency when switching frequencies

        if frequency > sampling_rate / 2:
            raise ValueError('maximum frequency is %d' % (sampling_rate / 2))

        f = nearest_frequency(frequency)
        print('nearest frequency: %f' % f)

        buf = generate(f)
        self.queue.put(buf)

    def run(self):
        buffer = None
        while True:
            try:
                buffer = self.queue.get(False)
            except Empty:
                pass
            if buffer:
                if self.device.write(buffer) < 0:
                    print("Playback buffer underrun! Continuing nonetheless ...")


isine = SinePlayer()
isine.start()

time.sleep(1)
isine.change(1000)
time.sleep(1)

