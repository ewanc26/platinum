/*
 * conversations_feed.c -- the Conversations window's protocol half: listing,
 * paging, opening a conversation and sending into it. No QuickDraw here, so it
 * links on a host for tests; conversations.c is the window.
 *
 * Every value is copied out of the response before it is freed, and every copy
 * is bounded by the field's own capacity. A response that does not have the
 * documented shape is reported rather than shown as an empty list: those are
 * different facts, and conflating them hides a broken bridge.
 */
#include "conversations.h"
#include "json_min.h"
#include "text_codec.h"

#include <string.h>

/* README prose is UTF-8 on the wire and MacRoman on screen. */
static void conversations_copy(char *destination,
                               long capacity,
                               platinum_json object,
                               const char *name)
{
    char utf8[PLATINUM_TEXT_UTF8_CAPACITY];
    long length;

    if (destination == NULL || capacity <= 0)
        return;

    destination[0] = '\0';
    if (platinum_json_string_truncating(object, name, utf8, sizeof(utf8))
        != WF_OK)
        return;

    length = platinum_text_utf8_to_macroman(utf8, destination, capacity, NULL);
    if (length < 0)
        destination[0] = '\0';
}

/*
 * Copy a string member exactly as sent or leave it empty. Used for identifiers
 * the bridge will be asked about again: half an identifier is worse than none,
 * because it would address a different conversation.
 */
static void conversations_copy_exact(char *destination,
                                     long capacity,
                                     platinum_json object,
                                     const char *name)
{
    if (destination == NULL || capacity <= 0)
        return;

    destination[0] = '\0';
    if (platinum_json_string(object, name, destination, (size_t)capacity)
        != WF_OK)
        destination[0] = '\0';
}

/* RFC 3339 puts the clock time at a fixed offset after the date's "T". */
static void conversations_time(char *destination,
                               long capacity,
                               platinum_json object,
                               const char *name)
{
    char raw[64];

    if (destination == NULL || capacity <= 0)
        return;

    destination[0] = '\0';
    if (platinum_json_string_truncating(object, name, raw, sizeof(raw))
        != WF_OK)
        return;

    if (strlen(raw) >= 16 && capacity >= 6) {
        memcpy(destination, raw + 11, 5);
        destination[5] = '\0';
    } else {
        long length = platinum_text_utf8_to_macroman(raw, destination,
                                                     capacity, NULL);
        if (length < 0)
            destination[0] = '\0';
    }
}

static void conversations_status(platinum_conversations *conversations,
                                 const char *status)
{
    long length;

    if (conversations == NULL)
        return;

    conversations->status[0] = '\0';
    if (status == NULL)
        return;

    length = (long)strlen(status);
    if (length > PLATINUM_CONVERSATIONS_STATUS_MAX)
        length = PLATINUM_CONVERSATIONS_STATUS_MAX;

    memcpy(conversations->status, status, (size_t)length);
    conversations->status[length] = '\0';
    /* No InvalRect here: this file draws nothing. The application invalidates
     * the window after every call that changes it. */
}

static int conversation_parse(platinum_conversation *conversation,
                              platinum_json value)
{
    char did[128];
    long unread;
    int group;

    if (conversation == NULL)
        return 0;

    memset(conversation, 0, sizeof(*conversation));

    /* The id is the only required member: everything else is presentation, and
     * a conversation with no id could not be opened. */
    conversations_copy_exact(conversation->id, sizeof(conversation->id), value,
                             "id");
    if (conversation->id[0] == '\0')
        return 0;

    conversations_copy(conversation->name, sizeof(conversation->name), value,
                       "name");
    if (conversation->name[0] == '\0') {
        /* The bridge names a one-to-one conversation after its other member
         * and a group "Group"; a row with neither still needs a label. */
        conversations_copy_exact(did, sizeof(did), value, "handle");
        if (did[0] != '\0')
            conversations_copy(conversation->name,
                               sizeof(conversation->name), value, "handle");
        else
            strcpy(conversation->name, "Conversation");
    }

    /* An @handle with no handle behind it would read as a bare "@". */
    conversation->handle[0] = '\0';
    conversations_copy(conversation->handle + 1,
                       sizeof(conversation->handle) - 1, value, "handle");
    if (conversation->handle[1] != '\0')
        conversation->handle[0] = '@';
    else
        conversation->handle[0] = '\0';

    conversations_copy(conversation->preview,
                       sizeof(conversation->preview), value, "preview");

    conversation->group = 0;
    if (platinum_json_bool(value, "group", &group) == WF_OK)
        conversation->group = group;

    conversation->unread = 0;
    if (platinum_json_int(value, "unread", &unread) == WF_OK && unread > 0)
        conversation->unread = unread;

    return 1;
}

