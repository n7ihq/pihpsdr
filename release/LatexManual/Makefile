#
# Makefile to produce the PDF manual file from its LaTeX source
# Need several LaTex runs to get cross-references right etc.
#
# Note that you need a fairly complete LaTex installation to
# produce the PDF file. On my RaspPi "Bullseye" test system,
# the "texlive" and "texlive-latex-extra" packages had
# to be installed.
#

piHPSDR-Manual.pdf:	piHPSDR-Manual.tex
	pdflatex piHPSDR-Manual.tex
	pdflatex piHPSDR-Manual.tex
	pdflatex piHPSDR-Manual.tex

.PHONY: release
release:			piHPSDR-Manual.pdf
	cp piHPSDR-Manual.pdf ..

.PHONY: clean
clean:
	rm -f piHPSDR-Manual.log piHPSDR-Manual.toc piHPSDR-Manual.aux piHPSDR-Manual.pdf
