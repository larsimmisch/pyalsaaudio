''' Use this example from an interactive python session, for example:

>>> from isine import change
>>> change(880)
'''

from __future__ import print_function

from threading import Thread
from queue import Queue, Empty
from math import pi, sin
import struct
import alsaaudio

sampling_rate = 44100
format = alsaaudio.PCM_FORMAT_S16_LE
framesize = 2 # bytes per frame for the values above

def digitize(s):
    if s > 1.0 or s < -1.0:
        raise ValueError
        
    return struct.pack('h', int(s * 32767)) 

def generate(frequency):
    # generate a buffer with a sine wave of frequency
    size = int(sampling_rate / frequency)
    buffer = bytes()
    for i in range(size):
        buffer = buffer + digitize(sin(i/(2 * pi)))

    return buffer

class SinePlayer(Thread):
    
    def __init__(self, frequency = 440.0):
        Thread.__init__(self)
        self.setDaemon(True)
        self.device = alsaaudio.PCM()
        self.device.setchannels(1)
        self.device.setformat(format)
        self.device.setrate(sampling_rate)
        self.queue = Queue()
        self.change(frequency)

    def change(self, frequency):
        '''This is called outside of the player thread'''
        # we generate the buffer in the calling thread for less
        # latency when switching frequencies
        

        # More than 100 writes/s are pushing it - play multiple buffers
        # for higher frequencies

        factor = int(frequency/100.0)
        if factor == 0:
            factor = 1
        
        buf = generate(frequency) * factor
        print('factor: %d, frames: %d' % (factor, len(buf) / framesize))

        self.queue.put( buf)
                        
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
    
