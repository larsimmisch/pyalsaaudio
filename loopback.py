#!/usr/bin/env python3
# -*- mode: python; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-

import sys
import select
import logging
from alsaaudio import PCM, pcms, PCM_PLAYBACK, PCM_CAPTURE, PCM_FORMAT_S16_LE, PCM_NONBLOCK
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

class Loopback(object):

	def __init__(self, playback, capture):

		self.playback = playback
		self.playback_fd, self.playback_mask = playback.polldescriptors()[0]

		self.capture = capture
		self.capture_fd, self.capture_mask = capture.polldescriptors()[0]

		self.poll = select.poll()

		self.poll.register(self.playback_fd, self.playback_mask)
		logging.debug(f'registered playback: {poll_desc(self.playback_mask)}')

		self.poll.register(self.capture_fd, self.capture_mask)
		logging.debug(f'registered capture: {poll_desc(self.capture_mask)}')

	def fd_desc(self, fd):
		if fd == self.capture_fd:
			return 'capture'

		if fd == self.playback_fd:
			return 'playback'

		return 'unknown'

	def run(self):

		# start reading
		self.capture.read()

		while True:
			events = self.poll.poll()
			for fd, ev in events:
				logging.debug(f'{self.fd_desc(fd)}: {poll_desc(ev)} ({ev})')

				# This is very basic. We just write data as soon as we read it
				# and don't care for errors or the playback device not being ready
				if fd == self.capture_fd:
					size, data = self.capture.read()
					written = self.playback.write(data)
					logging.debug(f'wrote {size}: {written}')

if __name__ == '__main__':

	logging.basicConfig(format='%(asctime)s %(message)s', level=logging.INFO)

	parser = ArgumentParser(description='ALSA loopback')

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

	args = parser.parse_args()

	if args.debug:
		logging.getLogger().setLevel(logging.DEBUG)

	playback = PCM(type=PCM_PLAYBACK, mode=PCM_NONBLOCK, device=args.output, rate=args.rate,
		channels=args.channels, periodsize=args.periodsize, periods=args.periods)

	capture = PCM(type=PCM_CAPTURE, mode=PCM_NONBLOCK, device=args.input, rate=args.rate,
		channels=args.channels, periodsize=args.periodsize, periods=args.periods)

	loopback = Loopback(playback, capture)

	loopback.run()
