/* The conversation routes: validation, status codes and the JSON the Mac reads. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conversations.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static platinum_conversations_result
fake_convos(void *ctx, int limit, const char *cursor, platinum_conversations *out)
{
    platinum_conversation *rows;
    const char *mode = (const char *) ctx;

    (void) cursor;
    if (mode != NULL && strcmp(mode, "error") == 0) return PLATINUM_CONVERSATIONS_ERROR;
    if (mode != NULL && strcmp(mode, "huge") == 0) {
        out->count = PLATINUM_CONVERSATIONS_ROWS_MAX + 1;
        return PLATINUM_CONVERSATIONS_OK;
    }
    if (limit <= 0) return PLATINUM_CONVERSATIONS_ERROR;

    rows = calloc(2, sizeof *rows);
    if (rows == NULL) return PLATINUM_CONVERSATIONS_ERROR;
    strcpy(rows[0].id, "convo-direct");
    strcpy(rows[0].name, "Bob");
    strcpy(rows[0].handle, "@bob.test");
    strcpy(rows[0].avatar, "avatar_thumbnail/plain/did:plc:bob/bafy@png");
    strcpy(rows[0].preview, "hello there");
    rows[0].unread = 2;
    strcpy(rows[1].id, "convo-group");
    strcpy(rows[1].name, "Group");
    rows[1].group = 1;
    strcpy(rows[1].preview, "Weekend plans");
    rows[1].unread = 0;

    out->items = rows;
    out->count = 2;
    strcpy(out->cursor, "next-page");
    out->has_cursor = 1;
    return PLATINUM_CONVERSATIONS_OK;
}

static platinum_conversations_result
fake_empty(void *ctx, int limit, const char *cursor, platinum_conversations *out)
{
    (void) ctx; (void) limit; (void) cursor; (void) out;
    return PLATINUM_CONVERSATIONS_OK;
}

static platinum_conversations_result
fake_messages(void *ctx, const char *convo_id, int limit, const char *cursor,
              platinum_conversation_messages *out)
{
    platinum_conversation_message *rows;
    (void) ctx; (void) limit; (void) cursor;

    if (strcmp(convo_id, "convo-direct") != 0) return PLATINUM_CONVERSATIONS_ERROR;
    rows = calloc(2, sizeof *rows);
    if (rows == NULL) return PLATINUM_CONVERSATIONS_ERROR;
    strcpy(rows[0].id, "m2");
    strcpy(rows[0].text, "mine");
    rows[0].mine = 1;
    strcpy(rows[0].sent_at, "2026-10-02T10:00:00.000Z");
    strcpy(rows[1].id, "m1");
    strcpy(rows[1].text, "theirs");
    strcpy(rows[1].sent_at, "2026-10-01T10:00:00.000Z");
    out->items = rows;
    out->count = 2;
    return PLATINUM_CONVERSATIONS_OK;
}

static platinum_conversations_result
fake_send(void *ctx, const char *convo_id, const char *text,
          platinum_conversation_message *out)
{
    (void) ctx; (void) convo_id;
    memset(out, 0, sizeof *out);
    strcpy(out->id, "m3");
    strcpy(out->text, text);
    strcpy(out->sent_at, "2026-10-03T10:00:00.000Z");
    out->mine = 1;
    return PLATINUM_CONVERSATIONS_OK;
}

static void
test_convo_id(void)
{
    char long_id[200];

    CHECK(platinum_valid_convo_id("convo-1"));
    CHECK(platinum_valid_convo_id("2l6q3x7y"));
    CHECK(!platinum_valid_convo_id(NULL));
    CHECK(!platinum_valid_convo_id(""));
    CHECK(!platinum_valid_convo_id("has space"));
    CHECK(!platinum_valid_convo_id("tab\there"));
    memset(long_id, 'a', sizeof long_id);
    long_id[PLATINUM_CONVO_ID_MAX - 1] = '\0';
    CHECK(platinum_valid_convo_id(long_id));
    long_id[PLATINUM_CONVO_ID_MAX - 1] = 'a';
    long_id[PLATINUM_CONVO_ID_MAX] = '\0';
    CHECK(!platinum_valid_convo_id(long_id));
}

static void
test_conversations(void)
{
    char body[8192];
    char long_cursor[300];

    CHECK(platinum_route_conversations(NULL, fake_convos, NULL, NULL, body, sizeof body) == 200);
    CHECK(strstr(body, "\"conversations\":[") != NULL);
    CHECK(strstr(body, "\"id\":\"convo-direct\"") != NULL);
    CHECK(strstr(body, "\"name\":\"Bob\"") != NULL);
    CHECK(strstr(body, "\"handle\":\"@bob.test\"") != NULL);
    CHECK(strstr(body, "\"avatar\":\"avatar_thumbnail/plain/did:plc:bob/bafy@png\"") != NULL);
    CHECK(strstr(body, "\"preview\":\"hello there\"") != NULL);
    CHECK(strstr(body, "\"unread\":2") != NULL);
    CHECK(strstr(body, "\"cursor\":\"next-page\"") != NULL);
    /* A group has a name and members, not a handle or an avatar. */
    CHECK(strstr(body, "\"name\":\"Group\"") != NULL);
    {
        char *group = strstr(body, "\"id\":\"convo-group\"");
        CHECK(group != NULL);
        if (group != NULL) {
            CHECK(strstr(group, "\"handle\"") == NULL || strstr(group, "\"handle\"") > strstr(group, "}"));
        }
    }

    CHECK(platinum_route_conversations(NULL, fake_empty, NULL, NULL, body, sizeof body) == 200);
    CHECK(strcmp(body, "{\"conversations\":[]}") == 0);

    memset(long_cursor, 'c', sizeof long_cursor);
    long_cursor[sizeof long_cursor - 1] = '\0';
    CHECK(platinum_route_conversations(NULL, fake_convos, NULL, long_cursor, body, sizeof body) == 400);
    CHECK(strstr(body, "invalid_cursor") != NULL);

    CHECK(platinum_route_conversations((void *) "error", fake_convos, NULL, NULL, body, sizeof body) == 500);
    CHECK(strstr(body, "bridge_error") != NULL);

    CHECK(platinum_route_conversations((void *) "huge", fake_convos, NULL, NULL, body, sizeof body) == 500);
}

