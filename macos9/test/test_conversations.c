/*
 * test_conversations.c -- host-side tests for the conversation feed: listing,
 * paging, opening a conversation and sending into it, over the stubbed Wolfram
 * transport. Not Classic Mac OS 9 validation.
 */

#include <stdio.h>
#include <string.h>

#include "conversations.h"
#include "wolfram_stub.h"

static int failures;
static int checks;
static char page[32768];

static void check(int condition, const char *what)
{
    checks++;
    if (!condition) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static void respond(int status, const char *body)
{
    fake_status = status;
    fake_http_status = status == WF_OK ? 200 : 500;
    fake_body = body;
    last_url[0] = '\0';
    last_body[0] = '\0';
    last_method = 0;
}

static const char *conversations_page(int first, int n, const char *cursor)
{
    int i;
    char item[320];

    strcpy(page, "{\"conversations\":[");
    for (i = 0; i < n; ++i) {
        sprintf(item,
                "%s{\"id\":\"convo-%d\",\"name\":\"Person %d\","
                "\"handle\":\"person%d.example\",\"group\":false,"
                "\"preview\":\"See you Saturday.\",\"unread\":%d}",
                i == 0 ? "" : ",", first + i, first + i, first + i,
                i == 0 ? 2 : 0);
        strcat(page, item);
    }
    strcat(page, "]");
    if (cursor != NULL) {
        strcat(page, ",\"cursor\":\"");
        strcat(page, cursor);
        strcat(page, "\"");
    }
    strcat(page, "}");
    return page;
}

static const char *messages_page(const char *cursor)
{
    strcpy(page,
           "{\"messages\":["
           "{\"id\":\"m2\",\"text\":\"See you Saturday\",\"mine\":true,"
           "\"sentAt\":\"2026-10-01T09:05:00.000Z\"},"
           "{\"id\":\"m1\",\"text\":\"Hello\",\"mine\":false,"
           "\"sentAt\":\"2026-10-01T09:00:00.000Z\"}"
           "]");
    if (cursor != NULL) {
        strcat(page, ",\"cursor\":\"");
        strcat(page, cursor);
        strcat(page, "\"");
    }
    strcat(page, "}");
    return page;
}

static void test_list_and_paging(void)
{
    static platinum_conversations c;
    platinum_bridge_client *bridge;
    unsigned short dropped;

    bridge = platinum_bridge_client_new("https://bridge.example");
    platinum_conversations_init(&c);
    check(strcmp(c.status, "No conversations loaded.") == 0, "starts empty");

    respond(WF_OK, conversations_page(0, 20, "next"));
    check(platinum_conversations_refresh(&c, bridge) == WF_OK, "first page loads");
    check(c.count == 20 && platinum_conversations_has_older(&c),
          "20 rows and a cursor");
    check(strcmp(c.items[0].name, "Person 0") == 0, "name read");
    check(strcmp(c.items[0].handle, "@person0.example") == 0, "handle gains its @");
    check(c.items[0].unread == 2 && c.items[1].unread == 0, "unread count read");
    check(c.items[0].group == 0, "group flag read");
    check(strcmp(c.items[0].preview, "See you Saturday.") == 0, "preview read");

    respond(WF_OK, conversations_page(20, 20, "c2"));
    check(platinum_conversations_load_older(&c, bridge, &dropped) == WF_OK,
          "second page loads");
    check(strstr(last_url, "cursor=next") != NULL, "cursor sent");
    check(c.count == 20 && dropped == 20, "cap of 20 holds");
    check(strcmp(c.items[0].id, "convo-20") == 0,
          "newest rows leave the front");
    check(strcmp(c.list_cursor, "c2") == 0, "cursor moved on");

    respond(WF_OK, conversations_page(0, 0, NULL));
    check(platinum_conversations_load_older(&c, bridge, &dropped) == WF_OK,
          "empty older page is not an error");
    check(c.count == 20 && dropped == 0 && !platinum_conversations_has_older(&c),
          "a page without a cursor ends paging");

    respond(WF_OK, "{\"conversations\":[{\"name\":\"No id\"}]}");
    check(platinum_conversations_refresh(&c, bridge) == WF_OK && c.count == 0,
          "a row with no id is not a conversation");
    check(strcmp(c.status, "No conversations.") == 0, "empty list says so");

    respond(WF_OK, "{}");
    check(platinum_conversations_refresh(&c, bridge) == WF_ERR_PARSE,
          "a body without the array is invalid, not empty");
    check(strstr(c.status, "invalid conversation list") != NULL,
          "and it says so");

    respond(WF_ERR_AUTH, "{}");
    check(platinum_conversations_refresh(&c, bridge) == WF_ERR_AUTH,
          "auth failure reported");
    check(strstr(c.status, "Session expired") != NULL, "auth read as expiry");

    respond(WF_ERR_HTTP, "{}");
    check(platinum_conversations_refresh(&c, bridge) != WF_OK, "http failure reported");
    check(strcmp(c.status, "Conversation refresh failed.") == 0, "and named");

    platinum_bridge_client_free(bridge);
}

static void test_open_and_send(void)
{
    static platinum_conversations c;
    platinum_bridge_client *bridge;
    unsigned short dropped;

    bridge = platinum_bridge_client_new("https://bridge.example");
    platinum_conversations_init(&c);

    respond(WF_OK, conversations_page(0, 2, "next"));
    check(platinum_conversations_refresh(&c, bridge) == WF_OK, "list for opening");

    respond(WF_OK, messages_page("older"));
    check(platinum_conversations_open_selected(&c, bridge, 1) == WF_OK,
          "the chosen conversation opens");
    check(strstr(last_url, "/v1/conversation?id=convo-1&limit=20") != NULL,
          "opened by id");
    check(c.message_count == 2, "two messages");
    check(strcmp(c.messages[0].text, "See you Saturday") == 0 && c.messages[0].mine,
          "newest first, mine read");
    check(strcmp(c.messages[0].time, "09:05") == 0, "clock time read");
    check(strcmp(c.messages[1].text, "Hello") == 0 && !c.messages[1].mine,
          "older message is not mine");
    check(platinum_conversations_has_older_messages(&c), "older page available");

    respond(WF_OK, messages_page(NULL));
    check(platinum_conversations_load_older_messages(&c, bridge, &dropped) == WF_OK,
          "older messages load");
    check(strstr(last_url, "cursor=older") != NULL, "message cursor sent");
    check(dropped == 0 && c.message_count == 4,
          "older messages append under the cap");
    check(strcmp(c.messages[0].id, "m2") == 0,
          "older pages append after the newest");
    check(!platinum_conversations_has_older_messages(&c),
          "a page without a cursor ends paging");

    check(platinum_conversations_open_selected(&c, bridge, 9) == WF_ERR_INVALID_ARG,
          "no such conversation is refused");

    respond(WF_OK, "{\"id\":\"m3\",\"text\":\"New one\",\"mine\":true,"
                   "\"sentAt\":\"2026-10-01T10:00:00.000Z\"}");
    check(platinum_conversations_send(&c, bridge, "  ") == WF_ERR_INVALID_ARG &&
              last_method == 0,
          "blank text is not sent");

    check(platinum_conversations_send(&c, bridge, "\"Hi\"\\") == WF_OK,
          "text is sent");
    check(strcmp(last_body,
                 "{\"convoId\":\"convo-1\",\"text\":\"\\\"Hi\\\"\\\\\"}")
              == 0,
          "body escapes the text and names the conversation");
    check(strcmp(c.messages[0].text, "New one") == 0,
          "the stored message goes to the top");
    check(strcmp(c.status, "Sent.") == 0, "send says it landed");

    {
        char too_long[PLATINUM_CONVERSATION_SEND_MAX + 2];
        memset(too_long, 'a', sizeof(too_long));
        too_long[sizeof(too_long) - 1] = '\0';
        respond(WF_OK, "{}");
        check(platinum_conversations_send(&c, bridge, too_long) == WF_ERR_INVALID_ARG &&
                  last_method == 0,
              "over-long text is refused locally");
        check(strstr(c.status, "too long") != NULL, "and says why");
    }

    respond(WF_ERR_AUTH, "{}");
    check(platinum_conversations_send(&c, bridge, "hello") == WF_ERR_AUTH,
          "auth failure on send reported");
    check(strstr(c.status, "Session expired") != NULL, "auth read as expiry");

    platinum_conversations_close_conversation(&c);
    check(c.selected_id[0] == '\0' && c.message_count == 0,
          "closing forgets the messages");
    respond(WF_OK, "{}");
    check(platinum_conversations_send(&c, bridge, "hello") == WF_ERR_INVALID_ARG &&
              last_method == 0,
          "nothing is sent without an open conversation");

    platinum_bridge_client_free(bridge);
}

int main(void)
{
    test_list_and_paging();
    test_open_and_send();
    if (failures != 0) {
        printf("test_conversations: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_conversations: all %d checks passed\n", checks);
    return 0;
}
