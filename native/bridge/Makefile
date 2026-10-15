# Host build and test for the native bridge parts. C89, warnings as errors.
CC ?= cc
CFLAGS ?= -std=c89 -pedantic-errors -Wall -Wextra -Werror -Wshadow -Wstrict-prototypes -Wmissing-prototypes -O1 -g

PARTS_OBJ = config.o token.o sha256.o fsstore.o pairing.o policy.o routes.o profile.o profile_route.o validate.o write_routes.o account_routes.o content_routes.o search_routes.o collection_routes.o post_routes.o plim.o image_route.o token_persist.o

TESTS = test_config test_token test_fsstore test_pairing test_policy test_routes test_profile test_profile_route test_validate test_write_routes test_account_routes test_content_routes test_search_routes test_collection_routes test_post_routes test_image_route test_token_persist

all: test

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

plim.o: ../plim/plim.c
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
	rm -f *.o $(TESTS) test_profile_wolfram test_profile_live test_publish_live test_image_source_wolfram test_server test_image_cdn

.PHONY: all test clean

# Live tests: the Wolfram fetch and publish run against Wolfram's library and a mock
# PDS on localhost (no network). They need Wolfram built as a host library first:
#   cmake -S $(WOLFRAM_ROOT) -B $(WOLFRAM_HOST_BUILD) -DWOLFRAM_BUILD_TESTS=OFF -DWOLFRAM_BUILD_EXAMPLES=OFF
#   cmake --build $(WOLFRAM_HOST_BUILD) --target wolfram
WOLFRAM_HOST_BUILD ?= /tmp/wf-host
LIVE_CFLAGS = -std=c2x -pedantic -Wall -Wextra -Werror -Wshadow -I$(WOLFRAM_ROOT)/include -I$(WOLFRAM_ROOT)/test -I/usr/include/cjson -I.
LIVE_LIBS = -L$(WOLFRAM_HOST_BUILD) -lwolfram -lcjson -lmicrohttpd -Wl,-rpath,$(WOLFRAM_HOST_BUILD)

mock_pds.o:
	$(CC) -std=c2x -I$(WOLFRAM_ROOT)/include -I$(WOLFRAM_ROOT)/test -c $(WOLFRAM_ROOT)/test/mock_pds.c -o $@

test_profile_live: test_profile_live.c profile_live.c profile_route.c profile_wolfram.c profile.c validate.c mock_pds.o
	$(CC) $(LIVE_CFLAGS) test_profile_live.c profile_live.c profile_route.c profile_wolfram.c profile.c validate.c mock_pds.o $(LIVE_LIBS) -o $@

test_publish_live: test_publish_live.c publish_live.c post_routes.c profile.c validate.c mock_pds.o
	$(CC) $(LIVE_CFLAGS) test_publish_live.c publish_live.c post_routes.c profile.c validate.c mock_pds.o $(LIVE_LIBS) -o $@

test_image_source_wolfram: test_image_source_wolfram.c image_source_wolfram.c image_route.c plim.o
	$(CC) $(LIVE_CFLAGS) test_image_source_wolfram.c image_source_wolfram.c $(LIVE_LIBS) -o $@

SERVER_SRCS = server.c server_http.c profile.c profile_route.c profile_live.c profile_wolfram.c validate.c post_routes.c publish_live.c routes.c ../plim/plim.c image_route.c image_cdn.c image_source_wolfram.c

test_server: test_server.c $(SERVER_SRCS) mock_pds.o
	$(CC) $(LIVE_CFLAGS) test_server.c $(SERVER_SRCS) mock_pds.o $(LIVE_LIBS) -o $@

test_image_cdn: test_image_cdn.c image_cdn.c image_route.c image_source_wolfram.c ../plim/plim.c
	$(CC) $(LIVE_CFLAGS) test_image_cdn.c image_cdn.c image_route.c image_source_wolfram.c ../plim/plim.c $(LIVE_LIBS) -o $@

# test_image_cdn makes a throwaway test certificate with openssl, in TMPDIR.
live: test_profile_live test_publish_live test_image_source_wolfram test_server test_image_cdn
	./test_profile_live
	./test_publish_live
	./test_image_source_wolfram
	./test_server
	./test_image_cdn

.PHONY: live
