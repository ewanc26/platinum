/* The timeline route: limit and cursor rules, page serialization, and failures. */
#include "timeline_route.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

enum { FAKE_PAGE = 0, FAKE_FAIL, FAKE_EMPTY, FAKE_HUGE };

typedef struct fake {
    int mode;
    int calls;
    int last_limit;
    int had_cursor;
    char last_cursor[256];
} fake;

static void
fill_author(platinum_timeline_author *a, const char *did)
{
    memset(a, 0, sizeof *a);
    strcpy(a->did, did);
}

static platinum_timeline_result
fake_fetch(void *ctx, int limit, const char *cursor, platinum_timeline *out)
{
    fake *f = (fake *) ctx;
    platinum_timeline_post *post;

    f->calls++;
    f->last_limit = limit;
    f->had_cursor = cursor != NULL;
    if (cursor != NULL) strcpy(f->last_cursor, cursor);

    if (f->mode == FAKE_FAIL) return PLATINUM_TIMELINE_ERROR;
    if (f->mode == FAKE_EMPTY) return PLATINUM_TIMELINE_OK;
    if (f->mode == FAKE_HUGE) {
        out->post_count = PLATINUM_TIMELINE_POSTS_MAX + 1;
        out->posts = calloc(out->post_count, sizeof *out->posts);
        return out->posts != NULL ? PLATINUM_TIMELINE_OK : PLATINUM_TIMELINE_ERROR;
    }

    out->posts = calloc(2, sizeof *out->posts);
    if (out->posts == NULL) return PLATINUM_TIMELINE_ERROR;
    out->post_count = 2;

    post = &out->posts[0];
    strcpy(post->uri, "at://did:plc:abc/app.bsky.feed.post/3k1");
    strcpy(post->cid, "bafyone");
    fill_author(&post->author, "did:plc:abc");
    strcpy(post->author.handle, "ewan.test");
    strcpy(post->author.display_name, "Ewan");
    strcpy(post->author.avatar, "avatar_thumbnail/plain/did:plc:abc/bafyav@jpeg");
    strcpy(post->text, "he said \"hi\"\nand \\ done");
    strcpy(post->created_at, "2026-10-01T10:00:00.000Z");
    post->like_count = 7;
    post->repost_count = 3;
    post->reply_count = 1;
    post->quote_count = 2;
    post->liked = 1;
    post->reposted = 0;
    post->has_images = 1;
    strcpy(post->images[0].ref, "feed_thumbnail/plain/did:plc:abc/bafyimg@jpeg");
    strcpy(post->images[0].alt, "a cat");
    post->has_card = 1;
    strcpy(post->card.uri, "https://example.com/story");
    strcpy(post->card.title, "A story");
    strcpy(post->card.domain, "example.com");

    post = &out->posts[1];
    strcpy(post->uri, "at://did:plc:bob/app.bsky.feed.post/3k2");
    strcpy(post->cid, "bafytwo");
    fill_author(&post->author, "did:plc:bob");
    strcpy(post->text, "second");
    strcpy(post->created_at, "2026-10-01T09:00:00.000Z");

    strcpy(out->cursor, "next-page");
    out->has_cursor = 1;
    return PLATINUM_TIMELINE_OK;
}

