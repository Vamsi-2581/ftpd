# Builds the FTPD manuals.  Needs typst and the bookmaster submodule
# (git submodule update --init docs/books/bookmaster).

TYPST ?= typst
BM    := bookmaster
# The PDF carries the edition: ml06-0001-0.pdf is the Draft of ML06-0001.
BOOKS := ml06-0001-0.pdf ml06-0002-0.pdf

.PHONY: all clean site

all: $(BOOKS)

ml06-0001-%.pdf: ml06-0001.typ $(wildcard guide/*.typ) $(wildcard ex/*/*.txt) $(BM)/bookmaster.typ
	$(TYPST) compile --root . --font-path $(BM)/fonts --ignore-system-fonts $< $@

ml06-0002-%.pdf: ml06-0002.typ $(wildcard ref/*.typ) $(BM)/bookmaster.typ
	$(TYPST) compile --root . --font-path $(BM)/fonts --ignore-system-fonts $< $@

# The web form, as Read the Docs builds it, into site/.
site:
	sh web/build.sh site

clean:
	rm -rf *.pdf site
