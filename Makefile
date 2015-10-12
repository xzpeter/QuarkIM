CFLAGS=-g -O0 -Wall -Werror
LDFLAGS=-lpthread

.PHONY: clean

clean:
	@rm -rf *.o core a.out