static void
test_page(void)
{
    fake f;
    char body[4096];
    int st;

    memset(&f, 0, sizeof f);
    st = platinum_route_timeline(&f, fake_fetch, NULL, NULL, body, sizeof body);
    CHECK(st == 200);
    CHECK(f.calls == 1);
    CHECK(f.last_limit == 20);
    CHECK(!f.had_cursor);
    CHECK(strncmp(body, "{\"posts\":[", 10) == 0);
    CHECK(strstr(body, "\"uri\":\"at://did:plc:abc/app.bsky.feed.post/3k1\"") != NULL);
    CHECK(strstr(body, "\"text\":\"he said \\\"hi\\\"\\nand \\\\ done\"") != NULL);
    CHECK(strstr(body, "\"displayName\":\"Ewan\"") != NULL);
    CHECK(strstr(body, "\"avatar\":\"avatar_thumbnail/plain/did:plc:abc/bafyav@jpeg\"") != NULL);
    CHECK(strstr(body, "\"likeCount\":7") != NULL);
    CHECK(strstr(body, "\"quoteCount\":2") != NULL);
    CHECK(strstr(body, "\"liked\":true") != NULL);
    CHECK(strstr(body, "\"reposted\":false") != NULL);
    CHECK(strstr(body, "\"images\":[{\"ref\":\"feed_thumbnail/plain/did:plc:abc/bafyimg@jpeg\",\"alt\":\"a cat\"}]") != NULL);
    CHECK(strstr(body, "\"card\":{\"uri\":\"https://example.com/story\",\"title\":\"A story\",\"domain\":\"example.com\"}") != NULL);
    CHECK(strstr(body, "\"cursor\":\"next-page\"") != NULL);
    CHECK(body[strlen(body) - 1] == '}');

    /* The second post has no optional author fields, no images and no card, and
     * they are left out rather than sent empty. */
    CHECK(strstr(body, "{\"did\":\"did:plc:bob\"}") != NULL);
    CHECK(strstr(body, "\"text\":\"second\"") != NULL);

    /* limit and cursor are the caller's, passed through. */
    memset(&f, 0, sizeof f);
    st = platinum_route_timeline(&f, fake_fetch, "5", "abc", body, sizeof body);
    CHECK(st == 200);
    CHECK(f.last_limit == 5);
    CHECK(f.had_cursor && strcmp(f.last_cursor, "abc") == 0);

    memset(&f, 0, sizeof f);
    st = platinum_route_timeline(&f, fake_fetch, "500", NULL, body, sizeof body);
    CHECK(st == 200 && f.last_limit == 50);

    /* An empty page is a posts array and nothing else. */
    memset(&f, 0, sizeof f);
    f.mode = FAKE_EMPTY;
    st = platinum_route_timeline(&f, fake_fetch, NULL, NULL, body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"posts\":[]}") == 0);
}

static void
test_errors(void)
{
    fake f;
    char body[4096];
    char cursor[300];
    int st;

    memset(&f, 0, sizeof f);
    f.mode = FAKE_FAIL;
    st = platinum_route_timeline(&f, fake_fetch, NULL, NULL, body, sizeof body);
    CHECK(st == 500);
    CHECK(strcmp(body, "{\"error\":\"bridge_error\",\"message\":\"The backend could not complete the request.\"}") == 0);

    /* A cursor that cannot be echoed back is refused before the fetch. */
    memset(&f, 0, sizeof f);
    memset(cursor, 'a', sizeof cursor - 1);
    cursor[sizeof cursor - 1] = '\0';
    st = platinum_route_timeline(&f, fake_fetch, NULL, cursor, body, sizeof body);
    CHECK(st == 400);
    CHECK(f.calls == 0);
    CHECK(strstr(body, "invalid_cursor") != NULL);

    /* A page too large for the response buffer is a 500, not a cut body. */
    memset(&f, 0, sizeof f);
    st = platinum_route_timeline(&f, fake_fetch, NULL, NULL, body, 256);
    CHECK(st == 500);
    CHECK(strstr(body, "bridge_error") != NULL);

    /* A fetch that fills more than one page can hold is refused. */
    memset(&f, 0, sizeof f);
    f.mode = FAKE_HUGE;
    st = platinum_route_timeline(&f, fake_fetch, NULL, NULL, body, sizeof body);
    CHECK(st == 500);

    memset(&f, 0, sizeof f);
    CHECK(platinum_route_timeline(&f, NULL, NULL, NULL, body, sizeof body) == 500);
    CHECK(platinum_route_timeline(&f, fake_fetch, NULL, NULL, NULL, 0) == 500);
    CHECK(f.calls == 0);
}

static void
test_free(void)
{
    platinum_timeline page;
    fake f;

    memset(&page, 0, sizeof page);
    memset(&f, 0, sizeof f);
    CHECK(fake_fetch(&f, 20, NULL, &page) == PLATINUM_TIMELINE_OK);
    CHECK(page.posts != NULL && page.post_count == 2);
    platinum_timeline_free(&page);
    CHECK(page.posts == NULL && page.post_count == 0);
    platinum_timeline_free(&page);
    platinum_timeline_free(NULL);
}

int
main(void)
{
    test_page();
    test_errors();
    test_free();
    if (failures) {
        fprintf(stderr, "test_timeline_route: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_timeline_route: %d checks passed\n", checks);
    return 0;
}
