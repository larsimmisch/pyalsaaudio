# Makefile for PyAlsaAudio documentation
#
# In order to use this you need an unpacked version of the
# Python source available. Set the source path below
#
# You also need a working latex installation, and the latex2html
# tool installed.
PYTHONSOURCE = /usr/src/Python-2.4.1/

# Shouldn't need to change anything below here!

MKHOWTO = $(PYTHONSOURCE)/Doc/tools/mkhowto

all:
	$(MKHOWTO) --dir .. --html pyalsaaudio.tex

text:
	$(MKHOWTO) --dir .. --text pyalsaaudio.tex


