
tests := $(patsubst %,bin/%,$(shell cat test-list))

all: $(tests)

bin/%: %.c
	mkdir -p `dirname $@`
	gcc -D_GNU_SOURCE -Wall $(shell cat $@.cflags) $< -ldl -lpthread -o $@

clean:
	rm -v $(tests)
