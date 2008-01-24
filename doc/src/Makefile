# Makefile for PyAlsaAudio documentation
#
# In order to use this you need an unpacked version of the
# Python source available. Set the source path below
#
# You also need a working latex installation, and the latex2html
# tool installed.
PYTHONSOURCE = /usr/local/src/Python-2.5.1/
SFUSER = larsimmisch

# Shouldn't need to change anything below here!

MKHOWTO = $(PYTHONSOURCE)/Doc/tools/mkhowto

all:
	$(MKHOWTO) --dir .. --html pyalsaaudio.tex
	python sffixup.py ../index.html

text:
	$(MKHOWTO) --dir .. --text pyalsaaudio.tex

install:
	scp ../*.html ../*.gif ../*.png $(SFUSER)@shell.sourceforge.net:/home/groups/p/py/pyalsaaudio/htdocs