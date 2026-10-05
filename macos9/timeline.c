#include "timeline.h"
#include "text_codec.h"

#include "json_min.h"
#include <string.h>

/*
 * Copy a string member of `object` into `destination` as MacRoman.
 *
 * Returns 0 when the member is absent, is not a string, or does not fit, so
 * the caller's own fallback chain runs. `uri` and `cid` use the strict
 * accessor, because a post without them cannot be shown at all; the fields that
 * are only displayed use the truncating one and never fail on length.
 */
static int timeline_copy_field(char *destination,
                               long capacity,
                               platinum_json object,
                               const char *name,
                               int clipping)
{
    char utf8[PLATINUM_TEXT_UTF8_CAPACITY];
    long length;
    wf_status status;

    if (destination == NULL || capacity <= 0)
        return 0;

    destination[0] = '\0';
    status = clipping
        ? platinum_json_string_truncating(object, name, utf8, sizeof(utf8))
        : platinum_json_string(object, name, utf8, sizeof(utf8));
    if (status != WF_OK)
        return 0;

    length = platinum_text_utf8_to_macroman(utf8, destination, capacity, NULL);
    return length >= 0;
}

static long timeline_json_number(platinum_json object, const char *name)
{
    long value = 0;

    if (platinum_json_int(object, name, &value) != WF_OK)
        return 0;

    return value;
}

static void timeline_set_status(platinum_timeline *timeline,
                                const char *status)
{
    long length;

    if (timeline == NULL)
        return;

    timeline->status[0] = '\0';
    if (status == NULL)
        return;

    length = (long)strlen(status);
    if (length > PLATINUM_TIMELINE_STATUS_MAX)
        length = PLATINUM_TIMELINE_STATUS_MAX;

    memcpy(timeline->status, status, (size_t)length);
    timeline->status[length] = '\0';
}

static void timeline_copy_wrapped_text(platinum_post_preview *post,
                                       const char *text)
{
    long length;
    long offset;
    long remaining;
    long chunk;
    char *destination;

    if (post == NULL)
        return;

    post->line1[0] = '\0';
    post->line2[0] = '\0';
    post->line3[0] = '\0';

    if (text == NULL)
        return;

    length = (long)strlen(text);
    offset = 0;

    while (offset < length && offset < 336) {
        remaining = length - offset;
        chunk = remaining;
        if (chunk > PLATINUM_TIMELINE_LINE_MAX - 1)
            chunk = PLATINUM_TIMELINE_LINE_MAX - 1;

        if (offset < PLATINUM_TIMELINE_LINE_MAX - 1)
            destination = post->line1;
        else if (offset < (PLATINUM_TIMELINE_LINE_MAX - 1) * 2)
            destination = post->line2;
        else
            destination = post->line3;

        memcpy(destination, text + offset, (size_t)chunk);
        destination[chunk] = '\0';
        offset += chunk;
    }
}

static int timeline_parse_post(platinum_post_preview *post,
                               platinum_json item)
{
    platinum_json author;
    char did[128];
    char handle[128];
    char created_at[64];
    char text[PLATINUM_TEXT_UTF8_CAPACITY];
    char text_macroman[PLATINUM_TEXT_MAX_CODEPOINTS + 1];

    if (post == NULL)
        return 0;

    memset(post, 0, sizeof(*post));

    /* uri and cid identify the post, so a post without them is not shown at all
     * rather than shown with blanks. */
    if (!timeline_copy_field(post->uri, sizeof(post->uri), item, "uri", 0))
        return 0;
    if (!timeline_copy_field(post->cid, sizeof(post->cid), item, "cid", 0))
        return 0;

    if (platinum_json_member(item, "author", &author) != WF_OK)
        return 0;
    if (!timeline_copy_field(did, sizeof(did), author, "did", 0))
        return 0;

    /* Prefer the display name, then the handle, then the DID, so an account
     * with no display name still reads as something. */
    post->author[0] = '\0';
    (void)timeline_copy_field(post->author, sizeof(post->author), author,
                               "displayName", 1);
    if (post->author[0] == '\0')
        (void)timeline_copy_field(post->author, sizeof(post->author), author,
                                  "handle", 1);
    if (post->author[0] == '\0')
        (void)timeline_copy_field(post->author, sizeof(post->author), author,
                                  "did", 1);

    if (platinum_json_string_truncating(author, "handle", handle,
                                        sizeof(handle)) == WF_OK) {
        post->handle[0] = '@';
        (void)timeline_copy_field(post->handle + 1,
                                  sizeof(post->handle) - 1,
                                  author,
                                  "handle",
                                  1);
    } else {
        post->handle[0] = '\0';
    }

    /* The bridge sends RFC 3339, so the clock time is the five characters after
     * the date's "T". Anything shorter is not that shape and is shown as it
     * arrived rather than sliced out of the middle. */
    post->time[0] = '\0';
    if (platinum_json_string_truncating(item, "createdAt", created_at,
                                        sizeof(created_at)) == WF_OK) {
        if (strlen(created_at) >= 16) {
            memcpy(post->time, created_at + 11, 5);
            post->time[5] = '\0';
        } else {
            (void)timeline_copy_field(post->time, sizeof(post->time), item,
                                      "createdAt", 1);
        }
    }

    if (platinum_json_string_truncating(item, "text", text,
                                        sizeof(text)) == WF_OK) {
        if (platinum_text_utf8_to_macroman(text,
                                           text_macroman,
                                           sizeof(text_macroman),
                                           NULL) >= 0)
            timeline_copy_wrapped_text(post, text_macroman);
    }

    post->like_count = timeline_json_number(item, "likeCount");
    post->repost_count = timeline_json_number(item, "repostCount");
    post->reply_count = timeline_json_number(item, "replyCount");
    post->quote_count = timeline_json_number(item, "quoteCount");

    /* Absent or not a boolean reads as "no": the bridge never sends a record
     * URI, only these two flags. */
    if (platinum_json_bool(item, "liked", &post->liked) != WF_OK)
        post->liked = 0;
    if (platinum_json_bool(item, "reposted", &post->reposted) != WF_OK)
        post->reposted = 0;
    return 1;
}

