/*
 * The conversation routes (Part 8): GET /v1/conversations, GET /v1/conversation
 * and POST /v1/conversation/message.
 *
 * The status and body decisions of server.ts: the limit is 1 to 50 (default 20),
 * a cursor is passed through and refused when it cannot fit the response, a
 * conversation with nobody else in it is left out, and a message is at most
 * 1000 characters with something other than spaces in it. The Wolfram calls live
 * in conversations_wolfram.c, behind callbacks, so this file needs neither the
 * network nor Wolfram.
 */
#ifndef PLATINUM_NATIVE_CONVERSATIONS_H
#define PLATINUM_NATIVE_CONVERSATIONS_H

#include <stddef.h>

/* One page holds at most the largest limit the routes accept. */
#define PLATINUM_CONVERSATIONS_ROWS_MAX 50
/* Field sizes are the Node bridge's clipping limits, in bytes, plus a terminator. */
#define PLATINUM_CONVO_ID_MAX 129
#define PLATINUM_CONVO_NAME_MAX 65
#define PLATINUM_CONVO_HANDLE_MAX 257
#define PLATINUM_CONVO_AVATAR_MAX 301
#define PLATINUM_CONVO_PREVIEW_MAX 129
#define PLATINUM_CONVO_MESSAGE_MAX 1001
#define PLATINUM_CONVO_SENT_AT_MAX 65

typedef struct platinum_conversation {
    char id[PLATINUM_CONVO_ID_MAX];
    char name[PLATINUM_CONVO_NAME_MAX];
    char handle[PLATINUM_CONVO_HANDLE_MAX];   /* with a leading @; empty for a group */
    char avatar[PLATINUM_CONVO_AVATAR_MAX];   /* an image reference; empty for a group */
    int group;
    char preview[PLATINUM_CONVO_PREVIEW_MAX];
    long unread;
} platinum_conversation;

typedef struct platinum_conversations {
    platinum_conversation *items;   /* malloc'd, count entries; may be NULL */
    size_t count;
    char cursor[256];
    int has_cursor;
} platinum_conversations;

typedef struct platinum_conversation_message {
    char id[PLATINUM_CONVO_ID_MAX];
    char text[PLATINUM_CONVO_MESSAGE_MAX];
    int mine;
    char sent_at[PLATINUM_CONVO_SENT_AT_MAX];
} platinum_conversation_message;

typedef struct platinum_conversation_messages {
    platinum_conversation_message *items;
    size_t count;
    char cursor[256];
    int has_cursor;
} platinum_conversation_messages;

typedef enum platinum_conversations_result {
    PLATINUM_CONVERSATIONS_OK = 0,
    PLATINUM_CONVERSATIONS_ERROR
} platinum_conversations_result;

/* Fetches one page of the account's conversations. cursor is NULL for the first. */
typedef platinum_conversations_result (*platinum_conversations_fetch_fn)(
    void *ctx, int limit, const char *cursor, platinum_conversations *out);

/* Fetches one page of one conversation's messages, newest first. */
typedef platinum_conversations_result (*platinum_messages_fetch_fn)(
    void *ctx, const char *convo_id, int limit, const char *cursor,
    platinum_conversation_messages *out);

/* Sends text and fills the message as the service stored it. */
typedef platinum_conversations_result (*platinum_send_message_fn)(
    void *ctx, const char *convo_id, const char *text,
    platinum_conversation_message *out);

/* Release the rows and reset the page. NULL-safe. */
void platinum_conversations_free(platinum_conversations *out);
void platinum_conversation_messages_free(platinum_conversation_messages *out);

/* A conversation id as the routes accept it: 1 to 128 printable, non-space ASCII
 * characters. Returns 1 for a valid id, 0 otherwise. */
int platinum_valid_convo_id(const char *id);

/* GET /v1/conversations. */
int platinum_route_conversations(void *ctx, platinum_conversations_fetch_fn fetch,
                                 const char *limit_param, const char *cursor_param,
                                 char *body, size_t cap);

/* GET /v1/conversation?id=... */
int platinum_route_conversation(void *ctx, platinum_messages_fetch_fn fetch,
                                const char *convo_id, const char *limit_param,
                                const char *cursor_param, char *body, size_t cap);

/* POST /v1/conversation/message, with the already-parsed convoId and text. */
int platinum_route_conversation_message(void *ctx, platinum_send_message_fn send,
                                        const char *convo_id, const char *text,
                                        char *body, size_t cap);

#endif