static void
test_conversation(void)
{
    char body[8192];
    char long_cursor[300];

    CHECK(platinum_route_conversation(NULL, fake_messages, "convo-direct", NULL, NULL,
                                       body, sizeof body) == 200);
    CHECK(strstr(body, "\"messages\":[") != NULL);
    CHECK(strstr(body, "\"id\":\"m2\"") != NULL);
    CHECK(strstr(body, "\"mine\":true") != NULL);
    CHECK(strstr(body, "\"sentAt\":\"2026-10-02T10:00:00.000Z\"") != NULL);

    CHECK(platinum_route_conversation(NULL, fake_messages, NULL, NULL, NULL, body, sizeof body) == 400);
    CHECK(strstr(body, "invalid_conversation") != NULL);
    CHECK(platinum_route_conversation(NULL, fake_messages, "has space", NULL, NULL,
                                      body, sizeof body) == 400);

    memset(long_cursor, 'c', sizeof long_cursor);
    long_cursor[sizeof long_cursor - 1] = '\0';
    CHECK(platinum_route_conversation(NULL, fake_messages, "convo-direct", NULL, long_cursor,
                                      body, sizeof body) == 400);
    CHECK(strstr(body, "invalid_cursor") != NULL);
}

static void
test_send(void)
{
    char body[8192];
    char long_text[1100];

    CHECK(platinum_route_conversation_message(NULL, fake_send, "convo-direct", "hello",
                                              body, sizeof body) == 200);
    CHECK(strstr(body, "\"text\":\"hello\"") != NULL);
    CHECK(strstr(body, "\"mine\":true") != NULL);

    CHECK(platinum_route_conversation_message(NULL, fake_send, NULL, "hello",
                                              body, sizeof body) == 400);
    CHECK(strstr(body, "invalid_conversation") != NULL);

    CHECK(platinum_route_conversation_message(NULL, fake_send, "convo-direct", "   \t\n",
                                              body, sizeof body) == 400);
    CHECK(strstr(body, "missing_text") != NULL);

    memset(long_text, 'a', sizeof long_text);
    long_text[1001] = '\0';
    CHECK(platinum_route_conversation_message(NULL, fake_send, "convo-direct", long_text,
                                              body, sizeof body) == 400);
    CHECK(strstr(body, "text_too_long") != NULL);

    long_text[1000] = '\0';
    CHECK(platinum_route_conversation_message(NULL, fake_send, "convo-direct", long_text,
                                              body, sizeof body) == 200);
}

int
main(void)
{
    test_convo_id();
    test_conversations();
    test_conversation();
    test_send();

    if (failures) {
        fprintf(stderr, "test_conversations: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_conversations: %d checks passed\n", checks);
    return 0;
}
