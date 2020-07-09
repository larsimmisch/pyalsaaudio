''' Use this example from an interactive python session, for example:

>>> from isine import change
>>> change(880)
'''

from __future__ import print_function

import sys
from threading import Thread
from multiprocessing import Queue

if sys.version_info[0] < 3:
    from Queue import Empty
else:
    from queue import Empty

from math import pi, sin
import struct
import alsaaudio

sampling_rate = 48000

format = alsaaudio.PCM_FORMAT_S16_LE
framesize = 2 # bytes per frame for the values above
channels = 2

def nearest_frequency(frequency):
    # calculate the nearest frequency where the wave form fits into the buffer
    # in other words, select f so that sampling_rate/f is an integer
    return float(sampling_rate)/int(sampling_rate/frequency)

def generate(frequency, duration = 0.125):
    # generate a buffer with a sine wave of `frequency`
    # that is approximately `duration` seconds long

    # the buffersize we approximately want
    target_size = int(sampling_rate * channels * duration)

    # the length of a full sine wave at the frequency
    cycle_size = int(sampling_rate / frequency)

    # number of full cycles we can fit into target_size
    factor = int(target_size / cycle_size)

    size = max(int(cycle_size * factor), 1)
    
    sine = [ int(32767 * sin(2 * pi * frequency * i / sampling_rate)) \
             for i in range(size)]
             
    return struct.pack('%dh' % size, *sine)
                                                                                  

class SinePlayer(Thread):
    
    def __init__(self, frequency = 440.0):
        Thread.__init__(self)
        self.setDaemon(True)
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
                self.device.setperiodsize(int(len(buffer) / framesize))
                self.device.write(buffer)
            except Empty:
                if buffer:
                    self.device.write(buffer)
                

isine = SinePlayer()
isine.start()

def change(f):
    isine.change(f)
    
