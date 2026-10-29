#include "timeline_wolfram.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <wolfram/actor_prefs_typed.h>
#include <wolfram/agent.h>
#include <wolfram/feed_typed.h>
#include <wolfram/muted_words.h>
#include <wolfram/post_display.h>

#include "image_route.h"

/* Copies src into dst, cutting to fit cap bytes on a UTF-8 boundary. The native
 * model has fixed buffers, so the Node bridge's code-point limits are applied as
 * the byte prefix that fits; a cut never splits a code point. */
static void
clip_bytes(const char *src, char *dst, size_t cap)
{
    size_t n;

    if (dst == NULL || cap == 0) return;
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    n = strlen(src);
    if (n >= cap) {
        n = cap - 1;
        while (n > 0 && ((unsigned char) src[n] & 0xC0) == 0x80) n--;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/* The hostname of an http(s) URL, as the card's domain; 0 on a URL the bridge
 * will not show. Userinfo and a port are dropped, and a URL with no host is
 * refused. */
static int
card_domain(const char *uri, char *out, size_t cap)
{
    const char *p;
    const char *end;
    const char *at;
    size_t n;

    if (uri == NULL || out == NULL || cap == 0) return -1;
    if (strncmp(uri, "https://", 8) == 0) {
        p = uri + 8;
    } else if (strncmp(uri, "http://", 7) == 0) {
        p = uri + 7;
    } else {
        return -1;
    }
    end = p + strcspn(p, "/?#");
    at = memchr(p, '@', (size_t) (end - p));
    if (at != NULL) p = at + 1;
    if (p >= end) return -1;
    n = (size_t) (end - p);
    {
        const char *colon = memchr(p, ':', n);
        if (colon != NULL) n = (size_t) (colon - p);
    }
    if (n == 0 || n >= cap) return -1;
    memcpy(out, p, n);
    out[n] = '\0';
    return 0;
}

/* Best effort: a preferences failure leaves the list empty, so the page is shown
 * unfiltered rather than not at all. */
static void
load_muted_words(wf_agent *agent, wf_muted_list *list)
{
    wf_actor_preferences prefs;
    wf_status st;

    memset(list, 0, sizeof *list);
    memset(&prefs, 0, sizeof prefs);
    st = wf_agent_get_actor_prefs_typed(agent, &prefs);
    if (st != WF_OK) return;
    wf_muted_list_from_prefs(list, &prefs, (int64_t) time(NULL));
    wf_actor_preferences_free(&prefs);
}

/* True when a muted word hides this post. The tags are the post's validated
 * tag facets, which is what the AppView's own matcher reads. */
static int
post_is_muted(const wf_muted_list *muted, const wf_post_display *display)
{
    const char *tags[16];
    size_t tag_count = 0;
    size_t i;

    if (muted->count == 0) return 0;
    for (i = 0; i < display->facet_count && tag_count < sizeof tags / sizeof tags[0]; i++) {
        if (display->facets[i].kind == WF_FACET_TAG && display->facets[i].target != NULL) {
            tags[tag_count++] = display->facets[i].target;
        }
    }
    return wf_muted_list_match(muted, display->text, tags, tag_count, 0) ? 1 : 0;
}

static void
fill_post(platinum_timeline_post *post, const wf_agent_post_view *view,
          const wf_post_display *display)
{
    size_t i;
    size_t image_count = 0;

    memset(post, 0, sizeof *post);

    clip_bytes(view->uri, post->uri, sizeof post->uri);
    clip_bytes(view->cid, post->cid, sizeof post->cid);
    clip_bytes(view->author.did, post->author.did, sizeof post->author.did);
    clip_bytes(view->author.handle, post->author.handle, sizeof post->author.handle);
    clip_bytes(view->author.display_name, post->author.display_name,
               sizeof post->author.display_name);
    platinum_avatar_ref(view->author.avatar, post->author.avatar,
                        sizeof post->author.avatar);

    clip_bytes(display->text, post->text, sizeof post->text);
    clip_bytes(display->created_at, post->created_at, sizeof post->created_at);

    post->like_count = view->like_count;
    post->repost_count = view->repost_count;
    post->reply_count = view->reply_count;
    post->quote_count = view->quote_count;
    post->liked = view->viewer.like != NULL ? 1 : 0;
    post->reposted = view->viewer.repost != NULL ? 1 : 0;

    for (i = 0; i < display->image_count && image_count < PLATINUM_TIMELINE_IMAGES_MAX; i++) {
        platinum_timeline_image *image = &post->images[image_count];

        if (platinum_cdn_ref(display->images[i].thumb, image->ref,
                             sizeof image->ref) != 0) {
            continue;
        }
        clip_bytes(display->images[i].alt, image->alt, sizeof image->alt);
        image_count++;
    }
    post->has_images = image_count > 0 ? 1 : 0;

    if (display->external_uri != NULL && display->external_uri[0] != '\0' &&
        card_domain(display->external_uri, post->card.domain,
                    sizeof post->card.domain) == 0) {
        clip_bytes(display->external_uri, post->card.uri, sizeof post->card.uri);
        clip_bytes(display->external_title, post->card.title, sizeof post->card.title);
        post->has_card = 1;
    }
}

int
platinum_fill_post_live(platinum_timeline_post *post, const wf_agent_post_view *view)
{
    wf_post_display display;

    if (post == NULL || view == NULL) return -1;
    memset(&display, 0, sizeof display);
    if (wf_agent_post_view_display(view, &display) != WF_OK) {
        wf_post_display_free(&display);
        return -1;
    }
    fill_post(post, view, &display);
    wf_post_display_free(&display);
    return 0;
}

/* Fills `out` from a parsed feed page, applying muted words. The caller frees
 * the page; a post the display model cannot read is skipped, not fatal. */
static platinum_timeline_result
fill_from_feed(wf_agent *agent, const wf_agent_feed_list *page, platinum_timeline *out)
{
    wf_muted_list muted;
    size_t i;

    if (page->item_count > 0) {
        size_t count = page->item_count < PLATINUM_TIMELINE_POSTS_MAX
                          ? page->item_count
                          : PLATINUM_TIMELINE_POSTS_MAX;

        out->posts = calloc(count, sizeof *out->posts);
        if (out->posts == NULL) return PLATINUM_TIMELINE_ERROR;

        load_muted_words(agent, &muted);
        for (i = 0; i < count; i++) {
            wf_post_display display;

            memset(&display, 0, sizeof display);
            if (wf_agent_post_view_display(&page->items[i].post, &display) != WF_OK) {
                wf_post_display_free(&display);
                continue;
            }
            if (!post_is_muted(&muted, &display)) {
                fill_post(&out->posts[out->post_count], &page->items[i].post, &display);
                out->post_count++;
            }
            wf_post_display_free(&display);
        }
    }

    if (page->cursor != NULL && page->cursor[0] != '\0' &&
        strlen(page->cursor) < sizeof out->cursor) {
        strcpy(out->cursor, page->cursor);
        out->has_cursor = 1;
    }
    return PLATINUM_TIMELINE_OK;
}

platinum_timeline_result
platinum_timeline_fetch_live(void *ctx, int limit, const char *cursor,
                             platinum_timeline *out)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_agent_feed_list page;
    platinum_timeline_result result;

    if (agent == NULL || out == NULL) return PLATINUM_TIMELINE_ERROR;
    memset(&page, 0, sizeof page);

    if (wf_agent_get_timeline_typed(agent, limit, cursor, &page) != WF_OK) {
        wf_agent_feed_list_free(&page);
        return PLATINUM_TIMELINE_ERROR;
    }

    result = fill_from_feed(agent, &page, out);
    wf_agent_feed_list_free(&page);
    return result;
}

platinum_timeline_result
platinum_author_feed_fetch_live(void *ctx, const char *actor, int limit,
                                const char *cursor, platinum_timeline *out)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_agent_feed_list page;
    platinum_timeline_result result;

    if (agent == NULL || actor == NULL || out == NULL) return PLATINUM_TIMELINE_ERROR;
    memset(&page, 0, sizeof page);

    if (wf_agent_get_author_feed_typed(agent, actor, limit, cursor, NULL, &page) != WF_OK) {
        wf_agent_feed_list_free(&page);
        return PLATINUM_TIMELINE_ERROR;
    }

    result = fill_from_feed(agent, &page, out);
    wf_agent_feed_list_free(&page);
    return result;
}