static int message_parse(platinum_message *message, platinum_json value)
{
    int mine;

    if (message == NULL)
        return 0;

    memset(message, 0, sizeof(*message));

    conversations_copy_exact(message->id, sizeof(message->id), value, "id");
    conversations_copy(message->text, sizeof(message->text), value, "text");
    conversations_time(message->time, sizeof(message->time), value, "sentAt");

    message->mine = 0;
    if (platinum_json_bool(value, "mine", &mine) == WF_OK)
        message->mine = mine;

    /* A message with no text has nothing to draw, and a deleted one is left
     * out by the bridge: either way it is not a row. */
    return message->text[0] != '\0';
}

int platinum_conversations_has_older(const platinum_conversations *conversations)
{
    return conversations != NULL && conversations->list_cursor[0] != '\0' &&
           conversations->count > 0;
}

int platinum_conversations_has_older_messages(
    const platinum_conversations *conversations)
{
    return conversations != NULL && conversations->selected_id[0] != '\0' &&
           conversations->message_cursor[0] != '\0' &&
           conversations->message_count > 0;
}

static int conversations_index_of(const platinum_conversations *conversations,
                                  const char *id)
{
    unsigned short index;

    if (conversations == NULL || id == NULL || id[0] == '\0')
        return -1;

    for (index = 0; index < conversations->count; ++index) {
        if (strcmp(conversations->items[index].id, id) == 0)
            return (int)index;
    }

    return -1;
}

void platinum_conversations_close_conversation(
    platinum_conversations *conversations)
{
    if (conversations == NULL)
        return;

    conversations->selected_id[0] = '\0';
    conversations->selected_name[0] = '\0';
    conversations->message_count = 0;
    conversations->message_row = 0;
    conversations->message_cursor[0] = '\0';
    conversations_status(conversations, "Choose a conversation from the list.");
}

void platinum_conversations_init(platinum_conversations *conversations)
{
    if (conversations == NULL)
        return;

    memset(conversations, 0, sizeof(*conversations));
    conversations_status(conversations, "No conversations loaded.");
}

