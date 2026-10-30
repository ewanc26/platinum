#include "conversations.h"

#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "search_routes.h"

void
platinum_conversations_free(platinum_conversations *out)
{
    if (out == NULL) return;
    free(out->items);
    memset(out, 0, sizeof *out);
}

void
platinum_conversation_messages_free(platinum_conversation_messages *out)
{
    if (out == NULL) return;
    free(out->items);
    memset(out, 0, sizeof *out);
}

int
platinum_valid_convo_id(const char *id)
{
    size_t i;
    size_t n;

    if (id == NULL || id[0] == '\0' || strlen(id) >= PLATINUM_CONVO_ID_MAX) return 0;
    n = strlen(id);
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char) id[i];
        if (c <= 0x20 || c >= 0x7f) return 0;
    }
    return 1;
}

static int
write_error(char *body, size_t cap, int status, const char *code, const char *message)
{
    int n = snprintf(body, cap, "{\"error\":\"%s\",\"message\":\"%s\"}", code, message);
    if (n < 0 || (size_t) n >= cap) {
        if (cap > 0) body[0] = '\0';
        return 500;
    }
    return status;
}

/* Prints root into body and frees it. A body that does not fit is a 500, never a
 * cut one: the Mac parses whatever it gets. */
static int
finish(cJSON *root, char *body, size_t cap, int status)
{
    int printed;

    if (root == NULL) {
        if (cap > 0) body[0] = '\0';
        return 500;
    }
    printed = cJSON_PrintPreallocated(root, body, (int) cap, 0);
    cJSON_Delete(root);
    if (!printed) {
        if (cap > 0) body[0] = '\0';
        return 500;
    }
    return status;
}

/* Adds a string member only when it is set, as the Node bridge leaves optional
 * fields out rather than sending them empty. */
static void
add_optional(cJSON *obj, const char *key, const char *value)
{
    if (value != NULL && value[0] != '\0') cJSON_AddStringToObject(obj, key, value);
}

int
platinum_route_conversations(void *ctx, platinum_conversations_fetch_fn fetch,
                             const char *limit_param, const char *cursor_param,
                             char *body, size_t cap)
{
    platinum_conversations page;
    cJSON *root;
    cJSON *arr;
    size_t i;
    int limit;

    if (body == NULL || cap == 0 || fetch == NULL) return 500;
    body[0] = '\0';
    if (cursor_param != NULL && strlen(cursor_param) >= sizeof page.cursor) {
        return write_error(body, cap, 400, "invalid_cursor",
                           "cursor must be 255 bytes or fewer.");
    }

    limit = platinum_query_limit(limit_param);
    memset(&page, 0, sizeof page);
    if (fetch(ctx, limit, cursor_param, &page) != PLATINUM_CONVERSATIONS_OK) {
        platinum_conversations_free(&page);
        return write_error(body, cap, 500, "bridge_error",
                           "The backend could not complete the request.");
    }
    if (page.count > PLATINUM_CONVERSATIONS_ROWS_MAX) {
        platinum_conversations_free(&page);
        return write_error(body, cap, 500, "bridge_error",
                           "The backend could not complete the request.");
    }

    root = cJSON_CreateObject();
    arr = cJSON_AddArrayToObject(root, "conversations");
    if (root == NULL || arr == NULL) {
        cJSON_Delete(root);
        platinum_conversations_free(&page);
        return write_error(body, cap, 500, "bridge_error",
                           "The backend could not complete the request.");
    }
    for (i = 0; i < page.count; i++) {
        const platinum_conversation *c = &page.items[i];
        cJSON *row = cJSON_CreateObject();

        if (row == NULL) continue;
        cJSON_AddStringToObject(row, "id", c->id);
        cJSON_AddStringToObject(row, "name", c->name);
        add_optional(row, "handle", c->handle);
        add_optional(row, "avatar", c->avatar);
        cJSON_AddBoolToObject(row, "group", c->group ? 1 : 0);
        cJSON_AddStringToObject(row, "preview", c->preview);
        cJSON_AddNumberToObject(row, "unread", (double) c->unread);
        cJSON_AddItemToArray(arr, row);
    }
    if (page.has_cursor && page.cursor[0] != '\0') {
        cJSON_AddStringToObject(root, "cursor", page.cursor);
    }

    platinum_conversations_free(&page);
    return finish(root, body, cap, 200);
}

