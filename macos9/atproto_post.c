/*
 * atproto_post.c -- publish a post through the native session: the
 * app.bsky.feed.post record, its facets, its reply or quote reference, and
 * the threadgate for a reply gate. See atproto_post.h.
 */
#include "atproto_post.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "atproto_url.h"
#include "json_min.h"

#define POST_COLLECTION "app.bsky.feed.post"
#define GATE_COLLECTION "app.bsky.feed.threadgate"

/* ------------------------------------------------------------------ */
/* Facets                                                              */
/* ------------------------------------------------------------------ */

/* Wolfram's richtext.h boundary: start of text, white space or "(". */
static int facet_boundary(const char *text, size_t at)
{
    char c;

    if (at == 0)
        return 1;
    c = text[at - 1];
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '(';
}

static int facet_alnum(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9');
}

static int facet_space(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
           c == '\f';
}

/* Printable ASCII other than the closing brackets, as Wolfram's is_url_char. */
static int facet_url_char(unsigned char c)
{
    return c > 0x20 && c < 0x7f && c != '>' && c != ']' && c != '}';
}

/* Drop trailing sentence punctuation; a ")" stays when the run opened one. */
static size_t facet_strip(const char *s, size_t len)
{
    size_t index;
    int has_open;
    char c;

    while (len > 0) {
        c = s[len - 1];
        if (c != '.' && c != ',' && c != ';' && c != ':' && c != '!' &&
            c != '?' && c != ')')
            break;
        if (c == ')') {
            has_open = 0;
            for (index = 0; index + 1 < len; index++) {
                if (s[index] == '(') {
                    has_open = 1;
                    break;
                }
            }
            if (has_open)
                break;
        }
        len--;
    }
    return len;
}

/* Length of an http:// or https:// link starting at `at`, or 0. */
static size_t facet_link(const char *text, size_t len, size_t at)
{
    size_t scheme;
    size_t end;
    size_t kept;

    if (len - at >= 8 && strncmp(text + at, "https://", 8) == 0)
        scheme = 8;
    else if (len - at >= 7 && strncmp(text + at, "http://", 7) == 0)
        scheme = 7;
    else
        return 0;

    end = at;
    while (end < len && facet_url_char((unsigned char)text[end]))
        end++;
    kept = facet_strip(text + at, end - at);
    return kept > scheme ? kept : 0;
}

/* Length of "@handle" at `at` (the @ included), the handle copied to
 * `handle` in lower case; 0 when it is not a mention of a valid handle. */
