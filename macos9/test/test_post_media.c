/*
 * test_post_media.c -- host-side tests for reading the avatar, pictures and
 * link card the bridge puts in a post. A logic and dialect check on a modern
 * compiler, not Classic Mac OS 9 hardware validation.
 */

#include <stdio.h>
#include <string.h>

#include "post_media.h"

static int failures;
static int checks;

static void check(int condition, const char *what)
{
    checks++;
    if (!condition) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

/* Parse `body`, a post object whose author is its "author" member. */
static void parse(const char *body, platinum_post_media *media)
{
    platinum_json doc;
    platinum_json author;

    check(platinum_json_open(&doc, body) == WF_OK, "the fixture is JSON");
    check(platinum_json_member(doc, "author", &author) == WF_OK,
          "the fixture has an author");
    platinum_post_media_parse(media, author, doc);
}

int main(void)
{
    platinum_post_media m;
    char body[2048];
    int i;

    parse("{\"author\":{\"did\":\"d\"},\"text\":\"hi\"}", &m);
    check(m.avatar[0] == '\0' && m.image_count == 0 &&
              m.card_title[0] == '\0' && m.card_domain[0] == '\0',
          "a plain post has no media");

    parse("{\"author\":{\"did\":\"d\",\"avatar\":"
          "\"avatar_thumbnail/plain/did:plc:a/bafk@jpeg\"}}",
          &m);
    check(strcmp(m.avatar, "avatar_thumbnail/plain/did:plc:a/bafk@jpeg") == 0,
          "the avatar reference is kept");

    parse("{\"author\":{\"did\":\"d\"},\"images\":["
          "{\"ref\":\"feed_thumbnail/plain/did:plc:a/b1@jpeg\",\"alt\":\"A heron\"},"
          "{\"ref\":\"feed_thumbnail/plain/did:plc:a/b2@png\",\"alt\":\"\"}]}",
          &m);
    check(m.image_count == 2, "two images");
    check(strcmp(m.images[0].ref, "feed_thumbnail/plain/did:plc:a/b1@jpeg") == 0,
          "the first reference");
    check(strcmp(m.images[0].alt, "A heron") == 0, "the first alt text");
    check(m.images[1].alt[0] == '\0', "an image with no description");

    /* More than four are cut at four, an image with no reference is skipped. */
    strcpy(body, "{\"author\":{\"did\":\"d\"},\"images\":[{\"alt\":\"no ref\"}");
    for (i = 0; i < 6; ++i) {
        char item[128];

        sprintf(item, ",{\"ref\":\"feed_thumbnail/plain/did:plc:a/b%d@jpeg\",\"alt\":\"%d\"}", i, i);
        strcat(body, item);
    }
    strcat(body, "]}");
    parse(body, &m);
    check(m.image_count == PLATINUM_MEDIA_IMAGES_MAX, "at most four images");
    check(strcmp(m.images[0].alt, "0") == 0, "the reference-less one was skipped");

    /* A reference that does not fit is refused, never cut short. */
    strcpy(body, "{\"author\":{\"did\":\"d\"},\"images\":[{\"ref\":\"");
    for (i = 0; i < PLATINUM_MEDIA_REF_MAX + 10; ++i)
        strcat(body, "x");
    strcat(body, "\",\"alt\":\"too long ref\"}]}");
    parse(body, &m);
    check(m.image_count == 0, "an oversize reference is dropped");

    /* Alt text is clipped to the buffer, and converted to MacRoman. */
    strcpy(body, "{\"author\":{\"did\":\"d\"},\"images\":[{\"ref\":\"r\",\"alt\":\"caf\\u00e9 ");
    for (i = 0; i < 400; ++i)
        strcat(body, "a");
    strcat(body, "\"}]}");
    parse(body, &m);
    check(m.image_count == 1, "one image with long alt text");
    check(strlen(m.images[0].alt) < PLATINUM_MEDIA_ALT_MAX, "alt text fits");
    check((unsigned char)m.images[0].alt[3] == 0x8E, "e acute is MacRoman 0x8E");

    parse("{\"author\":{\"did\":\"d\"},\"card\":{\"uri\":\"https://e.com/a\","
          "\"title\":\"An article\",\"domain\":\"e.com\"}}",
          &m);
    check(strcmp(m.card_title, "An article") == 0, "card title");
    check(strcmp(m.card_domain, "e.com") == 0, "card host");

    {
        platinum_json none;

        none.text = NULL;
        none.at = NULL;
        platinum_post_media_parse(NULL, none, none);
    }

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