void platinum_timeline_init(platinum_timeline *timeline)
{
    if (timeline == NULL)
        return;

    memset(timeline, 0, sizeof(*timeline));
    timeline_set_status(timeline, "Pair an account to load the timeline.");
}

/*
 * One request. With `append` clear it replaces the list; with it set it adds the
 * next page to the end, dropping from the front to stay within the cap, and
 * leaves the list untouched if anything fails.
 */
static wf_status timeline_fetch(platinum_timeline *timeline,
                                platinum_bridge_client *bridge,
                                int append,
                                unsigned short *dropped)
{
    wf_response response;
    platinum_json root;
    platinum_json posts;
    platinum_json item;
    long index;
    long available;
    long parsed;
    long drop;
    char path[64 + 3 * PLATINUM_TIMELINE_CURSOR_MAX];
    char cursor[PLATINUM_TIMELINE_CURSOR_MAX + 1];
    wf_status status;

    if (dropped != NULL)
        *dropped = 0;

    strcpy(path, "/v1/timeline?limit=20");
    if (append) {
        strcat(path, "&cursor=");
        if (platinum_bridge_query_escape(timeline->cursor,
                                         path + strlen(path),
                                         (long)(sizeof(path) - strlen(path)))
            < 0)
            return WF_ERR_INVALID_ARG;
    } else {
        timeline->count = 0;
        timeline->cursor[0] = '\0';
    }

    timeline->loading = 1;
    timeline_set_status(timeline, append ? "Loading older posts..."
                                         : "Loading timeline...");
    memset(&response, 0, sizeof(response));

    status = platinum_bridge_get(bridge, path, &response);
    if (status != WF_OK) {
        timeline->loading = 0;
        if (status == WF_ERR_AUTH)
            timeline_set_status(timeline,
                                "Session expired. Pair the account again.");
        else
            timeline_set_status(timeline, append
                                    ? "Could not load older posts."
                                    : "Timeline refresh failed.");
        wf_response_free(&response);
        return status;
    }

    /* posts has to be an array. A response that carries something else is not a
     * timeline, and silently treating an absent member as an empty feed would
     * show "the timeline is empty" for what is really a broken response. */
    if (platinum_json_open(&root, response.body != NULL ? response.body : "")
            != WF_OK ||
        platinum_json_member(root, "posts", &posts) != WF_OK ||
        platinum_json_count(posts, &available) != WF_OK) {
        timeline->loading = 0;
        timeline_set_status(timeline,
                            "The bridge returned an invalid timeline.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    parsed = (available > PLATINUM_TIMELINE_PAGE) ? PLATINUM_TIMELINE_PAGE
                                                  : available;

    /* The response is known good, so only now is it safe to drop rows. */
    drop = (long)timeline->count + parsed - PLATINUM_TIMELINE_MAX_POSTS;
    if (drop > (long)timeline->count)
        drop = (long)timeline->count;
    if (drop > 0) {
        memmove(&timeline->posts[0], &timeline->posts[drop],
                (size_t)((long)timeline->count - drop) *
                    sizeof(timeline->posts[0]));
        timeline->count = (unsigned short)((long)timeline->count - drop);
        if (dropped != NULL)
            *dropped = (unsigned short)drop;
    }

    for (index = 0; index < parsed &&
                    timeline->count < PLATINUM_TIMELINE_MAX_POSTS; ++index) {
        if (platinum_json_element(posts, index, &item) != WF_OK)
            continue;
        if (timeline_parse_post(&timeline->posts[timeline->count], item))
            ++timeline->count;
    }

    /* A cursor is an identifier: it is copied exactly or not at all. One that
     * does not fit ends paging rather than asking for the wrong page. */
    if (platinum_json_string(root, "cursor", cursor, sizeof(cursor)) == WF_OK)
        strcpy(timeline->cursor, cursor);
    else
        timeline->cursor[0] = '\0';

    /* Everything has been copied out of response.body by now. */
    wf_response_free(&response);

    timeline->loading = 0;
    if (timeline->count == 0)
        timeline_set_status(timeline, "The timeline is empty.");
    else
        timeline_set_status(timeline, NULL);

    return WF_OK;
}

wf_status platinum_timeline_refresh(platinum_timeline *timeline,
                                    platinum_bridge_client *bridge)
{
    if (timeline == NULL || bridge == NULL)
        return WF_ERR_INVALID_ARG;
    return timeline_fetch(timeline, bridge, 0, NULL);
}

wf_status platinum_timeline_load_older(platinum_timeline *timeline,
                                       platinum_bridge_client *bridge,
                                       unsigned short *dropped)
{
    if (dropped != NULL)
        *dropped = 0;
    if (timeline == NULL || bridge == NULL || !platinum_timeline_has_older(timeline))
        return WF_ERR_INVALID_ARG;
    return timeline_fetch(timeline, bridge, 1, dropped);
}

int platinum_timeline_has_older(const platinum_timeline *timeline)
{
    return timeline != NULL && timeline->cursor[0] != '\0' &&
           timeline->count > 0;
}

wf_status platinum_timeline_set_engagement(platinum_timeline *timeline,
                                           platinum_bridge_client *bridge,
                                           unsigned short index,
                                           int repost,
                                           int on)
{
    platinum_post_preview *post;
    char uri[sizeof(timeline->posts[0].uri) * 2];
    char cid[sizeof(timeline->posts[0].cid) * 2];
    char body[sizeof(uri) + sizeof(cid) + 48];
    wf_response response;
    platinum_json root;
    long count;
    int now_on;
    wf_status status;

    if (timeline == NULL || bridge == NULL || index >= timeline->count)
        return WF_ERR_INVALID_ARG;

    post = &timeline->posts[index];
    if (platinum_json_escape(uri, sizeof(uri), post->uri) != WF_OK ||
        platinum_json_escape(cid, sizeof(cid), post->cid) != WF_OK)
        return WF_ERR_INVALID_ARG;

    strcpy(body, "{\"uri\":\"");
    strcat(body, uri);
    strcat(body, "\",\"cid\":\"");
    strcat(body, cid);
    strcat(body, on ? "\",\"on\":true}" : "\",\"on\":false}");

    memset(&response, 0, sizeof(response));
    status = platinum_bridge_post(bridge, repost ? "/v1/repost" : "/v1/like",
                                  body, &response);
    if (status != WF_OK) {
        if (status == WF_ERR_AUTH)
            timeline_set_status(timeline,
                                "Session expired. Pair the account again.");
        else if (response.status == 404)
            timeline_set_status(timeline, "That post no longer exists.");
        else
            timeline_set_status(timeline, repost ? "The repost did not go through."
                                                 : "The like did not go through.");
        wf_response_free(&response);
        return status;
    }

    /* Both members are required: a reply missing either is not trusted to
     * change what the row says. */
    if (platinum_json_open(&root, response.body != NULL ? response.body : "")
            != WF_OK ||
        platinum_json_bool(root, "on", &now_on) != WF_OK ||
        platinum_json_int(root, "count", &count) != WF_OK || count < 0) {
        timeline_set_status(timeline, "The bridge returned an invalid reply.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }
    wf_response_free(&response);

    if (repost) {
        post->reposted = now_on;
        post->repost_count = count;
    } else {
        post->liked = now_on;
        post->like_count = count;
    }
    timeline_set_status(timeline, NULL);
    return WF_OK;
}

const platinum_post_preview *platinum_timeline_posts(
    const platinum_timeline *timeline)
{
    if (timeline == NULL)
        return NULL;
    return timeline->posts;
}

unsigned short platinum_timeline_post_count(
    const platinum_timeline *timeline)
{
    if (timeline == NULL)
        return 0;
    return timeline->count;
}

const char *platinum_timeline_status(const platinum_timeline *timeline)
{
    if (timeline == NULL)
        return NULL;
    return timeline->status;
}
