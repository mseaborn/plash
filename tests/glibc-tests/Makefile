
tests := $(shell cat test-list)

all: $(tests)

%: %.c
	gcc -D_GNU_SOURCE -Wall $(shell cat $@.cflags) $< -ldl -o $@

clean:
	rm -v $(tests)
