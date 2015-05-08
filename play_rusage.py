#!/usr/bin/env python

# Contributed by Stein Magnus Jodal

from __future__ import print_function

import resource
import time

import alsaaudio

seconds = 0
max_rss = 0
device = alsaaudio.PCM()

while True:
    device.write(b'\x00' * 44100)
    time.sleep(1)
    seconds += 1
    if seconds % 10 == 0:
        prev_rss = max_rss
        max_rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        diff_rss = max_rss - prev_rss
        print('After %ds: max RSS %d kB, increased %d kB' % (
            seconds, max_rss, diff_rss))
