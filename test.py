#!/usr/bin/env python

# These are internal test. They shouldn't fail, but they don't cover all
# of the ALSA API. Most of all PCM.read and PCM.write are missing.
# These need to be tested by playbacktest.py and recordtest.py

# In case of a problem, run these tests. If they fail, file a bug report on
# http://sourceforge.net/projects/pyalsaaudio

import unittest
import alsaaudio


# we can't test read and write well - these are tested otherwise
PCMMethods = [('pcmtype', None),
              ('pcmmode', None),
              ('cardname', None),
              ('setchannels', (2,)), 
              ('setrate', (44100,)),
              ('setformat', (alsaaudio.PCM_FORMAT_S8,)),
              ('setperiodsize', (320,))]

# A clever test would look at the Mixer capabilities and selectively run the
# omitted tests, but I am too tired for that.

MixerMethods = [('cardname', None),
                ('mixer', None),
                ('mixerid', None),
                ('switchcap', None),
                ('volumecap', None),
                ('getvolume', None),
                ('getrange', None),
                ('getenum', None),
#                ('getmute', None),
#                ('getrec', None),
#                ('setvolume', (60,)),
#                ('setmute', (0,))
#                ('setrec', (0')),
                ]

class MixerTest(unittest.TestCase):
    """Test Mixer objects"""

    def testMixer(self):
        """Open a Mixer on every card"""

        # Mixers are addressed by index, not name
        for i in range(len(alsaaudio.cards())):
            mixers = alsaaudio.mixers(i)
            for m in mixers:
                mixer = alsaaudio.Mixer(m, cardindex=i)
                mixer.close()

    def testMixerAll(self):
        "Run common Mixer methods on an open object"

        mixers = alsaaudio.mixers()
        mixer = alsaaudio.Mixer(mixers[0])

        for m, a in MixerMethods:
            f = alsaaudio.Mixer.__dict__[m]
            if a is None:
                f(mixer)
            else:
                f(mixer, *a)

        mixer.close()

    def testMixerClose(self):
        """Run common Mixer methods on a closed object and verify it raises an 
        error"""

        mixers = alsaaudio.mixers()
        mixer = alsaaudio.Mixer(mixers[0])
        mixer.close()

        for m, a in MixerMethods:
            f = alsaaudio.Mixer.__dict__[m]
            if a is None:
                self.assertRaises(alsaaudio.ALSAAudioError, f, mixer)
            else:
                self.assertRaises(alsaaudio.ALSAAudioError, f, mixer, *a)

class PCMTest(unittest.TestCase):
    """Test PCM objects"""

    def testPCM(self):
        "Open a PCM object on every card"

        for i in range(len(alsaaudio.cards())):
            pcm = alsaaudio.PCM(i)
            pcm.close()

    def testPCMAll(self):
        "Run all PCM methods on an open object"

        pcm = alsaaudio.PCM()

        for m, a in PCMMethods:
            f = alsaaudio.PCM.__dict__[m]
            if a is None:
                f(pcm)
            else:
                f(pcm, *a)

        pcm.close()


    def testPCMClose(self):
        "Run all PCM methods on a closed object and verify it raises an error"

        pcm = alsaaudio.PCM()
        pcm.close()

        for m, a in PCMMethods:
            f = alsaaudio.PCM.__dict__[m]
            if a is None:
                self.assertRaises(alsaaudio.ALSAAudioError, f, pcm)
            else:
                self.assertRaises(alsaaudio.ALSAAudioError, f, pcm, *a)

if __name__ == '__main__':
    unittest.main()
