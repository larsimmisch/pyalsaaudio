#!/usr/bin/env python

'''This package contains wrappers for accessing the ALSA API from Python.
It is fairly complete for PCM devices and Mixer access.
'''

from setuptools import setup
from setuptools.extension import Extension
from sys import version

pyalsa_version = '0.9.1'

if __name__ == '__main__':
    setup(
        name = 'pyalsaaudio',
        version = pyalsa_version,
        description = 'ALSA bindings',
        long_description = __doc__,
        author = 'Casper Wilstrup',
        author_email='cwi@aves.dk',
        maintainer = 'Lars Immisch',
        maintainer_email = 'lars@ibp.de',
        license='PSF',
        platforms=['posix'],
        url='http://larsimmisch.github.io/pyalsaaudio/',
        classifiers = [
            'Development Status :: 4 - Beta',
            'Intended Audience :: Developers',
            'License :: OSI Approved :: Python Software Foundation License',
            'Operating System :: POSIX :: Linux',
            'Programming Language :: Python :: 2',
            'Programming Language :: Python :: 3',    
            'Topic :: Multimedia :: Sound/Audio',
            'Topic :: Multimedia :: Sound/Audio :: Mixers',
            'Topic :: Multimedia :: Sound/Audio :: Players',
            'Topic :: Multimedia :: Sound/Audio :: Capture/Recording',
            ],
        ext_modules=[Extension('alsaaudio',['alsaaudio.c'], 
                               libraries=['asound'])]
    )
