#!/usr/bin/env python3
# -*- mode: python; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-

import alsaaudio
import select

def echo(inpcm, outpcm):
	q = list()

	# setup the synchronous event loop
	# See https://docs.python.org/3/library/select.html#poll-objects for background
	reactor = select.poll()

	infd, inmask = inpcm.polldecriptors()
	outfd, outmask = outpcm.polldescriptors()

	write_started = False

	def write():
		data = q.pop()
		written = outpcm.write(data)
		if written < len(data):
			q.insert(0, data[written:])

	reactor.register(infd, inmask)
	reactor.register(outfd, outmask)

	while True:
		events = reactor.poll()
		for fd, event in events:
			if event == select.POLLIN and fd == infd:
				data = inpcm.read()
				q.append(data)
				if not write_started:
					write()
					write_started = True
			elif event == select.POLLOUT and fd == outfd:
				if not q:
					return
				write()