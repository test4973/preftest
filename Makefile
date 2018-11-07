

CFLAGS ?= -O3
CFLAGS += -Wall -Wextra


.PHONY: default
default: benchDec

benchDec: CPPFLAGS += -DNDEBUG
benchDec: bench.o main.o zfgen.o zfdec.o util.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

.PHONY: test
test: benchDec
	./benchDec

.PHONY: clean
clean:
	$(RM) *.o benchDec
