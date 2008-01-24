from distutils.core import setup
from distutils.extension import Extension

setup(
    name = "alsaaudio",
    version = "0.1",
    description = "alsa bindings",
    author = "Casper Wilstrup",
    author_email="cwi@unispeed.com",
    ext_modules=[Extension("alsaaudio",["alsaaudio.c"],libraries=['asound'])
                 ]
    )
    
    
