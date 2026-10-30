#include "conversations_wolfram.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wolfram/agent.h>
#include <wolfram/chat_typed.h>

#include "image_route.h"

/* Copies src into dst, cutting to fit cap bytes on a UTF-8 boundary. The native
 * model has fixed buffers, so the Node bridge's character limits are applied as
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

/* The first member who is not the signed-in account, and how many others there
 * are. A convo does not say which member is the viewer, so the counterpart is
 * found by DID, as api.ts does. */
static const wf_agent_profile_view *
pick_counterpart(const wf_chat_convo *convo, const char *viewer, size_t *others)
{
    const wf_agent_profile_view *first = NULL;
    size_t count = 0;
    size_t i;

    for (i = 0; i < convo->member_count; i++) {
        const wf_agent_profile_view *member = &convo->members[i];

        if (member->did == NULL || member->did[0] == '\0') continue;
        if (viewer != NULL && viewer[0] != '\0' && strcmp(member->did, viewer) == 0) continue;
        if (first == NULL) first = member;
        count++;
    }
    *others = count;
    return first;
}

platinum_conversations_result
platinum_conversations_fetch_live(void *ctx, int limit, const char *cursor,
                                  platinum_conversations *out)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_chat_convo_list page;
    const char *viewer;
    size_t count;
    size_t i;

    if (agent == NULL || out == NULL) return PLATINUM_CONVERSATIONS_ERROR;
    memset(&page, 0, sizeof page);

    if (wf_agent_chat_list_convos(agent, limit, cursor, &page) != WF_OK) {
        wf_chat_convo_list_free(&page);
        return PLATINUM_CONVERSATIONS_ERROR;
    }

    viewer = wf_agent_get_did(agent);
    count = page.convo_count < PLATINUM_CONVERSATIONS_ROWS_MAX
                ? page.convo_count
                : PLATINUM_CONVERSATIONS_ROWS_MAX;
    if (count > 0) {
        out->items = calloc(count, sizeof *out->items);
        if (out->items == NULL) {
            wf_chat_convo_list_free(&page);
            return PLATINUM_CONVERSATIONS_ERROR;
        }
        for (i = 0; i < count; i++) {
            const wf_chat_convo *convo = &page.convos[i];
            const wf_agent_profile_view *who;
            platinum_conversation *row;
            size_t others = 0;

            who = pick_counterpart(convo, viewer, &others);
            /* A conversation with nobody else in it has no row to draw. */
            if (who == NULL) continue;

            row = &out->items[out->count];
            memset(row, 0, sizeof *row);
            clip_bytes(convo->id, row->id, sizeof row->id);
            row->group = others > 1 ? 1 : 0;
            if (row->group) {
                strcpy(row->name, "Group");
            } else {
                const char *name = who->display_name != NULL && who->display_name[0] != '\0'
                                       ? who->display_name
                                       : who->handle != NULL && who->handle[0] != '\0'
                                             ? who->handle
                                             : who->did;
                char handle[PLATINUM_CONVO_HANDLE_MAX];

                clip_bytes(name, row->name, sizeof row->name);
                if (who->handle != NULL && who->handle[0] != '\0') {
                    snprintf(handle, sizeof handle, "@%s", who->handle);
                    clip_bytes(handle, row->handle, sizeof row->handle);
                }
                platinum_avatar_ref(who->avatar, row->avatar, sizeof row->avatar);
            }
            clip_bytes(convo->last_message_text, row->preview, sizeof row->preview);
            row->unread = convo->has_unread_count && convo->unread_count > 0
                              ? (long) convo->unread_count
                              : 0;
            out->count++;
        }
    }

    if (page.cursor != NULL && page.cursor[0] != '\0' &&
        strlen(page.cursor) < sizeof out->cursor) {
        strcpy(out->cursor, page.cursor);
        out->has_cursor = 1;
    }

    wf_chat_convo_list_free(&page);
    return PLATINUM_CONVERSATIONS_OK;
}

platinum_conversations_result
platinum_messages_fetch_live(void *ctx, const char *convo_id, int limit,
                             const char *cursor, platinum_conversation_messages *out)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_chat_message_list page;
    const char *viewer;
    size_t count;
    size_t i;

    if (agent == NULL || out == NULL || convo_id == NULL) return PLATINUM_CONVERSATIONS_ERROR;
    memset(&page, 0, sizeof page);

    if (wf_agent_chat_get_messages(agent, convo_id, limit, cursor, &page) != WF_OK) {
        wf_chat_message_list_free(&page);
        return PLATINUM_CONVERSATIONS_ERROR;
    }

    viewer = wf_agent_get_did(agent);
    count = page.message_count < PLATINUM_CONVERSATIONS_ROWS_MAX
                ? page.message_count
                : PLATINUM_CONVERSATIONS_ROWS_MAX;
    if (count > 0) {
        out->items = calloc(count, sizeof *out->items);
        if (out->items == NULL) {
            wf_chat_message_list_free(&page);
            return PLATINUM_CONVERSATIONS_ERROR;
        }
        for (i = 0; i < count; i++) {
            const wf_chat_message *src = &page.messages[i];
            platinum_conversation_message *row;

            /* A deleted message has no text and no sender: it is left out. */
            if (src->text == NULL || src->sender == NULL) continue;

            row = &out->items[out->count];
            memset(row, 0, sizeof *row);
            clip_bytes(src->id, row->id, sizeof row->id);
            clip_bytes(src->text, row->text, sizeof row->text);
            clip_bytes(src->sent_at, row->sent_at, sizeof row->sent_at);
            row->mine = viewer != NULL && strcmp(src->sender, viewer) == 0 ? 1 : 0;
            out->count++;
        }
    }

    if (page.cursor != NULL && page.cursor[0] != '\0' &&
        strlen(page.cursor) < sizeof out->cursor) {
        strcpy(out->cursor, page.cursor);
        out->has_cursor = 1;
    }

    wf_chat_message_list_free(&page);
    return PLATINUM_CONVERSATIONS_OK;
}

platinum_conversations_result
platinum_send_message_live(void *ctx, const char *convo_id, const char *text,
                           platinum_conversation_message *out)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_chat_message sent;

    if (agent == NULL || out == NULL || convo_id == NULL || text == NULL) {
        return PLATINUM_CONVERSATIONS_ERROR;
    }
    memset(&sent, 0, sizeof sent);
    if (wf_agent_chat_send_message(agent, convo_id, text, NULL, &sent) != WF_OK) {
        wf_chat_message_reset(&sent);
        return PLATINUM_CONVERSATIONS_ERROR;
    }

    memset(out, 0, sizeof *out);
    clip_bytes(sent.id, out->id, sizeof out->id);
    /* The service echoes the text; fall back to what was sent if it did not. */
    clip_bytes(sent.text != NULL ? sent.text : text, out->text, sizeof out->text);
    clip_bytes(sent.sent_at, out->sent_at, sizeof out->sent_at);
    out->mine = 1;
    wf_chat_message_reset(&sent);
    return PLATINUM_CONVERSATIONS_OK;
}
