# PC test harness for the CoCoVGA W64 emulation (src/cocovga.c)
CC ?= cc
CFLAGS ?= -O2 -g -Wall -Wextra -std=c99

SRC = test_cocovga.c gen6847.c ../cocovga.c ../cocovga_font.c ../cocovga_bench.c

test_cocovga: $(SRC) gen6847.h ../cocovga.h ../cocovga_bench.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

test: test_cocovga
	./test_cocovga

clean:
	rm -f test_cocovga *.ppm *.png

.PHONY: test clean
