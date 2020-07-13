#!/usr/bin/env python3
# -*- mode: python; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-

# These are internal tests. They shouldn't fail, but they don't cover all
# of the ALSA API. Most importantly PCM.read and PCM.write are missing.
# These need to be tested by playbacktest.py and recordtest.py

# In case of a problem, run these tests. If they fail, file a bug report on
# http://github.com/larsimmisch/pyalsaaudio/issues

import unittest
import alsaaudio
import warnings

# we can't test read and write well - these are tested otherwise
PCMMethods = [
	('pcmtype', None),
	('pcmmode', None),
	('cardname', None)
]

PCMDeprecatedMethods = [
	('setchannels', (2,)), 
	('setrate', (44100,)),
	('setformat', (alsaaudio.PCM_FORMAT_S8,)),
	('setperiodsize', (320,))
]

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
#				('getmute', None),
#				('getrec', None),
#				('setvolume', (60,)),
#				('setmute', (0,))
#				('setrec', (0')),
				]

class MixerTest(unittest.TestCase):
	"""Test Mixer objects"""

	def testMixer(self):
		"""Open the default Mixers and the Mixers on every card"""
		
		for c in alsaaudio.card_indexes():
			mixers = alsaaudio.mixers(cardindex=c)
			
			for m in mixers:
				mixer = alsaaudio.Mixer(m, cardindex=c)
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

		for c in alsaaudio.card_indexes():
			pcm = alsaaudio.PCM(cardindex=c)
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

	def testPCMDeprecated(self):
		with warnings.catch_warnings(record=True) as w:
			# Cause all warnings to always be triggered.
			warnings.simplefilter("always")

			try:
				pcm = alsaaudio.PCM(card='default')
			except alsaaudio.ALSAAudioError:
				pass
				
			# Verify we got a DepreciationWarning
			self.assertEqual(len(w), 1, "PCM(card='default') expected a warning" )
			self.assertTrue(issubclass(w[-1].category, DeprecationWarning), "PCM(card='default') expected a DeprecationWarning")

		for m, a in PCMDeprecatedMethods:
			with warnings.catch_warnings(record=True) as w:
				# Cause all warnings to always be triggered.
				warnings.simplefilter("always")

				pcm = alsaaudio.PCM()

				f = alsaaudio.PCM.__dict__[m]
				if a is None:
					f(pcm)
				else:
					f(pcm, *a)

				# Verify we got a DepreciationWarning
				method = "%s%s" % (m, str(a))
				self.assertEqual(len(w), 1, method + " expected a warning")
				self.assertTrue(issubclass(w[-1].category, DeprecationWarning), method + " expected a DeprecationWarning")

if __name__ == '__main__':
	unittest.main()
