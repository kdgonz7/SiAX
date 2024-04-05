CC=clang
SRC=stax.c
OUTFILE=stax
OPTIMIZE=-O0

all:
    $(CC) $(OPTIMIZE) $(SRC) -o $(OUTFILE)
