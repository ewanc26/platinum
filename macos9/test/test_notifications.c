/*
 * test_notifications.c -- host-side tests for the Notifications list: paging
 * and the seen mark. Links the real notifications_feed.c and bridge client
 * over the stubbed Wolfram transport. Not Classic Mac OS 9 validation.
 */

#include <stdio.h>
#include <string.h>

#include "notifications.h"
#include "wolfram_stub.h"

static int failures;
static int checks;
static char page[16384];

static void check(int condition, const char *what)
{
    checks++;
    if (!condition) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static const char *make_page(int first, int n, int unread, const char *cursor)
{
    int i;
    char item[320];

    strcpy(page, "{\"notifications\":[");
    for (i = 0; i < n; ++i) {
        sprintf(item,
                "%s{\"uri\":\"at://n/%d\",\"cid\":\"c%d\",\"author\":{\"did\":"
                "\"did:plc:a\"},\"reason\":\"like\",\"indexedAt\":"
                "\"2026-10-05T00:%02d:00.000Z\",\"isRead\":%s}",
                i == 0 ? "" : ",", first + i, first + i, 59 - (first + i) % 60,
                (i < unread) ? "false" : "true");
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

static void respond(int status, const char *body)
{
    fake_status = status;
    fake_http_status = status == WF_OK ? 200 : 500;
    fake_body = body;
    last_url[0] = '\0';
    last_body[0] = '\0';
    last_method = 0;
}

static void test_paging_and_seen(void)
{
    static platinum_notifications n;
    platinum_bridge_client *bridge;
    unsigned short dropped;

    bridge = platinum_bridge_client_new("https://bridge.example");
    platinum_notifications_init(&n);

    respond(WF_OK, make_page(0, 20, 2, "2026-10-05T00:00:00Z"));
    check(platinum_notifications_refresh(&n, bridge) == WF_OK, "first page loads");
    check(n.count == 20 && platinum_notifications_has_older(&n), "20 rows and more to come");
    check(strcmp(n.newest_at, "2026-10-05T00:59:00.000Z") == 0, "newest timestamp kept exactly");
    check(n.any_unread == 1, "unread noticed");

    respond(WF_OK, "{\"seenAt\":\"x\"}");
    check(platinum_notifications_mark_seen(&n, bridge) == WF_OK, "mark seen");
    check(strcmp(last_body, "{\"seenAt\":\"2026-10-05T00:59:00.000Z\"}") == 0,
          "seen mark is the newest item's own timestamp");
    respond(WF_OK, "{}");
    check(platinum_notifications_mark_seen(&n, bridge) == WF_OK && last_method == 0,
          "nothing unread left, so no second request");

    respond(WF_OK, make_page(20, 20, 20, "c2"));
    check(platinum_notifications_load_older(&n, bridge, &dropped) == WF_OK, "second page");
    check(strstr(last_url, "cursor=2026-10-05T00%3A00%3A00Z") != NULL, "cursor sent encoded");
    check(n.count == 40 && dropped == 0, "appended");
    check(n.any_unread == 0 && strcmp(n.newest_at, "2026-10-05T00:59:00.000Z") == 0,
          "an older page never moves the seen mark");

    respond(WF_OK, make_page(40, 20, 0, NULL));
    check(platinum_notifications_load_older(&n, bridge, &dropped) == WF_OK &&
              dropped == 20 && n.count == 40,
          "cap of 40 holds");
    check(strcmp(n.items[0].uri, "at://n/20") == 0, "newest rows dropped from the front");
    check(!platinum_notifications_has_older(&n), "no cursor ends paging");

    respond(WF_OK, make_page(0, 1, 0, "x"));
    check(platinum_notifications_load_older(&n, bridge, &dropped) == WF_ERR_INVALID_ARG &&
              last_method == 0,
          "no request when nothing is older");

    respond(WF_OK, make_page(0, 3, 0, "c"));
    check(platinum_notifications_refresh(&n, bridge) == WF_OK, "refresh with nothing unread");
    respond(WF_OK, "{}");
    check(platinum_notifications_mark_seen(&n, bridge) == WF_OK && last_method == 0,
          "all read means no seen request");

    respond(WF_ERR_HTTP, "{}");
    check(platinum_notifications_load_older(&n, bridge, &dropped) != WF_OK &&
              n.count == 3,
          "failed page keeps rows");

    platinum_bridge_client_free(bridge);
}

int main(void)
{
    test_paging_and_seen();
    if (failures != 0) {
        printf("test_notifications: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_notifications: all %d checks passed\n", checks);
    return 0;
}
