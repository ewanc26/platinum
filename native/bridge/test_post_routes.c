#include "post_routes.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

typedef struct fake {
    int calls;
    int mode;            /* 0 ok, 1 not found, 2 error */
    int saw_reply;
    int saw_quote;
    const char *saw_gate;
} fake;

static platinum_post_result
fake_publish(void *ctx, const char *text, const platinum_ref *reply, const platinum_ref *quote,
             const char *gate, char *body, size_t cap)
{
    fake *f = (fake *) ctx;
    (void) text;
    f->calls++;
    f->saw_reply = reply != NULL;
    f->saw_quote = quote != NULL;
    f->saw_gate = gate;
    if (f->mode == 1) return PLATINUM_POST_NOT_FOUND;
    if (f->mode == 2) return PLATINUM_POST_ERROR;
    if (cap > 16) strcpy(body, "{\"uri\":\"x\"}");
    return PLATINUM_POST_OK;
}

static const char *good_uri = "at://did:plc:abc/app.bsky.feed.post/3k";

static void
test_utf16(void)
{
    /* BMP characters are one unit each; characters outside it are two (a surrogate pair). */
    CHECK(platinum_utf16_length("abc") == 3);
    CHECK(platinum_utf16_length("\xc3\xa9") == 1);                 /* U+00E9 */
    CHECK(platinum_utf16_length("\xe2\x82\xac") == 1);             /* U+20AC */
    CHECK(platinum_utf16_length("\xf0\x9f\x98\x80") == 2);         /* U+1F600 */
    CHECK(platinum_utf16_length(NULL) == -1);
}

static void
test_text(void)
{
    fake f;
    char body[256];
    platinum_post_request req;
    int st;
    char big[400];
    char emoji[1200];
    char *p;
    int i;

    memset(&req, 0, sizeof req);

    /* Missing text: absent, not a string, and only spaces all count as missing. */
    memset(&f, 0, sizeof f);
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 400 && f.calls == 0);
    CHECK(strstr(body, "\"error\":\"missing_text\"") != NULL);
    req.text = "   \t\n";
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 400 && f.calls == 0);
    /* Unicode whitespace that trim() removes also counts as missing. */
    req.text = "\xc2\xa0\xe3\x80\x80\xef\xbb\xbf";   /* NBSP, ideographic space, BOM */
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 400 && f.calls == 0);
    CHECK(strstr(body, "\"error\":\"missing_text\"") != NULL);
    req.text = "\xc2\xa0x";   /* NBSP then a letter is not blank */
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 200);

    /* 300 ASCII characters is the limit; 301 is refused. */
    memset(big, 'a', 300);
    big[300] = '\0';
    req.text = big;
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 200);
    big[300] = 'a';
    big[301] = '\0';
    req.text = big;
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 400);
    CHECK(strstr(body, "\"error\":\"text_too_long\"") != NULL);

    /* The limit counts UTF-16 units: 150 emoji are 300 units (accepted), 151 are 302 (refused). */
    p = emoji;
    for (i = 0; i < 150; i++) { *p++ = (char)0xF0; *p++ = (char)0x9F; *p++ = (char)0x98; *p++ = (char)0x80; }
    *p = '\0';
    req.text = emoji;
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 200);
    p = emoji;
    for (i = 0; i < 151; i++) { *p++ = (char)0xF0; *p++ = (char)0x9F; *p++ = (char)0x98; *p++ = (char)0x80; }
    *p = '\0';
    req.text = emoji;
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 400);
}

static void
test_refs_and_gate(void)
{
    fake f;
    char body[256];
    platinum_post_request req;
    int st;

    /* A reply with a valid reference and the default gate. */
    memset(&req, 0, sizeof req);
    memset(&f, 0, sizeof f);
    req.text = "hi";
    req.has_reply = 1;
    req.reply_is_object = 1;
    req.reply.uri = good_uri;
    req.reply.cid = "bafyabc";
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 200 && f.saw_reply && !f.saw_quote);

    /* A reply may only take the gate "everyone". */
    req.has_gate = 1;
    req.gate = "following";
    memset(&f, 0, sizeof f);
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 400 && f.calls == 0);
    CHECK(strstr(body, "\"error\":\"invalid_reply_gate\"") != NULL);
    req.gate = "everyone";
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 200 && strcmp(f.saw_gate, "everyone") == 0);

    /* A top-level post accepts each of the five gates, and rejects anything else. */
    memset(&req, 0, sizeof req);
    req.text = "hi";
    req.has_gate = 1;
    req.gate = "nobody";
    memset(&f, 0, sizeof f);
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 200 && strcmp(f.saw_gate, "nobody") == 0);
    req.gate = "anyone";
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 400);
    req.gate = NULL;     /* present but not a string */
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 400);

    /* A reply reference that is not an object, or is malformed, is refused. */
    memset(&req, 0, sizeof req);
    req.text = "hi";
    req.has_reply = 1;
    req.reply_is_object = 0;
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 400);
    CHECK(strstr(body, "replyTo must name a post by uri and cid.") != NULL);
    req.reply_is_object = 1;
    req.reply.uri = "https://example.com/p";
    req.reply.cid = "bafyabc";
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 400);
    req.reply.uri = good_uri;
    req.reply.cid = NULL;
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 400);

    /* A quote is checked the same way and is passed through. */
    memset(&req, 0, sizeof req);
    req.text = "look";
    req.has_quote = 1;
    req.quote_is_object = 1;
    req.quote.uri = good_uri;
    req.quote.cid = "bafyabc";
    memset(&f, 0, sizeof f);
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 200 && f.saw_quote && !f.saw_reply);
    req.quote.uri = "nope";
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 400);
    CHECK(strstr(body, "quote must name a post by uri and cid.") != NULL);
}

static void
test_outcomes(void)
{
    fake f;
    char body[256];
    platinum_post_request req;
    int st;

    memset(&req, 0, sizeof req);
    req.text = "hi";

    memset(&f, 0, sizeof f);
    f.mode = 1;
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 404);
    CHECK(strcmp(body, "{\"error\":\"post_not_found\",\"message\":\"The post being replied to or quoted no longer exists.\"}") == 0);

    memset(&f, 0, sizeof f);
    f.mode = 2;
    st = platinum_route_post(&f, fake_publish, &req, body, sizeof body);
    CHECK(st == 500);

    /* An error body that does not fit is the generic 500, with nothing in it. */
    memset(&f, 0, sizeof f);
    f.mode = 1;
    st = platinum_route_post(&f, fake_publish, &req, body, 20);
    CHECK(st == 500 && body[0] == '\0');
}

int
main(void)
{
    test_utf16();
    test_text();
    test_refs_and_gate();
    test_outcomes();
    if (failures) {
        fprintf(stderr, "test_post_routes: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_post_routes: %d checks passed\n", checks);
    return 0;
}
