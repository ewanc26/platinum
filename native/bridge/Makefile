# Host build and test for the native bridge parts. C89, warnings as errors.
CC ?= cc
CFLAGS ?= -std=c89 -pedantic-errors -Wall -Wextra -Werror -Wshadow -Wstrict-prototypes -Wmissing-prototypes -O1 -g

PARTS_OBJ = config.o token.o sha256.o fsstore.o pairing.o policy.o routes.o profile.o profile_route.o validate.o write_routes.o account_routes.o content_routes.o

TESTS = test_config test_token test_fsstore test_pairing test_policy test_routes test_profile test_profile_route test_validate test_write_routes test_account_routes test_content_routes

all: test

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TESTS): %: %.c $(PARTS_OBJ)
	$(CC) $(CFLAGS) $< $(PARTS_OBJ) -o $@

# The Wolfram adapter uses Wolfram's C23 headers, so it is built apart from the
# C89 parts. WOLFRAM_ROOT is the sibling checkout of Wolfram.
WOLFRAM_ROOT ?= ../../../wolfram
C2X_CFLAGS = -std=c2x -pedantic -Wall -Wextra -Werror -Wshadow -I$(WOLFRAM_ROOT)/include -I/usr/include/cjson

test_profile_wolfram: test_profile_wolfram.c profile_wolfram.c profile.c
	$(CC) $(C2X_CFLAGS) -I. test_profile_wolfram.c profile_wolfram.c profile.c -o $@

test: $(TESTS) test_profile_wolfram
	@for t in $(TESTS) test_profile_wolfram; do ./$$t || exit 1; done

clean:
	rm -f *.o $(TESTS) test_profile_wolfram

.PHONY: all test clean