wf_status platinum_conversations_refresh(
    platinum_conversations *conversations,
    platinum_bridge_client *bridge)
{
    wf_response response;
    platinum_json root;
    platinum_json items;
    platinum_json value;
    long available;
    long index;
    char cursor[PLATINUM_CONVERSATIONS_CURSOR_MAX];
    wf_status status;

    if (conversations == NULL || bridge == NULL)
        return WF_ERR_INVALID_ARG;

    conversations->count = 0;
    conversations->list_row = 0;
    conversations->list_cursor[0] = '\0';

    memset(&response, 0, sizeof(response));
    conversations->loading = 1;
    conversations_status(conversations, "Loading conversations...");

    status = platinum_bridge_get(bridge,
                                 "/v1/conversations?limit=20", &response);
    if (status != WF_OK) {
        conversations->loading = 0;
        if (status == WF_ERR_AUTH)
            conversations_status(
                conversations,
                "Session expired. Pair the account again.");
        else
            conversations_status(conversations, "Conversation refresh failed.");
        wf_response_free(&response);
        return status;
    }

    if (platinum_json_open(&root, response.body != NULL ? response.body : "")
            != WF_OK ||
        platinum_json_member(root, "conversations", &items) != WF_OK ||
        platinum_json_count(items, &available) != WF_OK) {
        conversations->loading = 0;
        conversations_status(conversations,
                             "The bridge returned an invalid conversation list.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    if (available > PLATINUM_CONVERSATIONS_MAX)
        available = PLATINUM_CONVERSATIONS_MAX;

    for (index = 0; index < available; ++index) {
        if (platinum_json_element(items, index, &value) != WF_OK)
            continue;
        if (conversation_parse(&conversations->items[conversations->count],
                               value))
            ++conversations->count;
    }

    /* A cursor is copied exactly or not at all. */
    if (platinum_json_string(root, "cursor", cursor, sizeof(cursor)) == WF_OK)
        strcpy(conversations->list_cursor, cursor);
    else
        conversations->list_cursor[0] = '\0';

    wf_response_free(&response);
    conversations->loading = 0;

    /* The open conversation can have been left since the last refresh. */
    if (conversations->selected_id[0] != '\0' &&
        conversations_index_of(conversations, conversations->selected_id) < 0)
        platinum_conversations_close_conversation(conversations);

    if (conversations->count == 0)
        conversations_status(conversations, "No conversations.");
    else if (conversations->selected_id[0] == '\0')
        conversations_status(conversations,
                             "Choose a conversation from the list.");

    return WF_OK;
}

wf_status platinum_conversations_load_older(
    platinum_conversations *conversations,
    platinum_bridge_client *bridge,
    unsigned short *dropped)
{
    wf_response response;
    platinum_json root;
    platinum_json items;
    platinum_json value;
    long available;
    long index;
    long drop;
    char path[128 + 3 * PLATINUM_CONVERSATIONS_CURSOR_MAX];
    char cursor[PLATINUM_CONVERSATIONS_CURSOR_MAX];
    wf_status status;

    if (dropped != NULL)
        *dropped = 0;

    if (conversations == NULL || bridge == NULL ||
        !platinum_conversations_has_older(conversations))
        return WF_ERR_INVALID_ARG;

    strcpy(path, "/v1/conversations?limit=20&cursor=");
    if (platinum_bridge_query_escape(conversations->list_cursor,
                                     path + strlen(path),
                                     (long)(sizeof(path) - strlen(path))) < 0)
        return WF_ERR_INVALID_ARG;

    memset(&response, 0, sizeof(response));
    conversations->loading = 1;
    conversations_status(conversations, "Loading older conversations...");

    status = platinum_bridge_get(bridge, path, &response);
    if (status != WF_OK) {
        conversations->loading = 0;
        conversations_status(conversations,
                             "Could not load older conversations.");
        wf_response_free(&response);
        return status;
    }

    if (platinum_json_open(&root, response.body != NULL ? response.body : "")
            != WF_OK ||
        platinum_json_member(root, "conversations", &items) != WF_OK ||
        platinum_json_count(items, &available) != WF_OK) {
        conversations->loading = 0;
        conversations_status(conversations,
                             "The bridge returned an invalid conversation list.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    if (available > PLATINUM_CONVERSATIONS_PAGE)
        available = PLATINUM_CONVERSATIONS_PAGE;

    /* Known good, so only now drop the newest rows to stay within the cap. */
    drop = (long)conversations->count + available - PLATINUM_CONVERSATIONS_MAX;
    if (drop > (long)conversations->count)
        drop = (long)conversations->count;
    if (drop > 0) {
        memmove(&conversations->items[0], &conversations->items[drop],
                (size_t)((long)conversations->count - drop) *
                    sizeof(conversations->items[0]));
        conversations->count =
            (unsigned short)((long)conversations->count - drop);
        if (dropped != NULL)
            *dropped = (unsigned short)drop;
    }

    for (index = 0; index < available &&
                    conversations->count < PLATINUM_CONVERSATIONS_MAX;
         ++index) {
        if (platinum_json_element(items, index, &value) != WF_OK)
            continue;
        if (conversation_parse(&conversations->items[conversations->count],
                               value))
            ++conversations->count;
    }

    if (platinum_json_string(root, "cursor", cursor, sizeof(cursor)) == WF_OK)
        strcpy(conversations->list_cursor, cursor);
    else
        conversations->list_cursor[0] = '\0';

    wf_response_free(&response);
    conversations->loading = 0;

    if (conversations->selected_id[0] != '\0' &&
        conversations_index_of(conversations, conversations->selected_id) < 0)
        platinum_conversations_close_conversation(conversations);

    return WF_OK;
}

/* One page of messages for the open conversation, replacing or appending. */
static wf_status conversations_fetch_messages(
    platinum_conversations *conversations,
    platinum_bridge_client *bridge,
    int append,
    unsigned short *dropped)
{
    wf_response response;
    platinum_json root;
    platinum_json items;
    platinum_json value;
    long available;
    long index;
    long drop;
    char path[128 + 3 * PLATINUM_CONVERSATION_ID_MAX +
              3 * PLATINUM_CONVERSATIONS_CURSOR_MAX];
    char cursor[PLATINUM_CONVERSATIONS_CURSOR_MAX];
    wf_status status;

    if (dropped != NULL)
        *dropped = 0;

    if (conversations == NULL || bridge == NULL ||
        conversations->selected_id[0] == '\0')
        return WF_ERR_INVALID_ARG;

    strcpy(path, "/v1/conversation?id=");
    if (platinum_bridge_query_escape(conversations->selected_id,
                                     path + strlen(path),
                                     (long)(sizeof(path) - strlen(path))) < 0)
        return WF_ERR_INVALID_ARG;

    strcat(path, "&limit=20");
    if (append) {
        strcat(path, "&cursor=");
        if (platinum_bridge_query_escape(conversations->message_cursor,
                                         path + strlen(path),
                                         (long)(sizeof(path) - strlen(path)))
            < 0)
            return WF_ERR_INVALID_ARG;
    } else {
        conversations->message_count = 0;
        conversations->message_row = 0;
        conversations->message_cursor[0] = '\0';
    }

    memset(&response, 0, sizeof(response));
    conversations->loading = 1;
    conversations_status(conversations,
                         append ? "Loading older messages..."
                                : "Loading messages...");

    status = platinum_bridge_get(bridge, path, &response);
    if (status != WF_OK) {
        conversations->loading = 0;
        if (status == WF_ERR_AUTH)
            conversations_status(
                conversations,
                "Session expired. Pair the account again.");
        else
            conversations_status(conversations,
                                 append ? "Could not load older messages."
                                        : "Could not load the conversation.");
        wf_response_free(&response);
        return status;
    }

    if (platinum_json_open(&root, response.body != NULL ? response.body : "")
            != WF_OK ||
        platinum_json_member(root, "messages", &items) != WF_OK ||
        platinum_json_count(items, &available) != WF_OK) {
        conversations->loading = 0;
        conversations_status(conversations,
                             "The bridge returned an invalid message list.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    if (available > PLATINUM_CONVERSATIONS_PAGE)
        available = PLATINUM_CONVERSATIONS_PAGE;

    drop = (long)conversations->message_count + available - PLATINUM_MESSAGES_MAX;
    if (drop > (long)conversations->message_count)
        drop = (long)conversations->message_count;
    if (drop > 0) {
        memmove(&conversations->messages[0], &conversations->messages[drop],
                (size_t)((long)conversations->message_count - drop) *
                    sizeof(conversations->messages[0]));
        conversations->message_count =
            (unsigned short)((long)conversations->message_count - drop);
        if (dropped != NULL)
            *dropped = (unsigned short)drop;
    }

    for (index = 0; index < available &&
                    conversations->message_count < PLATINUM_MESSAGES_MAX;
         ++index) {
        if (platinum_json_element(items, index, &value) != WF_OK)
            continue;
        if (message_parse(
                &conversations->messages[conversations->message_count], value))
            ++conversations->message_count;
    }

    if (platinum_json_string(root, "cursor", cursor, sizeof(cursor)) == WF_OK)
        strcpy(conversations->message_cursor, cursor);
    else
        conversations->message_cursor[0] = '\0';

    wf_response_free(&response);
    conversations->loading = 0;

    if (conversations->message_count == 0)
        conversations_status(conversations, "No messages yet. Type one below.");
    else
        conversations_status(conversations,
                             "Return opens a conversation. Command-R refreshes.");

    return WF_OK;
}

wf_status platinum_conversations_open(platinum_conversations *conversations,
                                      platinum_bridge_client *bridge,
                                      unsigned short index)
{
    const platinum_conversation *chosen;

    if (conversations == NULL || bridge == NULL ||
        index >= conversations->count)
        return WF_ERR_INVALID_ARG;

    chosen = &conversations->items[index];
    strcpy(conversations->selected_id, chosen->id);
    strcpy(conversations->selected_name, chosen->name);
    conversations->message_count = 0;
    conversations->message_row = 0;
    conversations->message_cursor[0] = '\0';

    return conversations_fetch_messages(conversations, bridge, 0, NULL);
}

wf_status platinum_conversations_open_selected(
    platinum_conversations *conversations,
    platinum_bridge_client *bridge,
    short index)
{
    if (conversations == NULL)
        return WF_ERR_INVALID_ARG;

    if (index < 0)
        index = conversations->list_row;

    if (index < 0 || index >= (short)conversations->count)
        return WF_ERR_INVALID_ARG;

    return platinum_conversations_open(conversations, bridge,
                                       (unsigned short)index);
}

wf_status platinum_conversations_load_older_messages(
    platinum_conversations *conversations,
    platinum_bridge_client *bridge,
    unsigned short *dropped)
{
    if (dropped != NULL)
        *dropped = 0;

    if (conversations == NULL || bridge == NULL ||
        !platinum_conversations_has_older_messages(conversations))
        return WF_ERR_INVALID_ARG;

    return conversations_fetch_messages(conversations, bridge, 1, dropped);
}

wf_status platinum_conversations_send(platinum_conversations *conversations,
                                      platinum_bridge_client *bridge,
                                      const char *utf8_text)
{
    static char body[PLATINUM_CONVERSATION_SEND_MAX * 6 + 1024];
    static char path[64 + 3 * PLATINUM_CONVERSATION_ID_MAX];
    static char id[PLATINUM_CONVERSATION_ID_MAX * 6 + 1];
    static char text[PLATINUM_CONVERSATION_SEND_MAX * 6 + 1];
    wf_response response;
    platinum_json root;
    platinum_message sent;
    long length;
    int anything;
    wf_status status;

    if (conversations == NULL || bridge == NULL)
        return WF_ERR_INVALID_ARG;

    if (conversations->selected_id[0] == '\0') {
        conversations_status(conversations,
                             "Choose a conversation before sending.");
        return WF_ERR_INVALID_ARG;
    }

    if (utf8_text == NULL) {
        conversations_status(conversations, "Type a message first.");
        return WF_ERR_INVALID_ARG;
    }

    length = (long)strlen(utf8_text);
    anything = 0;
    {
        long index;
        for (index = 0; index < length; ++index) {
            if (utf8_text[index] != ' ' && utf8_text[index] != '\t' &&
                utf8_text[index] != '\r' && utf8_text[index] != '\n') {
                anything = 1;
                break;
            }
        }
    }

    if (!anything) {
        conversations_status(conversations, "Type a message first.");
        return WF_ERR_INVALID_ARG;
    }

    if (length > PLATINUM_CONVERSATION_SEND_MAX) {
        conversations_status(conversations, "That message is too long.");
        return WF_ERR_INVALID_ARG;
    }

    /* The body is built once, here, and never holds anything but the two
     * escaped strings the bridge asks for. */
    if (platinum_json_escape(id, sizeof(id), conversations->selected_id)
            != WF_OK ||
        platinum_json_escape(text, sizeof(text), utf8_text) != WF_OK) {
        conversations_status(conversations,
                             "That message is too long to send.");
        return WF_ERR_INVALID_ARG;
    }

    if (strlen(id) + strlen(text) + 32 > sizeof(body)) {
        conversations_status(conversations,
                             "That message is too long to send.");
        return WF_ERR_INVALID_ARG;
    }

    strcpy(body, "{\"convoId\":\"");
    strcat(body, id);
    strcat(body, "\",\"text\":\"");
    strcat(body, text);
    strcat(body, "\"}");
    strcpy(path, "/v1/conversation/message");

    memset(&response, 0, sizeof(response));
    conversations->sending = 1;
    conversations_status(conversations, "Sending...");

    status = platinum_bridge_post(bridge, path, body, &response);
    if (status != WF_OK) {
        conversations->sending = 0;
        if (status == WF_ERR_AUTH)
            conversations_status(
                conversations,
                "Session expired. Pair the account again.");
        else
            conversations_status(conversations, "The message was not sent.");
        wf_response_free(&response);
        return status;
    }

    /* The answer is the stored message. Reading it is optional: the send
     * already happened, so a body this client does not understand is reported
     * as sent and simply is not added to the list. */
    if (platinum_json_open(&root, response.body != NULL ? response.body : "")
            == WF_OK &&
        message_parse(&sent, root)) {
        if (conversations->message_count < PLATINUM_MESSAGES_MAX)
            ++conversations->message_count;
        if (conversations->message_count > 1) {
            memmove(&conversations->messages[1], &conversations->messages[0],
                    (size_t)(conversations->message_count - 1) *
                        sizeof(conversations->messages[0]));
        }
        conversations->messages[0] = sent;
    }

    wf_response_free(&response);
    conversations->sending = 0;
    conversations_status(conversations, "Sent.");

    return WF_OK;
}
