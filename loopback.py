#!/usr/bin/env python3
# -*- mode: python; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-

import sys
import select
import logging
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

class AlsaEvent(object):
	def __init__(self, name, alsaobject):
		self.name = name
		self.fd, self.mask = alsaobject.polldescriptors()[0]

	def handle_event(self, mask):
		'''Override this if you need to handle events'''
		pass


class CaptureEvent(AlsaEvent):

	def __init__(self, name, capture, playback):
		super().__init__(name, capture)
		self.playback = playback
		self.capture = capture

	def start(self):
		# start reading data
		self.capture.read()

	def handle_event(self, mask):
		size, data = self.capture.read()
		if not size:
			logging.warning(f'underrun')
			return

		written = self.playback.write(data)
		if not written:
			# if this happened, we might push the data to a queue
			logging.warning('overrun')
		else:
			logging.debug(f'{self.name} wrote {size}: {written}')


class VolumeEvent(AlsaEvent):
	def __init__(self, name, capture_control, playback_control):
		super().__init__(name, capture_control)
		self.playback_control = playback_control
		self.capture_control = capture_control

	def handle_event(self, mask):
		volume = self.capture_control.getvolume(pcmtype=PCM_CAPTURE)
		# it looks as if we need to get the playback volume to reset the event
		self.capture_control.getvolume()
		logging.info(f'{self.name} adjusting volume to {volume}')
		if volume:
			self.playback_control.setvolume(volume[0])


class Reactor(object):
	'''A wrapper around select.poll'''

	def __init__(self):
		self.poll = select.poll()
		self.events = {}

	def register(self, event, mask=None):
		logging.debug(f'registered {event.name}: {poll_desc(event.mask)}')
		self.events[event.fd] = event

		# allow overriding of mask
		if mask is None:
			mask = event.mask

		self.poll.register(event.fd, mask)

	def unregister(self, event):
		self.poll.unregister(event.fd)
		del self.events[event.fd]

	def run(self):
		while True:
			events = self.poll.poll()
			for fd, ev in events:
				handler = self.events[fd]

				# warn about unexpected/unhandled events
				if ev & (select.POLLERR | select.POLLHUP | select.POLLNVAL | select.POLLRDHUP):
					logging.warning(f'{handler.name}: {poll_desc(ev)} ({ev})')
				else:
					logging.debug(f'{handler.name}: {poll_desc(ev)} ({ev})')

				handler.handle_event(ev)

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

	capture_handler = CaptureEvent('capture', capture, playback)
	playback_handler = AlsaEvent('playback', playback)

	reactor = Reactor()

	capture_handler.start()

	reactor.register(capture_handler)
	reactor.register(playback_handler)

	# If args.input_mixer and args.output_mixer are set, forward the capture volume to the playback volume.
	# The usecase is a capture device that is implemented using g_audio, i.e. the Linux USB gadget driver.
	# When a USB device (eg. an iPad) is connected to this machine, its volume events will to the volume control
	# of the output device
	if args.input_mixer and args.output_mixer:
		playback_control = Mixer(control=args.output_mixer, cardindex=playback.info()['card_no'])
		capture_control = Mixer(control=args.input_mixer, cardindex=capture.info()['card_no'])

		volume_handler = VolumeEvent('volume', capture_control, playback_control)
		reactor.register(volume_handler, select.POLLIN)

	reactor.run()
