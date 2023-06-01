#!/usr/bin/env python3
# -*- mode: python; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-

import sys
import select
import logging
from collections import namedtuple
from alsaaudio import PCM, pcms, PCM_PLAYBACK, PCM_CAPTURE, PCM_FORMAT_S16_LE, PCM_NONBLOCK, Mixer
from argparse import ArgumentParser

poll_names = {
	select.POLLIN: 'POLLIN',
	select.POLLPRI: 'POLLPRI',
	select.POLLOUT: 'POLLOUT',
	select.POLLERR: 'POLLERR',
	select.POLLHUP: 'POLLHUP',
	select.POLLRDHUP: 'POLLRDHUP',
	select.POLLNVAL: 'POLLNVAL'
}

def poll_desc(mask):
	return '|'.join([poll_names[bit] for bit, name in poll_names.items() if mask & bit])

class PollDescriptor(object):
	'''File Descriptor, event mask and a name for logging'''
	def __init__(self, name, fd, mask):
		self.name = name
		self.fd = fd
		self.mask = mask

	@classmethod
	def fromAlsaObject(cls, name, alsaobject, mask=None):
		fd, alsamask = alsaobject.polldescriptors()[0]

		if mask is None:
			mask = alsamask

		return cls(name, fd, mask)

class Loopback(object):
	'''Loopback state and event handling'''

	def __init__(self, capture, playback):
		self.playback = playback
		self.playback_pd = PollDescriptor.fromAlsaObject('playback', playback)

		self.capture = capture
		self.capture_pd = PollDescriptor.fromAlsaObject('capture', capture)

	def register(self, reactor):
		reactor.register(self.capture_pd, self)
		reactor.register(self.playback_pd, self)

	def start(self):
		# start reading data
		self.capture.read()

	def handle_playback_event(self, eventmask, name):
		pass

	def handle_capture_event(self, eventmask, name):
		size, data = self.capture.read()
		if not size:
			logging.warning(f'underrun')
			return

		written = self.playback.write(data)
		if not written:
			logging.warning('overrun')
		else:
			logging.debug(f'wrote {size}: {written}')

	def __call__(self, fd, eventmask, name):
		if fd == self.capture_pd.fd:
			self.handle_capture_event(eventmask, name)
		else:
			self.handle_playback_event(eventmask, name)


class VolumeForwarder(object):
	'''Volume control event handling'''

	def __init__(self, capture_control, playback_control):
		self.playback_control = playback_control
		self.capture_control = capture_control

	def __call__(self, fd, eventmask, name):
		volume = self.capture_control.getvolume(pcmtype=PCM_CAPTURE)
		# it looks as if we need to get the playback volume to reset the event
		self.capture_control.getvolume()
		logging.info(f'{name} adjusting volume to {volume}')
		if volume:
			self.playback_control.setvolume(volume[0])


class Reactor(object):
	'''A wrapper around select.poll'''

	def __init__(self):
		self.poll = select.poll()
		self.descriptors = {}

	def register(self, polldescriptor, callable):
		logging.debug(f'registered {polldescriptor.name}: {poll_desc(polldescriptor.mask)}')
		self.descriptors[polldescriptor.fd] = (polldescriptor, callable)

		self.poll.register(polldescriptor.fd, polldescriptor.mask)

	def unregister(self, polldescriptor):
		self.poll.unregister(polldescriptor.fd)
		del self.descriptors[polldescriptor.fd]

	def run(self):
		while True:
			events = self.poll.poll()
			for fd, ev in events:
				polldescriptor, handler = self.descriptors[fd]

				# warn about unexpected/unhandled events
				if ev & (select.POLLERR | select.POLLHUP | select.POLLNVAL | select.POLLRDHUP):
					logging.warning(f'{polldescriptor.name}: {poll_desc(ev)} ({ev})')
				else:
					logging.debug(f'{polldescriptor.name}: {poll_desc(ev)} ({ev})')

				handler(fd, ev, polldescriptor.name)


if __name__ == '__main__':

	logging.basicConfig(format='%(asctime)s %(message)s', level=logging.INFO)

	parser = ArgumentParser(description='ALSA loopback (with volume forwarding)')

	playback_pcms = pcms(pcmtype=PCM_PLAYBACK)
	capture_pcms = pcms(pcmtype=PCM_CAPTURE)

	if not playback_pcms:
		log.error('no playback PCM found')
		sys.exit(2)

	if not capture_pcms:
		log.error('no capture PCM found')
		sys.exit(2)

	parser.add_argument('-d', '--debug', action='store_true')
	parser.add_argument('-i', '--input', default=capture_pcms[0])
	parser.add_argument('-o', '--output', default=playback_pcms[0])
	parser.add_argument('-r', '--rate', type=int, default=48000)
	parser.add_argument('-c', '--channels', type=int, default=2)
	parser.add_argument('-p', '--periodsize', type=int, default=480)
	parser.add_argument('-P', '--periods', type=int, default=4)
	parser.add_argument('-I', '--input-mixer', help='Control of the input mixer')
	parser.add_argument('-O', '--output-mixer', help='control of the output mixer')

	args = parser.parse_args()

	if args.debug:
		logging.getLogger().setLevel(logging.DEBUG)

	playback = PCM(type=PCM_PLAYBACK, mode=PCM_NONBLOCK, device=args.output, rate=args.rate,
		channels=args.channels, periodsize=args.periodsize, periods=args.periods)

	capture = PCM(type=PCM_CAPTURE, mode=PCM_NONBLOCK, device=args.input, rate=args.rate,
		channels=args.channels, periodsize=args.periodsize, periods=args.periods)

	loopback = Loopback(capture, playback)

	reactor = Reactor()

	# If args.input_mixer and args.output_mixer are set, forward the capture volume to the playback volume.
	# The usecase is a capture device that is implemented using g_audio, i.e. the Linux USB gadget driver.
	# When a USB device (eg. an iPad) is connected to this machine, its volume events will to the volume control
	# of the output device
	if args.input_mixer and args.output_mixer:
		playback_control = Mixer(control=args.output_mixer, cardindex=playback.info()['card_no'])
		capture_control = Mixer(control=args.input_mixer, cardindex=capture.info()['card_no'])

		volume_handler = VolumeForwarder(capture_control, playback_control)
		reactor.register(PollDescriptor.fromAlsaObject('capture_control', capture_control, select.POLLIN), volume_handler)

	loopback.register(reactor)

	loopback.start()

	reactor.run()