static size_t facet_mention(const char *text, size_t len, size_t at,
                            char *handle, size_t cap)
{
    size_t end;
    size_t count;
    size_t index;
    char c;

    if (text[at] != '@' || !facet_boundary(text, at))
        return 0;
    end = at + 1;
    while (end < len &&
           (facet_alnum((unsigned char)text[end]) || text[end] == '.' ||
            text[end] == '-'))
        end++;
    while (end > at + 1 && text[end - 1] == '.')
        end--;
    count = end - at - 1;
    if (count == 0 || count >= cap)
        return 0;
    for (index = 0; index < count; index++) {
        c = text[at + 1 + index];
        handle[index] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    handle[count] = '\0';
    if (!platinum_atproto_valid_handle(handle))
        return 0;
    return end - at;
}

/* Length of "#tag" at `at` (the # included), or 0. A tag needs a letter or a
 * non-ASCII character and is at most 64 code points, as Wolfram's rule. */
static size_t facet_tag(const char *text, size_t len, size_t at)
{
    size_t end;
    size_t kept;
    size_t index;
    size_t points;
    int content;
    unsigned char c;

    if (text[at] != '#' || !facet_boundary(text, at))
        return 0;
    end = at + 1;
    while (end < len) {
        c = (unsigned char)text[end];
        if (facet_space(c) || c == '\\' || c == '"' || c == '\'')
            break;
        end++;
    }
    kept = facet_strip(text + at + 1, end - at - 1);
    if (kept == 0)
        return 0;
    content = 0;
    points = 0;
    for (index = 0; index < kept; index++) {
        c = (unsigned char)text[at + 1 + index];
        if (c >= 0x80 || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
            content = 1;
        if ((c & 0xC0) != 0x80)
            points++;
    }
    if (!content || points > 64)
        return 0;
    return kept + 1;
}

/* Append one facet. `value` is `value_len` bytes, escaped into the output. */
static int facet_append(char *out, size_t *used, size_t cap, char *scratch,
                        size_t scratch_cap, int first, size_t start,
                        size_t end, const char *type, const char *key,
                        const char *value, size_t value_len)
{
    char head[160];
    size_t head_len;
    size_t value_esc;

    if (value_len >= scratch_cap)
        return 0;
    memcpy(scratch, value, value_len);
    scratch[value_len] = '\0';
    sprintf(head,
            "%s{\"index\":{\"byteStart\":%lu,\"byteEnd\":%lu},"
            "\"features\":[{\"$type\":\"app.bsky.richtext.facet#%s\","
            "\"%s\":\"",
            first ? "" : ",", (unsigned long)start, (unsigned long)end, type,
            key);
    head_len = strlen(head);
    if (*used + head_len >= cap)
        return 0;
    memcpy(out + *used, head, head_len);
    *used += head_len;
    if (platinum_json_escape(out + *used, cap - *used, scratch) != WF_OK)
        return 0;
    value_esc = strlen(out + *used);
    *used += value_esc;
    if (*used + 5 >= cap)
        return 0;
    memcpy(out + *used, "\"}]}", 4);
    *used += 4;
    out[*used] = '\0';
    return 1;
}

char *platinum_atproto_post_facets(const char *utf8_text,
                                   platinum_atproto_handle_resolver resolve,
                                   void *userdata)
{
    size_t len;
    size_t cap;
    size_t used;
    size_t at;
    size_t match;
    size_t count;
    char *out;
    char *scratch;
    char handle[256];
    char did[256];
    int ok;

    if (utf8_text == NULL)
        return NULL;
    len = strlen(utf8_text);
    /* Values are disjoint substrings of the text (escaped at most six times
     * over) or DIDs; the rest is a fixed frame per facet. */
    cap = 8 + len * 6 + PLATINUM_ATPROTO_FACETS_MAX * (160 + sizeof(did));
    out = (char *)malloc(cap);
    scratch = (char *)malloc(len + sizeof(did) + 1);
    if (out == NULL || scratch == NULL) {
        free(out);
        free(scratch);
        return NULL;
    }

    out[0] = '[';
    out[1] = '\0';
    used = 1;
    count = 0;
    ok = 1;
    at = 0;
    while (at < len && count < PLATINUM_ATPROTO_FACETS_MAX && ok) {
        match = facet_link(utf8_text, len, at);
        if (match != 0) {
            ok = facet_append(out, &used, cap, scratch, len + sizeof(did) + 1,
                              count == 0, at, at + match, "link", "uri",
                              utf8_text + at, match);
            count++;
            at += match;
            continue;
        }
        match = facet_mention(utf8_text, len, at, handle, sizeof(handle));
        if (match != 0) {
            did[0] = '\0';
            if (resolve != NULL &&
                resolve(handle, did, sizeof(did), userdata) == 1 &&
                platinum_atproto_valid_did(did)) {
                ok = facet_append(out, &used, cap, scratch,
                                  len + sizeof(did) + 1, count == 0, at,
                                  at + match, "mention", "did", did,
                                  strlen(did));
                count++;
            }
            at += match;
            continue;
        }
        match = facet_tag(utf8_text, len, at);
        if (match != 0) {
            ok = facet_append(out, &used, cap, scratch, len + sizeof(did) + 1,
                              count == 0, at, at + match, "tag", "tag",
                              utf8_text + at + 1, match - 1);
            count++;
            at += match;
            continue;
        }
        at++;
    }
    free(scratch);
    if (!ok) {
        free(out);
        return NULL;
    }
    if (count == 0) {
        out[0] = '\0';
        return out;
    }
    out[used++] = ']';
    out[used] = '\0';
    return out;
}

/* ------------------------------------------------------------------ */
/* Record bodies                                                       */
/* ------------------------------------------------------------------ */

static int post_valid_cid(const char *cid)
{
    const char *scan;
    size_t length;

    if (cid == NULL)
        return 0;
    length = strlen(cid);
    if (length == 0 || length > 255)
        return 0;
    for (scan = cid; *scan != '\0'; scan++) {
        if (!facet_alnum((unsigned char)*scan))
            return 0;
    }
    return 1;
}

static int post_set(const char *value)
{
    return value != NULL && value[0] != '\0';
}

/* A strong reference: both parts present, a valid AT URI and cid. */
static int post_valid_ref(const char *uri, const char *cid)
{
    return post_set(uri) && strlen(uri) < 513 &&
           platinum_atproto_valid_at_uri(uri) && post_valid_cid(cid);
}

/* Append "<prefix>{"uri":"...","cid":"..."}"; both are already checked to
 * hold no character that needs escaping. */
static void post_ref(char *body, const char *prefix, const char *uri,
                     const char *cid)
{
    strcat(body, prefix);
    strcat(body, "{\"uri\":\"");
    strcat(body, uri);
    strcat(body, "\",\"cid\":\"");
    strcat(body, cid);
    strcat(body, "\"}");
}

static int post_uri_plain(const char *uri)
{
    const char *scan;

    for (scan = uri; *scan != '\0'; scan++) {
        if (*scan == '"' || *scan == '\\' || (unsigned char)*scan < 0x20)
            return 0;
    }
    return 1;
}

char *platinum_atproto_post_record_body(
    const char *repo_did, const char *utf8_text, const char *facets_json,
    const char *root_uri, const char *root_cid, const char *parent_uri,
    const char *parent_cid, const char *quote_uri, const char *quote_cid,
    const char *created_at)
{
    int has_reply;
    int has_quote;
    size_t text_cap;
    size_t cap;
    char *text_esc;
    char *body;

    if (repo_did == NULL || !platinum_atproto_valid_did(repo_did) ||
        utf8_text == NULL || created_at == NULL || created_at[0] == '\0' ||
        strlen(created_at) > 40 || !post_uri_plain(created_at))
        return NULL;

    has_reply = post_set(root_uri) || post_set(root_cid) ||
                post_set(parent_uri) || post_set(parent_cid);
    if (has_reply && (!post_valid_ref(root_uri, root_cid) ||
                      !post_valid_ref(parent_uri, parent_cid) ||
                      !post_uri_plain(root_uri) || !post_uri_plain(parent_uri)))
        return NULL;
    has_quote = post_set(quote_uri) || post_set(quote_cid);
    if (has_quote && (!post_valid_ref(quote_uri, quote_cid) ||
                      !post_uri_plain(quote_uri)))
        return NULL;
    if (has_reply && has_quote)
        return NULL;
    if (post_set(facets_json) && facets_json[0] != '[')
        return NULL;

    text_cap = strlen(utf8_text) * 6 + 1;
    cap = text_cap + (post_set(facets_json) ? strlen(facets_json) : 0) +
          strlen(repo_did) + 4 * 513 + 512;
    text_esc = (char *)malloc(text_cap);
    body = (char *)malloc(cap);
    if (text_esc == NULL || body == NULL ||
        platinum_json_escape(text_esc, text_cap, utf8_text) != WF_OK) {
        free(text_esc);
        free(body);
        return NULL;
    }

    strcpy(body, "{\"repo\":\"");
    strcat(body, repo_did);
    strcat(body, "\",\"collection\":\"" POST_COLLECTION "\","
                 "\"record\":{\"$type\":\"" POST_COLLECTION "\",\"text\":\"");
    strcat(body, text_esc);
    strcat(body, "\"");
    free(text_esc);
    if (post_set(facets_json)) {
        strcat(body, ",\"facets\":");
        strcat(body, facets_json);
    }
    if (has_reply) {
        post_ref(body, ",\"reply\":{\"root\":", root_uri, root_cid);
        post_ref(body, ",\"parent\":", parent_uri, parent_cid);
        strcat(body, "}");
    }
    if (has_quote) {
        post_ref(body,
                 ",\"embed\":{\"$type\":\"app.bsky.embed.record\",\"record\":",
                 quote_uri, quote_cid);
        strcat(body, "}");
    }
    strcat(body, ",\"createdAt\":\"");
    strcat(body, created_at);
    strcat(body, "\"}}");

    if (platinum_json_valid(body) != WF_OK) {
        free(body);
        return NULL;
    }
    return body;
}

/* The threadgate allow list for a gate name, or NULL for an unknown one. */
static const char *gate_allow(const char *gate)
{
    if (gate == NULL)
        return NULL;
    if (strcmp(gate, "nobody") == 0)
        return "[]";
    if (strcmp(gate, "mentioned") == 0)
        return "[{\"$type\":\"" GATE_COLLECTION "#mentionRule\"}]";
    if (strcmp(gate, "following") == 0)
        return "[{\"$type\":\"" GATE_COLLECTION "#followingRule\"}]";
    if (strcmp(gate, "followers") == 0)
        return "[{\"$type\":\"" GATE_COLLECTION "#followerRule\"}]";
    return NULL;
}

/* The record key of `post_uri` when it is a post in `repo_did`, else NULL. */
static const char *post_rkey(const char *repo_did, const char *post_uri)
{
    char prefix[320];
    size_t length;
    const char *rkey;

    if (repo_did == NULL || post_uri == NULL ||
        strlen(repo_did) + sizeof(POST_COLLECTION) + 8 >= sizeof(prefix) ||
        !platinum_atproto_valid_at_uri(post_uri) || !post_uri_plain(post_uri))
        return NULL;
    sprintf(prefix, "at://%s/" POST_COLLECTION "/", repo_did);
    length = strlen(prefix);
    if (strncmp(post_uri, prefix, length) != 0)
        return NULL;
    rkey = post_uri + length;
    if (rkey[0] == '\0' || strchr(rkey, '/') != NULL || strlen(rkey) > 512)
        return NULL;
    return rkey;
}

wf_status platinum_atproto_threadgate_body(const char *repo_did,
                                           const char *post_uri,
                                           const char *gate,
                                           const char *created_at, char *out,
                                           size_t cap)
{
    const char *allow;
    const char *rkey;
    size_t need;

    if (out == NULL || cap == 0)
        return WF_ERR_INVALID_ARG;
    out[0] = '\0';
    allow = gate_allow(gate);
    if (allow == NULL || repo_did == NULL ||
        !platinum_atproto_valid_did(repo_did) || created_at == NULL ||
        created_at[0] == '\0' || strlen(created_at) > 40 ||
        !post_uri_plain(created_at))
        return WF_ERR_INVALID_ARG;
    rkey = post_rkey(repo_did, post_uri);
    if (rkey == NULL)
        return WF_ERR_INVALID_ARG;

    need = strlen(repo_did) + strlen(rkey) + strlen(post_uri) +
           strlen(allow) + strlen(created_at) + 160;
    if (need >= cap)
        return WF_ERR_ALLOC;
    sprintf(out,
            "{\"repo\":\"%s\",\"collection\":\"" GATE_COLLECTION "\","
            "\"rkey\":\"%s\",\"record\":{\"$type\":\"" GATE_COLLECTION "\","
            "\"post\":\"%s\",\"allow\":%s,\"createdAt\":\"%s\"}}",
            repo_did, rkey, post_uri, allow, created_at);
    return WF_OK;
}

/* ------------------------------------------------------------------ */
/* Publishing                                                          */
/* ------------------------------------------------------------------ */

static wf_status post_timestamp(char *out, size_t cap)
{
    time_t now;
    struct tm *utc;

    now = time(NULL);
    if (now == (time_t)-1)
        return WF_ERR_STATE;
    utc = gmtime(&now);
    if (utc == NULL || strftime(out, cap, "%Y-%m-%dT%H:%M:%SZ", utc) == 0)
        return WF_ERR_STATE;
    return WF_OK;
}

typedef struct post_scratch {
    char parent_uri[513];
    char parent_cid[256];
    char root_uri[513];
    char root_cid[256];
    char quote_uri[513];
    char quote_cid[256];
    char post_uri[513];
    char gate_body[2048];
} post_scratch;

/*
 * Read one post back from the AppView: its uri and cid, and the root of the
 * thread it belongs to (itself when it is not a reply). WF_ERR_NOT_FOUND when
 * the AppView no longer has it; WF_ERR_PARSE when it names a root this client
 * cannot read, because answering the wrong root breaks the thread.
 */
static wf_status post_lookup(platinum_atproto_client *client, const char *uri,
                             char *out_uri, char *out_cid, char *root_uri,
                             char *root_cid)
{
    static const char *const names[1] = { "uris" };
    const char *values[1];
    wf_response response;
    wf_status status;
    platinum_json doc;
    platinum_json posts;
    platinum_json post;
    platinum_json record;
    platinum_json reply;
    platinum_json root;

    values[0] = uri;
    memset(&response, 0, sizeof(response));
    status = platinum_atproto_client_get(client, "app.bsky.feed.getPosts",
                                         names, values, 1, &response);
    if (status != WF_OK) {
        wf_response_free(&response);
        return status;
    }
    if (response.body == NULL ||
        platinum_json_open(&doc, response.body) != WF_OK ||
        platinum_json_member(doc, "posts", &posts) != WF_OK) {
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }
    if (platinum_json_element(posts, 0, &post) != WF_OK) {
        wf_response_free(&response);
        return WF_ERR_NOT_FOUND;
    }
    if (platinum_json_string(post, "uri", out_uri, 513) != WF_OK ||
        platinum_json_string(post, "cid", out_cid, 256) != WF_OK ||
        !post_valid_ref(out_uri, out_cid) || !post_uri_plain(out_uri)) {
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    strcpy(root_uri, out_uri);
    strcpy(root_cid, out_cid);
    if (platinum_json_member(post, "record", &record) == WF_OK &&
        platinum_json_member(record, "reply", &reply) == WF_OK) {
        if (platinum_json_member(reply, "root", &root) != WF_OK ||
            platinum_json_string(root, "uri", root_uri, 513) != WF_OK ||
            platinum_json_string(root, "cid", root_cid, 256) != WF_OK ||
            !post_valid_ref(root_uri, root_cid) || !post_uri_plain(root_uri)) {
            wf_response_free(&response);
            return WF_ERR_PARSE;
        }
    }
    wf_response_free(&response);
    return WF_OK;
}

/* Resolve a mentioned handle through the PDS the session already dials. */
static int post_resolve(const char *handle, char *did, size_t cap,
                        void *userdata)
{
    static const char *const names[1] = { "handle" };
    const char *values[1];
    wf_response response;
    wf_status status;
    platinum_json doc;
    int ok;

    values[0] = handle;
    memset(&response, 0, sizeof(response));
    status = platinum_atproto_client_get(
        (platinum_atproto_client *)userdata,
        "com.atproto.identity.resolveHandle", names, values, 1, &response);
    ok = status == WF_OK && response.body != NULL &&
         platinum_json_open(&doc, response.body) == WF_OK &&
         platinum_json_string(doc, "did", did, cap) == WF_OK &&
         platinum_atproto_valid_did(did);
    wf_response_free(&response);
    if (!ok && cap > 0)
        did[0] = '\0';
    return ok;
}

wf_status platinum_atproto_publish(platinum_atproto_client *client,
                                   const char *utf8_text,
                                   const char *reply_uri,
                                   const char *reply_cid,
                                   const char *quote_uri,
                                   const char *quote_cid, const char *gate,
                                   int *gate_failed)
{
    const platinum_atproto_session *session;
    post_scratch *scratch;
    char created_at[32];
    char *facets;
    char *body;
    int has_reply;
    int has_quote;
    wf_response response;
    wf_status status;
    platinum_json doc;

    if (gate_failed != NULL)
        *gate_failed = 0;
    if (client == NULL || utf8_text == NULL || gate_failed == NULL)
        return WF_ERR_INVALID_ARG;

    has_reply = post_set(reply_uri) || post_set(reply_cid);
    has_quote = post_set(quote_uri) || post_set(quote_cid);
    if ((has_reply && !post_valid_ref(reply_uri, reply_cid)) ||
        (has_quote && !post_valid_ref(quote_uri, quote_cid)) ||
        (has_reply && has_quote))
        return WF_ERR_INVALID_ARG;
    if (gate != NULL && strcmp(gate, "everyone") == 0)
        gate = NULL;
    if (gate != NULL && (gate_allow(gate) == NULL || has_reply))
        return WF_ERR_INVALID_ARG;

    session = platinum_atproto_client_session(client);
    if (session == NULL || !platinum_atproto_valid_did(session->did))
        return WF_ERR_AUTH;

    scratch = (post_scratch *)malloc(sizeof(post_scratch));
    if (scratch == NULL)
        return WF_ERR_ALLOC;
    memset(scratch, 0, sizeof(*scratch));

    if (has_reply) {
        status = post_lookup(client, reply_uri, scratch->parent_uri,
                             scratch->parent_cid, scratch->root_uri,
                             scratch->root_cid);
        if (status != WF_OK) {
            free(scratch);
            return status;
        }
    }
    if (has_quote) {
        /* The AppView's own cid, so a stale copy cannot be quoted. */
        status = post_lookup(client, quote_uri, scratch->quote_uri,
                             scratch->quote_cid, scratch->root_uri,
                             scratch->root_cid);
        scratch->root_uri[0] = '\0';
        scratch->root_cid[0] = '\0';
        if (status != WF_OK) {
            free(scratch);
            return status;
        }
    }

    status = post_timestamp(created_at, sizeof(created_at));
    if (status != WF_OK) {
        free(scratch);
        return status;
    }
    facets = platinum_atproto_post_facets(utf8_text, post_resolve, client);
    if (facets == NULL) {
        free(scratch);
        return WF_ERR_ALLOC;
    }
    body = platinum_atproto_post_record_body(
        session->did, utf8_text, facets, scratch->root_uri, scratch->root_cid,
        scratch->parent_uri, scratch->parent_cid, scratch->quote_uri,
        scratch->quote_cid, created_at);
    free(facets);
    if (body == NULL) {
        free(scratch);
        return WF_ERR_INVALID_ARG;
    }

    memset(&response, 0, sizeof(response));
    status = platinum_atproto_client_post(
        client, "com.atproto.repo.createRecord", body, &response);
    free(body);
    if (status != WF_OK) {
        wf_response_free(&response);
        free(scratch);
        return status;
    }

    if (gate != NULL) {
        /* The post is out whatever happens now; a gate that cannot be
         * written is reported, never turned into a failed post. */
        session = platinum_atproto_client_session(client);
        if (session == NULL || response.body == NULL ||
            platinum_json_open(&doc, response.body) != WF_OK ||
            platinum_json_string(doc, "uri", scratch->post_uri,
                                 sizeof(scratch->post_uri)) != WF_OK ||
            platinum_atproto_threadgate_body(
                session->did, scratch->post_uri, gate, created_at,
                scratch->gate_body, sizeof(scratch->gate_body)) != WF_OK) {
            *gate_failed = 1;
        } else {
            wf_response_free(&response);
            memset(&response, 0, sizeof(response));
            if (platinum_atproto_client_post(
                    client, "com.atproto.repo.createRecord",
                    scratch->gate_body, &response) != WF_OK)
                *gate_failed = 1;
        }
    }
    wf_response_free(&response);
    free(scratch);
    return WF_OK;
}
