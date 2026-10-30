/*
 * The live conversation fetches: Wolfram's listConvos, getMessages and
 * sendMessage, adapted to the conversation routes. Built as C2x against
 * Wolfram's headers and linked to libwolfram; the routes do not change.
 */
#ifndef PLATINUM_NATIVE_CONVERSATIONS_WOLFRAM_H
#define PLATINUM_NATIVE_CONVERSATIONS_WOLFRAM_H

#include "conversations.h"

/* platinum_conversations_fetch_fn, platinum_messages_fetch_fn and
 * platinum_send_message_fn whose ctx is a wf_agent *. */
platinum_conversations_result platinum_conversations_fetch_live(
    void *ctx, int limit, const char *cursor, platinum_conversations *out);
platinum_conversations_result platinum_messages_fetch_live(
    void *ctx, const char *convo_id, int limit, const char *cursor,
    platinum_conversation_messages *out);
platinum_conversations_result platinum_send_message_live(
    void *ctx, const char *convo_id, const char *text,
    platinum_conversation_message *out);

#endif
