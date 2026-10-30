/*
 * Post creation: POST /v1/post (Part 8, post and reply slice).
 *
 * The checks of server.ts, in the same order: text present, text within 300
 * UTF-16 code units (JavaScript's String.length), replyTo and quote as post
 * references, and replyGate one of the five values and only "everyone" on a
 * reply. The publish itself is a callback.
 */
#ifndef PLATINUM_NATIVE_POST_ROUTES_H
#define PLATINUM_NATIVE_POST_ROUTES_H

#include <stddef.h>

#define PLATINUM_POST_TEXT_MAX 300

/* Length in UTF-16 code units, as JavaScript's String.length counts. -1 for NULL. */
long platinum_utf16_length(const char *s);

typedef struct platinum_ref {
    const char *uri;   /* NULL when absent */
    const char *cid;
} platinum_ref;

typedef enum platinum_post_result {
    PLATINUM_POST_OK = 0,
    PLATINUM_POST_NOT_FOUND,
    PLATINUM_POST_ERROR
} platinum_post_result;

/* Publishes the post and writes its JSON body. reply and quote are NULL when absent.
 * gate is NULL when absent, otherwise the gate's name. */
typedef platinum_post_result (*platinum_publish_fn)(void *ctx, const char *text,
                                                    const platinum_ref *reply,
                                                    const platinum_ref *quote,
                                                    const char *gate,
                                                    char *body, size_t cap);

/* The request, already parsed by the server layer. text is NULL when absent or not a
 * string. has_reply and has_quote say whether the field was present. replyGate_valid
 * is 0 when the field was present but not a string. */
typedef struct platinum_post_request {
    const char *text;
    int has_reply;
    platinum_ref reply;      /* uri/cid may be NULL if the object lacked them */
    int reply_is_object;
    int has_quote;
    platinum_ref quote;
    int quote_is_object;
    int has_gate;
    const char *gate;        /* NULL when the field was not a string */
} platinum_post_request;

int platinum_route_post(void *ctx, platinum_publish_fn fn, const platinum_post_request *req,
                        char *body, size_t cap);

#endif
