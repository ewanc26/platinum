# Host build and test for the native bridge parts. C89, warnings as errors.
CC ?= cc
CFLAGS ?= -std=c89 -pedantic-errors -Wall -Wextra -Werror -Wshadow -Wstrict-prototypes -Wmissing-prototypes -O1 -g

PARTS_OBJ = config.o token.o sha256.o fsstore.o pairing.o policy.o

TESTS = test_config test_token test_fsstore test_pairing test_policy

all: test

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TESTS): %: %.c $(PARTS_OBJ)
	$(CC) $(CFLAGS) $< $(PARTS_OBJ) -o $@

test: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

clean:
	rm -f *.o $(TESTS)

.PHONY: all test clean
