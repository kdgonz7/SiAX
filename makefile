CC=clang
SRC=stax.c
OUTFILE=stax
OPTIMIZE=-O2

all:
	$(CC) $(OPTIMIZE) $(SRC) -o output/$(OUTFILE) -g
