#!/usr/bin/env python

'''This package contains wrappers for accessing the ALSA API from Python.
It is fairly complete for PCM devices and Mixer access.
'''

from distutils.core import setup
from distutils.extension import Extension
from sys import version

# patch distutils if it can't cope with the "classifiers" or
# "download_url" keywords
from sys import version
if version < '2.2.3':
    from distutils.dist import DistributionMetadata
    DistributionMetadata.classifiers = None
    DistributionMetadata.download_url = None
    
setup(
    name = 'pyalsaaudio',
    version = '0.4',
    description = 'ALSA bindings',
    long_description = __doc__,
    author = 'Casper Wilstrup',
    author_email='cwi@aves.dk',
    # maintainer = 'Lars Immisch',
    # maintainer_email = 'lars@ibp.de',
    license='PSF',
    platforms=['posix'],
    url='http://pyalsaaudio.sourceforge.net/',
    classifiers = [
    'Development Status :: 4 - Beta',
    'Intended Audience :: Developers',
    'License :: OSI Approved :: Python Software Foundation License',
    'Operating System :: POSIX :: Linux',
    'Topic :: Multimedia :: Sound/Audio',
    'Topic :: Multimedia :: Sound/Audio :: Mixers',
    'Topic :: Multimedia :: Sound/Audio :: Players',
    'Topic :: Multimedia :: Sound/Audio :: Capture/Recording',
    ],
    ext_modules=[Extension('alsaaudio',['alsaaudio.c'],libraries=['asound'])]
    )
