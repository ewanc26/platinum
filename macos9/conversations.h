#ifndef PLATINUM_CONVERSATIONS_H
#define PLATINUM_CONVERSATIONS_H

#include "bridge_client.h"
#include "scrollbar.h"
#include "textfield.h"

#include <Events.h>
#include <TextEdit.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The Conversations window: one list of conversations and, for the one chosen,
 * its messages and a one-line composer. conversations_feed.c is the protocol
 * half (no QuickDraw, so it links on a host for tests); conversations.c is the
 * window that draws it.
 *
 * The bridge caps a conversation list page at 50 and a message page at 20; the
 * caps here are what the window keeps, so a long session cannot grow the
 * application's globals without bound.
 */
#define PLATINUM_CONVERSATIONS_MAX 20
#define PLATINUM_CONVERSATIONS_PAGE 20
#define PLATINUM_CONVERSATION_ID_MAX 128
#define PLATINUM_CONVERSATION_NAME_MAX 64
#define PLATINUM_CONVERSATION_HANDLE_MAX 64
#define PLATINUM_CONVERSATION_PREVIEW_MAX 96
#define PLATINUM_CONVERSATIONS_CURSOR_MAX 256
#define PLATINUM_CONVERSATIONS_STATUS_MAX 127

#define PLATINUM_MESSAGES_MAX 30
#define PLATINUM_MESSAGE_ID_MAX 128
#define PLATINUM_MESSAGE_TEXT_MAX 400
#define PLATINUM_MESSAGE_TIME_MAX 8

/* The bridge refuses text over 1000 characters; a message is typed to that
 * limit and no further. */
#define PLATINUM_CONVERSATION_SEND_MAX 1000

typedef struct platinum_conversation {
    char id[PLATINUM_CONVERSATION_ID_MAX];
    char name[PLATINUM_CONVERSATION_NAME_MAX];
    /* "@handle" including the @, empty for a group. */
    char handle[PLATINUM_CONVERSATION_HANDLE_MAX];
    char preview[PLATINUM_CONVERSATION_PREVIEW_MAX];
    int group;
    long unread;
} platinum_conversation;

typedef struct platinum_message {
    char id[PLATINUM_MESSAGE_ID_MAX];
    char text[PLATINUM_MESSAGE_TEXT_MAX];
    /* 1 for the signed-in account's own message, as the bridge said. */
    int mine;
    /* Clock time of sentAt, or the raw value when it is not that shape. */
    char time[PLATINUM_MESSAGE_TIME_MAX];
} platinum_message;

typedef struct platinum_conversations {
    WindowPtr window;
    platinum_scrollbar list_scrollbar;
    platinum_scrollbar message_scrollbar;
    TEHandle composer;

    platinum_conversation items[PLATINUM_CONVERSATIONS_MAX];
    unsigned short count;
    short list_row;
    char list_cursor[PLATINUM_CONVERSATIONS_CURSOR_MAX];

    platinum_message messages[PLATINUM_MESSAGES_MAX];
    unsigned short message_count;
    short message_row;
    char message_cursor[PLATINUM_CONVERSATIONS_CURSOR_MAX];
    /* Empty when no conversation is open. */
    char selected_id[PLATINUM_CONVERSATION_ID_MAX];
    char selected_name[PLATINUM_CONVERSATION_NAME_MAX];

    int composer_focused;
    int loading;
    int sending;
    char status[PLATINUM_CONVERSATIONS_STATUS_MAX + 1];
} platinum_conversations;

enum {
    PLATINUM_CONVERSATIONS_NONE = 0,
    PLATINUM_CONVERSATIONS_CLOSE = 1,
    PLATINUM_CONVERSATIONS_REFRESH = 2,
    PLATINUM_CONVERSATIONS_LOAD_OLDER = 3,
    PLATINUM_CONVERSATIONS_OPEN = 4,
    PLATINUM_CONVERSATIONS_SEND = 5,
    PLATINUM_CONVERSATIONS_OLDER_MESSAGES = 6
};

void platinum_conversations_init(platinum_conversations *conversations);
wf_status platinum_conversations_refresh(platinum_conversations *conversations,
                                         platinum_bridge_client *bridge);
/* Append the next page of conversations; on success `dropped` rows left the
 * front. A failed page keeps what was loaded. WF_ERR_INVALID_ARG when nothing
 * is older. */
wf_status platinum_conversations_load_older(platinum_conversations *conversations,
                                            platinum_bridge_client *bridge,
                                            unsigned short *dropped);
int platinum_conversations_has_older(const platinum_conversations *conversations);

/* Open the conversation at `index` and load its newest page of messages. On
 * failure the previous selection and messages are kept. */
wf_status platinum_conversations_open(platinum_conversations *conversations,
                                      platinum_bridge_client *bridge,
                                      unsigned short index);
void platinum_conversations_close_conversation(platinum_conversations *conversations);

/* The next older page of the open conversation. */
wf_status platinum_conversations_load_older_messages(
    platinum_conversations *conversations,
    platinum_bridge_client *bridge,
    unsigned short *dropped);
int platinum_conversations_has_older_messages(
    const platinum_conversations *conversations);

/* Negative `index` chooses the conversation the list's cursor is on. */
wf_status platinum_conversations_open_selected(
    platinum_conversations *conversations,
    platinum_bridge_client *bridge,
    short index);

/*
 * Send `utf8_text` to the open conversation. Refused locally (no request) for
 * no open conversation, empty or all-space text, or more than
 * PLATINUM_CONVERSATION_SEND_MAX characters. On success the stored message is
 * put at the top of the message list.
 */
wf_status platinum_conversations_send(platinum_conversations *conversations,
                                      platinum_bridge_client *bridge,
                                      const char *utf8_text);

OSErr platinum_conversations_open_window(platinum_conversations *conversations);
void platinum_conversations_close_window(platinum_conversations *conversations);
int platinum_conversations_handle_event(platinum_conversations *conversations,
                                        EventRecord *event);
void platinum_conversations_draw(platinum_conversations *conversations);

/* The text in the composer, as a C string; 0 when it does not fit. */
long platinum_conversations_composer_text(const platinum_conversations *conversations,
                                          char *buffer,
                                          long capacity);
void platinum_conversations_clear_composer(platinum_conversations *conversations);
/* Set the status line and redraw it. */
void platinum_conversations_set_status(platinum_conversations *conversations,
                                       const char *status);

#ifdef __cplusplus
}
#endif

#endif
