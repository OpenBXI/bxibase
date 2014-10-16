BASENAME=bxilog
DOT_SRC=$(wildcard *.dot)
MSC_SRC=$(wildcard *.msc)
# ODG_SRC=$(wildcard *.odg)
DOT_EPS=$(DOT_SRC:.dot=.eps)
MSC_EPS=$(MSC_SRC:.msc=.eps)
# ODG_EPS=$(ODG_SRC:.odg=.eps)

all: $(BASENAME).pdf

%.eps: %.dot
	dot -Teps $< -o $@

%.eps: %.msc
	mscgen -Teps $<
	
%.eps: %.odg
	office --headless --convert-to eps $<

bxilog.pdf: $(BASENAME).lyx $(DOT_EPS) $(MSC_EPS)
	lyx --export pdf $<

clean:
	@rm -rf $(DOT_EPS) $(MSC_EPS) $(BASENAME).tex $(BASENAME).aux $(BASENAME).log

.PHONY: all clean
