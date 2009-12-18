# Makefile for Sphinx documentation
#

# You can set these variables from the command line.
SPHINXOPTS    =
SPHINXBUILD   = sphinx-build
PAPER         =

# Internal variables.
PAPEROPT_a4     = -D latex_paper_size=a4
PAPEROPT_letter = -D latex_paper_size=letter
ALLSPHINXOPTS   = -d doctrees $(PAPEROPT_$(PAPER)) $(SPHINXOPTS) .

SFUSER = larsimmisch

.PHONY: help clean html web pickle htmlhelp latex changes linkcheck install

all: html

help:
	@echo "Please use \`make <target>' where <target> is one of"
	@echo "  html      to make standalone HTML files"
	@echo "  pickle    to make pickle files (usable by e.g. sphinx-web)"
	@echo "  htmlhelp  to make HTML files and a HTML help project"
	@echo "  latex     to make LaTeX files, you can set PAPER=a4 or PAPER=letter"
	@echo "  changes   to make an overview over all changed/added/deprecated items"
	@echo "  linkcheck to check all external links for integrity"

clean:
	-rm -rf html doctrees pickle htmlhelp latex changes linkcheck

html:
	mkdir -p html doctrees
	$(SPHINXBUILD) -b html $(ALLSPHINXOPTS) html
	@echo
	@echo "Build finished. The HTML pages are in html."

pickle:
	mkdir -p pickle doctrees
	$(SPHINXBUILD) -b pickle $(ALLSPHINXOPTS) pickle
	@echo
	@echo "Build finished; now you can process the pickle files or run"
	@echo "  sphinx-web pickle"
	@echo "to start the sphinx-web server."

web: pickle

htmlhelp:
	mkdir -p htmlhelp doctrees
	$(SPHINXBUILD) -b htmlhelp $(ALLSPHINXOPTS) htmlhelp
	@echo
	@echo "Build finished; now you can run HTML Help Workshop with the" \
	      ".hhp project file in htmlhelp."

latex:
	mkdir -p latex doctrees
	$(SPHINXBUILD) -b latex $(ALLSPHINXOPTS) latex
	@echo
	@echo "Build finished; the LaTeX files are in .build/latex."
	@echo "Run \`make all-pdf' or \`make all-ps' in that directory to" \
	      "run these through (pdf)latex."

changes:
	mkdir -p changes doctrees
	$(SPHINXBUILD) -b changes $(ALLSPHINXOPTS) changes
	@echo
	@echo "The overview file is in changes."

linkcheck:
	mkdir -p linkcheck doctrees
	$(SPHINXBUILD) -b linkcheck $(ALLSPHINXOPTS) linkcheck
	@echo
	@echo "Link check complete; look for any errors in the above output " \
	      "or in linkcheck/output.txt."

upload: html
	scp -r ./html/* $(SFUSER),pyalsaaudio@web.sourceforge.net:htdocs
