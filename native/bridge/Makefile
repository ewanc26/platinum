# Host build and test for the native bridge parts. C89, warnings as errors.
CC ?= cc
CFLAGS ?= -std=c89 -pedantic-errors -Wall -Wextra -Werror -Wshadow -Wstrict-prototypes -Wmissing-prototypes -O1 -g

# The sibling Wolfram checkout supplies the C23 headers and the host library the
# live tests link against. cJSON and libmicrohttpd come from Homebrew; the include
# paths below are the Homebrew locations, so a CI or other machine sets the two
# prefixes to its own.
WOLFRAM_ROOT ?= ../../../wolfram
WOLFRAM_HOST_BUILD ?= $(WOLFRAM_ROOT)/build-host
CJSON_PREFIX ?= /opt/homebrew
CJSON_INCLUDE = $(CJSON_PREFIX)/include/cjson
MHD_INCLUDE = $(CJSON_PREFIX)/include

PARTS_OBJ = config.o token.o sha256.o fsstore.o pairing.o policy.o routes.o profile.o profile_route.o validate.o write_routes.o account_routes.o content_routes.o search_routes.o collection_routes.o named_routes.o thread_route.o post_routes.o plim.o image_route.o token_persist.o timeline_route.o notifications_route.o

TESTS = test_config test_token test_fsstore test_pairing test_policy test_routes test_profile test_profile_route test_validate test_write_routes test_account_routes test_content_routes test_search_routes test_collection_routes test_thread_route test_post_routes test_image_route test_token_persist test_timeline_route test_notifications_route

all: test

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

plim.o: ../plim/plim.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TESTS): %: %.c $(PARTS_OBJ)
	$(CC) $(CFLAGS) $< $(PARTS_OBJ) -o $@

# The Wolfram adapter uses Wolfram's C23 headers, so it is built apart from the
# C89 parts.
C2X_CFLAGS = -std=c2x -pedantic -Wall -Wextra -Werror -Wshadow -I$(WOLFRAM_ROOT)/include -I$(CJSON_INCLUDE)

test_profile_wolfram: test_profile_wolfram.c profile_wolfram.c profile.c
	$(CC) $(C2X_CFLAGS) -I. test_profile_wolfram.c profile_wolfram.c profile.c -o $@

test: $(TESTS) test_profile_wolfram
	@for t in $(TESTS) test_profile_wolfram; do ./$$t || exit 1; done

clean:
	rm -f *.o $(TESTS) test_profile_wolfram test_profile_live test_publish_live test_image_source_wolfram test_timeline_live test_notifications_live test_server test_conversations test_image_cdn test_auth platinum-bridge

.PHONY: all test clean

# Live tests: the Wolfram fetch and publish run against Wolfram's library and a mock
# PDS on localhost (no network). They need Wolfram built as a host library first:
#   cmake -S $(WOLFRAM_ROOT) -B $(WOLFRAM_HOST_BUILD) -DWOLFRAM_BUILD_TESTS=OFF -DWOLFRAM_BUILD_EXAMPLES=OFF
#   cmake --build $(WOLFRAM_HOST_BUILD) --target wolfram
LIVE_CFLAGS = -std=c2x -pedantic -Wall -Wextra -Werror -Wshadow -I$(WOLFRAM_ROOT)/include -I$(WOLFRAM_ROOT)/test -I$(CJSON_INCLUDE) -I$(MHD_INCLUDE) -I.
LIVE_LIBS = -L$(WOLFRAM_HOST_BUILD) -L$(CJSON_PREFIX)/lib -lwolfram -lcjson -lmicrohttpd -Wl,-rpath,$(WOLFRAM_HOST_BUILD) -Wl,-rpath,$(CJSON_PREFIX)/lib

mock_pds.o:
	$(CC) -std=c2x -I$(WOLFRAM_ROOT)/include -I$(WOLFRAM_ROOT)/test -I$(CJSON_INCLUDE) -I$(MHD_INCLUDE) -c $(WOLFRAM_ROOT)/test/mock_pds.c -o $@

