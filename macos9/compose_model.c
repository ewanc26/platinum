/*
 * compose_model.c -- what a compose window is replying to or quoting, and who
 * may reply. No QuickDraw here, so it links on a host for tests; the window is
 * compose.c.
 */
#include "bridge_client.h"
#include "compose.h"

#include <string.h>

static int compose_target(platinum_compose *compose, char *uri_out,
                          long uri_cap, char *cid_out, long cid_cap,
                          const char *uri, const char *cid,
                          const char *handle, const char *verb,
                          const char *fallback)
{
    size_t length;

    if (uri == NULL || cid == NULL || uri[0] == '\0' || cid[0] == '\0' ||
        (long)strlen(uri) >= uri_cap || (long)strlen(cid) >= cid_cap)
        return 0;

    strcpy(uri_out, uri);
    strcpy(cid_out, cid);
    strcpy(compose->caption, verb);
    if (handle != NULL && handle[0] != '\0') {
        length = strlen(compose->caption);
        if (handle[0] != '@')
            compose->caption[length++] = '@';
        strncpy(compose->caption + length, handle,
                sizeof(compose->caption) - length - 1);
        compose->caption[sizeof(compose->caption) - 1] = '\0';
    } else {
        strcpy(compose->caption, fallback);
    }
    return 1;
}

int platinum_compose_set_reply(platinum_compose *compose,
                               const char *uri,
                               const char *cid,
                               const char *handle)
{
    if (compose == NULL)
        return 0;
    compose->reply_uri[0] = '\0';
    compose->reply_cid[0] = '\0';
    compose->caption[0] = '\0';
    /* A quote replaced by a reply, and a reply has no gate. */
    compose->quote_uri[0] = '\0';
    compose->quote_cid[0] = '\0';
    compose->reply_gate = 0;
    return compose_target(compose, compose->reply_uri,
                          (long)sizeof(compose->reply_uri), compose->reply_cid,
                          (long)sizeof(compose->reply_cid), uri, cid, handle,
                          "Replying to ", "Replying to a post");
}

int platinum_compose_set_quote(platinum_compose *compose,
                               const char *uri,
                               const char *cid,
                               const char *handle)
{
    if (compose == NULL)
        return 0;
    compose->reply_uri[0] = '\0';
    compose->reply_cid[0] = '\0';
    compose->quote_uri[0] = '\0';
    compose->quote_cid[0] = '\0';
    compose->caption[0] = '\0';
    return compose_target(compose, compose->quote_uri,
                          (long)sizeof(compose->quote_uri), compose->quote_cid,
                          (long)sizeof(compose->quote_cid), uri, cid, handle,
                          "Quoting ", "Quoting a post");
}

int platinum_compose_cycle_gate(platinum_compose *compose)
{
    if (compose == NULL || compose->reply_uri[0] != '\0')
        return 0;
    compose->reply_gate = (compose->reply_gate + 1) % PLATINUM_REPLY_GATE_COUNT;
    return 1;
}
