#!/usr/bin/env python3
# -*- mode: python; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-

import sys
import select
import logging
from collections import namedtuple
from queue import SimpleQueue
from alsaaudio import (PCM, pcms, PCM_PLAYBACK, PCM_CAPTURE, PCM_FORMAT_S16_LE, PCM_NONBLOCK, Mixer,
	PCM_STATE_OPEN, PCM_STATE_SETUP, PCM_STATE_PREPARED, PCM_STATE_RUNNING, PCM_STATE_XRUN, PCM_STATE_DRAINING,
	PCM_STATE_PAUSED, PCM_STATE_SUSPENDED, ALSAAudioError)
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

state_names = {
	PCM_STATE_OPEN: 'PCM_STATE_OPEN',
	PCM_STATE_SETUP: 'PCM_STATE_SETUP',
	PCM_STATE_PREPARED: 'PCM_STATE_PREPARED',
	PCM_STATE_RUNNING: 'PCM_STATE_RUNNING',
	PCM_STATE_XRUN: 'PCM_STATE_XRUN',
	PCM_STATE_DRAINING: 'PCM_STATE_DRAINING',
	PCM_STATE_PAUSED: 'PCM_STATE_PAUSED',
	PCM_STATE_SUSPENDED: 'PCM_STATE_SUSPENDED'
}

def poll_desc(mask):
	return '|'.join([poll_names[bit] for bit, name in poll_names.items() if mask & bit])

class PollDescriptor(object):
	'''File Descriptor, event mask and a name for logging'''
	def __init__(self, name, fd, mask):
		self.name = name
		self.fd = fd
		self.mask = mask

	def as_tuple(self):
		return (self.fd, self.mask)

	@classmethod
	def from_alsa_object(cls, name, alsaobject, mask=None):
		# TODO maybe refactor: we ignore objects that have more then one polldescriptor
		fd, alsamask = alsaobject.polldescriptors()[0]

		if mask is None:
			mask = alsamask

		return cls(name, fd, mask)

class Loopback(object):
	'''Loopback state and event handling'''

	def __init__(self, capture, playback_args):
		self.playback_args = playback_args

		self.playback = None
		self.playback_pd = None

		self.capture = capture
		self.capture_pd = PollDescriptor.from_alsa_object('capture', capture)

		self.queue = SimpleQueue()

	def register(self, reactor):
		reactor.register_timeout_handler(self.timeout)
		reactor.register(self.capture_pd, self)

	def start(self):
		# start reading data
		size, data = self.capture.read()
		if size:
			self.queue.put_nowait(data)

	def timeout(self):
		if self.playback:
			logging.info('timeout - closing playback device')
			self.playback.close()
			self.playback = None

	# unused
	def handle_playback_event(self, eventmask, name):
		# state = self.playback.state()
		# logging.error(f'{name} state: {state_names[state]}')
		# if state == PCM_STATE_RUNNING and not self.queue.empty():
		# 	written = self.playback.write(self.queue.get_nowait())
		# 	if not written:
		# 		logging.warning('overrun despite event')
		# 	else:
		# 		logging.debug(f'{name} wrote {written}: queue: {self.queue.qsize()}')
		avail = self.playback.avail()
		logging.debug(f'playback available: {avail}')

	def handle_capture_event(self, eventmask, name):
		'''called when data is available for reading'''
		size, data = self.capture.read()
		if not size:
			logging.warning(f'capture event but no data')
			return

		self.queue.put_nowait(data)

		if self.queue.qsize() <= 2:
			logging.info(f'buffering: {self.queue.qsize()}')
			return

		if self.playback:
			state = self.playback.state()
			if state == PCM_STATE_XRUN:
				# underrun: close the playback device and reopen it
				self.playback.close()
				self.playback = PCM(**self.playback_args)
		else:
			self.playback = PCM(**self.playback_args)
			logging.info('opened playback device')

		try:
			while not self.queue.empty():
				written = self.playback.write(self.queue.get_nowait())
				if not written:
					logging.warning('overrun')
				space = self.playback.avail()
				if space < self.playback_args['periodsize'] * self.queue.qsize():
					logging.debug(f'space available: {space} after write {written} queue: {self.queue.qsize()}')
					break

		except ALSAAudioError as e:
			logging.error('underrun', exc_info=1)

	def __call__(self, fd, eventmask, name):

		if fd == self.capture_pd.fd:
			real_mask = self.capture.polldescriptors_revents([self.capture_pd.as_tuple()])
			if real_mask:
				self.handle_capture_event(real_mask, name)
			else:
				logging.debug('null capture event')
		else:
			real_mask = self.playback.polldescriptors_revents([self.playback_pd.as_tuple()])
			if real_mask:
				self.handle_playback_event(real_mask, name)
			else:
				logging.debug('null playback event')


