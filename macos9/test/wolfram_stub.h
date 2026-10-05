/*
 * wolfram_stub.h -- stand-ins for the Wolfram transport entry points the Mac
 * client calls, shared by the host-side tests. They record what the client
 * asked for and hand back a scripted response. Not Open Transport, not macTLS.
 */
#ifndef PLATINUM_TEST_WOLFRAM_STUB_H
#define PLATINUM_TEST_WOLFRAM_STUB_H

#include <stddef.h>

extern char last_url[512];
extern char last_body[512];
extern char last_auth[256];
extern int auth_set;
extern size_t last_max_response_bytes;
extern int last_method;
extern int fake_status;
extern long fake_http_status;
extern const char *fake_body;

#endif
