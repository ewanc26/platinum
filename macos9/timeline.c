#include "timeline.h"

#include <cJSON.h>
#include <string.h>

static int timeline_copy_json_string(char *destination,
                                     long capacity,
                                     const cJSON *value)
{
    long length;

    if (destination == NULL || value == NULL || capacity <= 0)
        return 0;
    if (!cJSON_IsString(value) || value->valuestring == NULL)
        return 0;

    length = (long)strlen(value->valuestring);
    if (length >= capacity)
        length = capacity - 1;

    if (length > 0)
        memcpy(destination, value->valuestring, (size_t)length);
    destination[length] = '\\0';
    return 1;
}

static long timeline_json_number(const cJSON *object, const char *name)
{
    const cJSON *item;

    item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (item == NULL || !cJSON_IsNumber(item))
        return 0;
    return item->valueint;
}

static void timeline_set_status(platinum_timeline *timeline,
                                const char *status)
{
    long length;

    if (timeline == NULL)
        return;

    timeline->status[0] = '\\0';
    if (status == NULL)
        return;

    length = (long)strlen(status);
    if (length > PLATINUM_TIMELINE_STATUS_MAX)
        length = PLATINUM_TIMELINE_STATUS_MAX;

    memcpy(timeline->status, status, (size_t)length);
    timeline->status[length] = '\\0';
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

    post->line1[0] = '\\0';
    post->line2[0] = '\\0';
    post->line3[0] = '\\0';

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
        destination[chunk] = '\\0';
        offset += chunk;
    }
}

static int timeline_parse_post(platinum_post_preview *post,
                               const cJSON *item)
{
    const cJSON *author;
    const cJSON *value;
    const cJSON *record;
    const cJSON *handle;
    const cJSON *display_name;
    const cJSON *created_at;

    if (post == NULL || item == NULL || !cJSON_IsObject(item))
        return 0;

    memset(post, 0, sizeof(*post));

    value = cJSON_GetObjectItemCaseSensitive(item, "uri");
    if (!timeline_copy_json_string(post->uri, sizeof(post->uri), value))
        return 0;

    value = cJSON_GetObjectItemCaseSensitive(item, "cid");
    if (!timeline_copy_json_string(post->cid, sizeof(post->cid), value))
        return 0;

    author = cJSON_GetObjectItemCaseSensitive(item, "author");
    if (author == NULL || !cJSON_IsObject(author))
        return 0;

    value = cJSON_GetObjectItemCaseSensitive(author, "did");
    if (value == NULL || !cJSON_IsString(value) || value->valuestring == NULL)
        return 0;

    handle = cJSON_GetObjectItemCaseSensitive(author, "handle");
    display_name = cJSON_GetObjectItemCaseSensitive(author, "displayName");

    post->author[0] = '\\0';
    if (display_name != NULL)
        timeline_copy_json_string(post->author,
                                  sizeof(post->author),
                                  display_name);
    if (post->author[0] == '\\0' && handle != NULL)
        timeline_copy_json_string(post->author,
                                  sizeof(post->author),
                                  handle);
    if (post->author[0] == '\\0')
        timeline_copy_json_string(post->author,
                                  sizeof(post->author),
                                  value);

    post->handle[0] = '@';
    if (handle != NULL && cJSON_IsString(handle) &&
        handle->valuestring != NULL) {
        timeline_copy_json_string(post->handle + 1,
                                  sizeof(post->handle) - 1,
                                  handle);
    } else {
        post->handle[1] = '\\0';
    }

    created_at = cJSON_GetObjectItemCaseSensitive(item, "createdAt");
    timeline_copy_json_string(post->time, sizeof(post->time), created_at);

    record = cJSON_GetObjectItemCaseSensitive(item, "text");
    if (record != NULL && cJSON_IsString(record) &&
        record->valuestring != NULL)
        timeline_copy_wrapped_text(post, record->valuestring);

    post->like_count = timeline_json_number(item, "likeCount");
    post->repost_count = timeline_json_number(item, "repostCount");
    post->reply_count = timeline_json_number(item, "replyCount");
    post->quote_count = timeline_json_number(item, "quoteCount");
    return 1;
}

void platinum_timeline_init(platinum_timeline *timeline)
{
    if (timeline == NULL)
        return;

    memset(timeline, 0, sizeof(*timeline));
    timeline_set_status(timeline, "Pair an account to load the timeline.");
}

wf_status platinum_timeline_refresh(platinum_timeline *timeline,
                                    platinum_bridge_client *bridge)
{
    wf_response response;
    cJSON *root;
    cJSON *posts;
    cJSON *cursor;
    cJSON *item;
    int index;
    int parsed;
    wf_status status;

    if (timeline == NULL || bridge == NULL)
        return WF_ERR_INVALID_ARG;

    timeline->loading = 1;
    timeline->count = 0;
    timeline_set_status(timeline, "Loading timeline...");
    memset(&response, 0, sizeof(response));

    status = platinum_bridge_get(bridge, "/v1/timeline?limit=20", &response);
    if (status != WF_OK) {
        timeline->loading = 0;
        timeline_set_status(timeline, "Timeline refresh failed.");
        wf_response_free(&response);
        return status;
    }

    root = cJSON_Parse(response.body != NULL ? response.body : "");
    if (root == NULL) {
        timeline->loading = 0;
        timeline_set_status(timeline,
                            "The bridge returned invalid timeline data.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    posts = cJSON_GetObjectItemCaseSensitive(root, "posts");
    if (posts == NULL || !cJSON_IsArray(posts)) {
        cJSON_Delete(root);
        timeline->loading = 0;
        timeline_set_status(timeline,
                            "The bridge returned an invalid timeline.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    timeline->count = 0;
    parsed = cJSON_GetArraySize(posts);
    if (parsed > PLATINUM_TIMELINE_MAX_POSTS)
        parsed = PLATINUM_TIMELINE_MAX_POSTS;

    for (index = 0; index < parsed; ++index) {
        item = cJSON_GetArrayItem(posts, index);
        if (timeline_parse_post(&timeline->posts[timeline->count], item))
            ++timeline->count;
    }

    cursor = cJSON_GetObjectItemCaseSensitive(root, "cursor");
    timeline->cursor[0] = '\\0';
    if (cursor != NULL && cJSON_IsString(cursor) &&
        cursor->valuestring != NULL) {
        timeline_copy_json_string(timeline->cursor,
                                  sizeof(timeline->cursor),
                                  cursor);
    }

    cJSON_Delete(root);
    wf_response_free(&response);

    timeline->loading = 0;
    if (timeline->count == 0)
        timeline_set_status(timeline, "The timeline is empty.");
    else
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