int
platinum_route_conversation(void *ctx, platinum_messages_fetch_fn fetch,
                            const char *convo_id, const char *limit_param,
                            const char *cursor_param, char *body, size_t cap)
{
    platinum_conversation_messages page;
    cJSON *root;
    cJSON *arr;
    size_t i;
    int limit;

    if (body == NULL || cap == 0 || fetch == NULL) return 500;
    body[0] = '\0';
    if (!platinum_valid_convo_id(convo_id)) {
        return write_error(body, cap, 400, "invalid_conversation",
                           "id must name a conversation.");
    }
    if (cursor_param != NULL && strlen(cursor_param) >= sizeof page.cursor) {
        return write_error(body, cap, 400, "invalid_cursor",
                           "cursor must be 255 bytes or fewer.");
    }

    limit = platinum_query_limit(limit_param);
    memset(&page, 0, sizeof page);
    if (fetch(ctx, convo_id, limit, cursor_param, &page) != PLATINUM_CONVERSATIONS_OK) {
        platinum_conversation_messages_free(&page);
        return write_error(body, cap, 500, "bridge_error",
                           "The backend could not complete the request.");
    }
    if (page.count > PLATINUM_CONVERSATIONS_ROWS_MAX) {
        platinum_conversation_messages_free(&page);
        return write_error(body, cap, 500, "bridge_error",
                           "The backend could not complete the request.");
    }

    root = cJSON_CreateObject();
    arr = cJSON_AddArrayToObject(root, "messages");
    if (root == NULL || arr == NULL) {
        cJSON_Delete(root);
        platinum_conversation_messages_free(&page);
        return write_error(body, cap, 500, "bridge_error",
                           "The backend could not complete the request.");
    }
    for (i = 0; i < page.count; i++) {
        const platinum_conversation_message *m = &page.items[i];
        cJSON *row = cJSON_CreateObject();

        if (row == NULL) continue;
        cJSON_AddStringToObject(row, "id", m->id);
        cJSON_AddStringToObject(row, "text", m->text);
        cJSON_AddBoolToObject(row, "mine", m->mine ? 1 : 0);
        cJSON_AddStringToObject(row, "sentAt", m->sent_at);
        cJSON_AddItemToArray(arr, row);
    }
    if (page.has_cursor && page.cursor[0] != '\0') {
        cJSON_AddStringToObject(root, "cursor", page.cursor);
    }

    platinum_conversation_messages_free(&page);
    return finish(root, body, cap, 200);
}

/* The Node bridge counts characters, not bytes, for the 1000 limit. Counting the
 * non-continuation bytes of valid UTF-8 is the same number, and a byte sequence
 * the service would refuse anyway is reported as too long rather than accepted. */
static size_t
utf8_length(const char *text)
{
    const unsigned char *p = (const unsigned char *) text;
    size_t n = 0;

    for (; *p != '\0'; p++) {
        if ((*p & 0xC0) != 0x80) n++;
    }
    return n;
}

static int
text_has_content(const char *text)
{
    const char *p;

    for (p = text; *p != '\0'; p++) {
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') return 1;
    }
    return 0;
}

int
platinum_route_conversation_message(void *ctx, platinum_send_message_fn send,
                                    const char *convo_id, const char *text,
                                    char *body, size_t cap)
{
    platinum_conversation_message message;
    cJSON *root;

    if (body == NULL || cap == 0 || send == NULL) return 500;
    body[0] = '\0';
    if (!platinum_valid_convo_id(convo_id)) {
        return write_error(body, cap, 400, "invalid_conversation",
                           "convoId must name a conversation.");
    }
    if (text == NULL || !text_has_content(text)) {
        return write_error(body, cap, 400, "missing_text",
                           "Message text is required.");
    }
    if (utf8_length(text) > 1000) {
        return write_error(body, cap, 400, "text_too_long",
                           "A message is at most 1000 characters.");
    }

    memset(&message, 0, sizeof message);
    if (send(ctx, convo_id, text, &message) != PLATINUM_CONVERSATIONS_OK) {
        return write_error(body, cap, 500, "bridge_error",
                           "The backend could not complete the request.");
    }

    root = cJSON_CreateObject();
    if (root == NULL) {
        return write_error(body, cap, 500, "bridge_error",
                           "The backend could not complete the request.");
    }
    cJSON_AddStringToObject(root, "id", message.id);
    cJSON_AddStringToObject(root, "text", message.text);
    cJSON_AddBoolToObject(root, "mine", message.mine ? 1 : 0);
    cJSON_AddStringToObject(root, "sentAt", message.sent_at);
    return finish(root, body, cap, 200);
}