test_profile_live: test_profile_live.c profile_live.c profile_route.c profile_wolfram.c profile.c validate.c mock_pds.o
	$(CC) $(LIVE_CFLAGS) test_profile_live.c profile_live.c profile_route.c profile_wolfram.c profile.c validate.c mock_pds.o $(LIVE_LIBS) -o $@

test_publish_live: test_publish_live.c publish_live.c post_routes.c profile.c validate.c mock_pds.o
	$(CC) $(LIVE_CFLAGS) test_publish_live.c publish_live.c post_routes.c profile.c validate.c mock_pds.o $(LIVE_LIBS) -o $@

test_image_source_wolfram: test_image_source_wolfram.c image_source_wolfram.c image_route.c plim.o
	$(CC) $(LIVE_CFLAGS) test_image_source_wolfram.c image_source_wolfram.c $(LIVE_LIBS) -o $@

test_timeline_live: test_timeline_live.c timeline_wolfram.c timeline_route.c search_routes.c validate.c profile.c image_route.c ../plim/plim.c mock_pds.o
	$(CC) $(LIVE_CFLAGS) test_timeline_live.c timeline_wolfram.c timeline_route.c search_routes.c validate.c profile.c image_route.c ../plim/plim.c mock_pds.o $(LIVE_LIBS) -o $@

test_notifications_live: test_notifications_live.c notifications_wolfram.c notifications_route.c account_routes.c search_routes.c validate.c profile.c image_route.c ../plim/plim.c mock_pds.o
	$(CC) $(LIVE_CFLAGS) test_notifications_live.c notifications_wolfram.c notifications_route.c account_routes.c search_routes.c validate.c profile.c image_route.c ../plim/plim.c mock_pds.o $(LIVE_LIBS) -o $@

SERVER_SRCS = server.c server_http.c profile.c profile_route.c profile_live.c profile_wolfram.c validate.c post_routes.c publish_live.c routes.c ../plim/plim.c image_route.c image_cdn.c image_source_wolfram.c account_routes.c collection_routes.c collection_live.c named_routes.c named_live.c thread_route.c thread_live.c content_routes.c search_routes.c search_live.c write_routes.c timeline_route.c timeline_wolfram.c conversations.c conversations_wolfram.c notifications_route.c notifications_wolfram.c auth.c token.c sha256.c fsstore.c policy.c token_persist.c

test_server: test_server.c $(SERVER_SRCS) mock_pds.o
	$(CC) $(LIVE_CFLAGS) test_server.c $(SERVER_SRCS) mock_pds.o $(LIVE_LIBS) -o $@

# Route-level conversation tests: no network, no Wolfram call, just the status
# codes and the JSON shape the Mac parses.
test_conversations: test_conversations.c conversations.c search_routes.c validate.c profile.c
	$(CC) $(LIVE_CFLAGS) test_conversations.c conversations.c search_routes.c validate.c profile.c $(LIVE_LIBS) -o $@

AUTH_SRCS = auth.c token.c sha256.c fsstore.c policy.c token_persist.c routes.c

test_auth: test_auth.c $(AUTH_SRCS)
	$(CC) $(LIVE_CFLAGS) test_auth.c $(AUTH_SRCS) $(LIVE_LIBS) -o $@

platinum-bridge: main.c config.c $(SERVER_SRCS)
	$(CC) $(LIVE_CFLAGS) main.c config.c $(SERVER_SRCS) $(LIVE_LIBS) -o $@

test_image_cdn: test_image_cdn.c image_cdn.c image_route.c image_source_wolfram.c ../plim/plim.c
	$(CC) $(LIVE_CFLAGS) test_image_cdn.c image_cdn.c image_route.c image_source_wolfram.c ../plim/plim.c $(LIVE_LIBS) -o $@

# test_image_cdn makes a throwaway test certificate with openssl, in TMPDIR.
live: test_profile_live test_publish_live test_image_source_wolfram test_timeline_live test_notifications_live test_server test_conversations test_image_cdn test_auth
	./test_profile_live
	./test_publish_live
	./test_image_source_wolfram
	./test_timeline_live
	./test_notifications_live
	./test_server
	./test_conversations
	./test_image_cdn
	./test_auth

.PHONY: live
