#!/usr/bin/env python3
# -*- mode: python; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-

import sys
import select
import logging
import re
import struct
import subprocess
from datetime import datetime, timedelta
from alsaaudio import (PCM, pcms, PCM_PLAYBACK, PCM_CAPTURE, PCM_NONBLOCK, Mixer,
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

	def __init__(self, capture, playback_args, volume_handler, run_after_stop=None, run_before_start=None):
		self.playback_args = playback_args
		self.playback = None
		self.volume_handler = volume_handler
		self.capture_started = None
		self.last_capture_event = None

		self.capture = capture
		self.capture_pd = PollDescriptor.from_alsa_object('capture', capture)

		self.run_after_stop = None
		if run_after_stop:
			self.run_after_stop = run_after_stop.split(' ')

		self.run_before_start = None
		if run_before_start:
			self.run_before_start = run_before_start.split(' ')
		self.run_after_stop_did_run = False

		self.waitBeforeOpen = False
		self.queue = []

		self.period_size = 0
		self.silent_periods = 0

	@staticmethod
	def compute_energy(data):
		values = struct.unpack(f'{len(data)//2}h', data)
		e = 0
		for v in values:
			e = e + v * v

		return e

	@staticmethod
	def run_command(cmd):
		if cmd:
			rc = subprocess.run(cmd)
			if rc.returncode:
				logging.warning(f'run {cmd}, return code {rc.returncode}')
			else:
				logging.info(f'run {cmd}, return code {rc.returncode}')

	def register(self, reactor):
		reactor.register_timeout_handler(self.timeout_handler)
		reactor.register(self.capture_pd, self)

	def start(self):
		# start reading data
		size, data = self.capture.read()
		if size:
			self.queue.append(data)

	def timeout_handler(self):
		if self.playback and self.capture_started:
			if self.last_capture_event:
				if datetime.now() - self.last_capture_event > timedelta(seconds=2):
					logging.info('timeout - closing playback device')
					self.playback.close()
					self.playback = None
					self.capture_started = None
					if self.volume_handler:
						self.volume_handler.stop()
					self.run_command(self.run_after_stop)
			return

		self.waitBeforeOpen = False

		if not self.run_after_stop_did_run and not self.playback:
			if self.volume_handler:
				self.volume_handler.stop()
			self.run_command(self.run_after_stop)
			self.run_after_stop_did_run = True

	def pop(self):
		if len(self.queue):
			return self.queue.pop()
		else:
			return None

	def handle_capture_event(self, eventmask, name):
		'''called when data is available for reading'''
		self.last_capture_event = datetime.now()
		size, data = self.capture.read()
		if not size:
			logging.warning(f'capture event but no data')
			return False

		energy = self.compute_energy(data)
		logging.debug(f'energy: {energy}')

		# the usecase is a USB capture device where we get perfect silence when it's idle
		if energy == 0:
			self.silent_periods = self.silent_periods + 1

			# turn off playback after two seconds of silence
			# 2 channels * 2 seconds * 2 bytes per sample
			fps = self.playback_args['rate']  * 8 // (self.playback_args['periodsize'] * self.playback_args['periods'])

			logging.debug(f'{self.silent_periods} of {fps} silent periods: {self.playback}')

			if self.silent_periods > fps and self.playback:
				logging.info(f'closing playback due to silence')
				self.playback.close()
				self.playback = None
				if self.volume_handler:
					self.volume_handler.stop()
				self.run_command(self.run_after_stop)
				self.run_after_stop_did_run = True

			if not self.playback:
				return
		else:
			self.silent_periods = 0

		if not self.playback:
			if self.waitBeforeOpen:
				return False
			try:
				if self.volume_handler:
					self.volume_handler.start()
				self.run_command(self.run_before_start)
				self.playback = PCM(**self.playback_args)
				self.period_size = self.playback.info()['period_size']
				logging.info(f'opened playback device with period_size {self.period_size}')
			except ALSAAudioError as e:
				logging.info('opening PCM playback device failed: %s', e)
				self.waitBeforeOpen = True
				return False

			self.capture_started = datetime.now()
			logging.info(f'{self.playback} capture started: {self.capture_started}')

		self.queue.append(data)

		if len(self.queue) <= 2:
			logging.info(f'buffering: {len(self.queue)}')
			return False

		try:
			data = self.pop()
			if data:
				space = self.playback.avail()
				written = self.playback.write(data)
				logging.debug(f'wrote {written} bytes while space was {space}')
		except ALSAAudioError:
			logging.error('underrun', exc_info=1)

		return True

	def __call__(self, fd, eventmask, name):

		if fd == self.capture_pd.fd:
			real_mask = self.capture.polldescriptors_revents([self.capture_pd.as_tuple()])
			if real_mask:
				return self.handle_capture_event(real_mask, name)
			else:
				logging.debug('null capture event')
				return False
		else:
			real_mask = self.playback.polldescriptors_revents([self.playback_pd.as_tuple()])
			if real_mask:
				return self.handle_playback_event(real_mask, name)
			else:
				logging.debug('null playback event')
				return False

class VolumeForwarder(object):
	'''Volume control event handling'''

	def __init__(self, capture_control, playback_control):
		self.playback_control = playback_control
		self.capture_control = capture_control
		self.active = True
		self.volume = None

	def start(self):
		self.active = True
		if self.volume:
			self.volume = playback_control.setvolume(self.volume)

	def stop(self):
		self.active = False
		self.volume = self.playback_control.getvolume(pcmtype=PCM_CAPTURE)[0]

	def __call__(self, fd, eventmask, name):
		if not self.active:
			return

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
		last_timeout_ev = datetime.now()
		while True:
			# poll for a bit, then send a timeout to registered handlers
			events = self.poll.poll(0.25)
			for fd, ev in events:
				polldescriptor, handler = self.descriptors[fd]

				# very chatty - log all events
				# logging.debug(f'{polldescriptor.name}: {poll_desc(ev)} ({ev})')

				handler(fd, ev, polldescriptor.name)

			if datetime.now() - last_timeout_ev > timedelta(seconds=0.25):
				for t in self.timeout_handlers:
					t()
				last_timeout_ev = datetime.now()


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
	parser.add_argument('-r', '--rate', type=int, default=44100)
	parser.add_argument('-c', '--channels', type=int, default=2)
	parser.add_argument('-p', '--periodsize', type=int, default=444) # must be divisible by 6 for 44k1
	parser.add_argument('-P', '--periods', type=int, default=2)
	parser.add_argument('-I', '--input-mixer', help='Control of the input mixer, can contain the card index, e.g. Digital:2')
	parser.add_argument('-O', '--output-mixer', help='Control of the output mixer, can contain the card index, e.g. PCM:1')
	parser.add_argument('-A', '--run-after-stop', help='command to run when the capture device is idle/silent')
	parser.add_argument('-B', '--run-before-start', help='command to run when the capture device becomes active')
	parser.add_argument('-V', '--volume', help='Initial volume (default is leave unchanged)')

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

	reactor = Reactor()

	# If args.input_mixer and args.output_mixer are set, forward the capture volume to the playback volume.
	# The usecase is a capture device that is implemented using g_audio, i.e. the Linux USB gadget driver.
	# When a USB device (eg. an iPad) is connected to this machine, its volume events will go to the volume control
	# of the output device

	capture = None
	playback = None
	volume_handler = None
	if args.input_mixer and args.output_mixer:
		re_mixer = re.compile(r'([a-zA-Z0-9]+):?([0-9+])?')

		input_mixer_card = None
		m = re_mixer.match(args.input_mixer)
		if m:
			input_mixer = m.group(1)
			if m.group(2):
				input_mixer_card = int(m.group(2))
		else:
			parser.print_usage()
			sys.exit(1)

		output_mixer_card = None
		m = re_mixer.match(args.output_mixer)
		if m:
			output_mixer = m.group(1)
			if m.group(2):
				output_mixer_card = int(m.group(2))
		else:
			parser.print_usage()
			sys.exit(1)

		if input_mixer_card is None:
			capture = PCM(type=PCM_CAPTURE, mode=PCM_NONBLOCK, device=args.input, rate=args.rate,
				channels=args.channels, periodsize=args.periodsize, periods=args.periods)
			input_mixer_card = capture.info()['card_no']

		if output_mixer_card is None:
			playback = PCM(**playback_args)
			output_mixer_card = playback.info()['card_no']
			playback.close()

		playback_control = Mixer(control=output_mixer, cardindex=int(output_mixer_card))
		capture_control = Mixer(control=input_mixer, cardindex=int(input_mixer_card))

		volume_handler = VolumeForwarder(capture_control, playback_control)
		reactor.register(PollDescriptor.from_alsa_object('capture_control', capture_control, select.POLLIN), volume_handler)

	if args.volume and playback_control:
		playback_control.setvolume(int(args.volume))

	loopback = Loopback(capture, playback_args, volume_handler, args.run_after_stop, args.run_before_start)
	loopback.register(reactor)
	loopback.start()

	reactor.run()