class VolumeForwarder(object):
	'''Volume control event handling'''

	def __init__(self, capture_control, playback_control):
		self.playback_control = playback_control
		self.capture_control = capture_control

	def __call__(self, fd, eventmask, name):
		volume = self.capture_control.getvolume(pcmtype=PCM_CAPTURE)
		# indicate that we've handled the event
		self.capture_control.handleevents()
		logging.info(f'{name} adjusting volume to {volume}')
		if volume:
			self.playback_control.setvolume(volume[0])


class Reactor(object):
	'''A wrapper around select.poll'''

	def __init__(self):
		self.poll = select.poll()
		self.descriptors = {}
		self.timeout_handlers = set()

	def register(self, polldescriptor, callable):
		logging.debug(f'registered {polldescriptor.name}: {poll_desc(polldescriptor.mask)}')
		self.descriptors[polldescriptor.fd] = (polldescriptor, callable)
		self.poll.register(polldescriptor.fd, polldescriptor.mask)

	def unregister(self, polldescriptor):
		self.poll.unregister(polldescriptor.fd)
		del self.descriptors[polldescriptor.fd]

	def register_timeout_handler(self, callable):
		self.timeout_handlers.add(callable)

	def unregister_timeout_handler(self, callable):
		self.timeout_handlers.remove(callable)

	def run(self):
		while True:
			# poll for a bit, then send a timeout to registered handlers
			events = self.poll.poll(250)
			for fd, ev in events:
				polldescriptor, handler = self.descriptors[fd]

				# very chatty - log all events
				logging.debug(f'{polldescriptor.name}: {poll_desc(ev)} ({ev})')

				handler(fd, ev, polldescriptor.name)

			if not events:
				for t in self.timeout_handlers:
					t()


if __name__ == '__main__':

	logging.basicConfig(format='%(asctime)s %(levelname)s %(message)s', level=logging.INFO)

	parser = ArgumentParser(description='ALSA loopback (with volume forwarding)')

	playback_pcms = pcms(pcmtype=PCM_PLAYBACK)
	capture_pcms = pcms(pcmtype=PCM_CAPTURE)

	if not playback_pcms:
		logging.error('no playback PCM found')
		sys.exit(2)

	if not capture_pcms:
		logging.error('no capture PCM found')
		sys.exit(2)

	parser.add_argument('-d', '--debug', action='store_true')
	parser.add_argument('-i', '--input', default=capture_pcms[0])
	parser.add_argument('-o', '--output', default=playback_pcms[0])
	parser.add_argument('-r', '--rate', type=int, default=48000)
	parser.add_argument('-c', '--channels', type=int, default=2)
	parser.add_argument('-p', '--periodsize', type=int, default=480)
	parser.add_argument('-P', '--periods', type=int, default=2)
	parser.add_argument('-I', '--input-mixer', help='Control of the input mixer')
	parser.add_argument('-O', '--output-mixer', help='control of the output mixer')

	args = parser.parse_args()

	if args.debug:
		logging.getLogger().setLevel(logging.DEBUG)

	playback_args = {
		'type': PCM_PLAYBACK,
		'mode': PCM_NONBLOCK,
		'device': args.output,
		'rate': args.rate,
		'channels': args.channels,
		'periodsize': args.periodsize,
		'periods': args.periods
	}

	capture = PCM(type=PCM_CAPTURE, mode=PCM_NONBLOCK, device=args.input, rate=args.rate,
		channels=args.channels, periodsize=args.periodsize, periods=args.periods)

	loopback = Loopback(capture, playback_args)

	reactor = Reactor()

	# If args.input_mixer and args.output_mixer are set, forward the capture volume to the playback volume.
	# The usecase is a capture device that is implemented using g_audio, i.e. the Linux USB gadget driver.
	# When a USB device (eg. an iPad) is connected to this machine, its volume events will go to the volume control
	# of the output device


	playback = PCM(**playback_args)

	if args.input_mixer and args.output_mixer:
		playback_control = Mixer(control=args.output_mixer, cardindex=playback.info()['card_no'])
		capture_control = Mixer(control=args.input_mixer, cardindex=capture.info()['card_no'])

		volume_handler = VolumeForwarder(capture_control, playback_control)
		reactor.register(PollDescriptor.from_alsa_object('capture_control', capture_control, select.POLLIN), volume_handler)

	playback.close()

	loopback.register(reactor)

	loopback.start()

	reactor.run()
