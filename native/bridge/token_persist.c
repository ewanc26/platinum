#include "token_persist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fsstore.h"

#define TOKEN_FILE "tokens.txt"
#define HEADER "platinum-tokens 1"

static int
clean_field(const char *s)
{
    const char *p;
    for (p = s; *p; p++) {
        if (*p == '\t' || *p == '\n' || *p == '\r') return 0;
    }
    return 1;
}

/* Appends text to buf at *used, refusing to overflow cap. */
static int
append(char *buf, size_t cap, size_t *used, const char *text)
{
    size_t n = strlen(text);
    if (*used + n >= cap) return -1;
    memcpy(buf + *used, text, n + 1);
    *used += n;
    return 0;
}

long
platinum_token_store_serialize(const platinum_token_store *store, char *buf, size_t cap)
{
    size_t used = 0;
    size_t i;
    char line[512];

    if (store == NULL || buf == NULL || cap == 0) return -1;
    buf[0] = '\0';
    if (append(buf, cap, &used, HEADER "\n") != 0) return -1;

    for (i = 0; i < PLATINUM_TOKEN_SLOTS; i++) {
        const platinum_token_slot *s = &store->slots[i];
        if (s->id[0] == '\0') continue;
        if (!clean_field(s->id) || !clean_field(s->did) || !clean_field(s->token_hash_hex)) return -1;
        /* Every field is length-capped by the slot, so the line fits its buffer. */
        sprintf(line, "%s\t%s\t%s\t%ld\t%d\t%ld\n", s->id, s->did, s->token_hash_hex,
                s->created_at, s->revoked ? 1 : 0, s->last_used);
        if (append(buf, cap, &used, line) != 0) return -1;
    }
    return (long) used;
}

/* Parses one occupied-slot line into slot. Returns 0, or -1 for any malformed field. */
static int
parse_line(char *line, platinum_token_slot *slot)
{
    char *fields[6];
    int k = 0;
    char *p = line;
    char *end;
    long created;
    long revoked;
    long last;

    memset(slot, 0, sizeof *slot);
    for (;;) {
        char *tab = strchr(p, '\t');
        if (k >= 6) return -1;
        fields[k++] = p;
        if (tab == NULL) break;
        *tab = '\0';
        p = tab + 1;
    }
    if (k != 6) return -1;
    if (strlen(fields[0]) == 0 || strlen(fields[0]) >= PLATINUM_TOKEN_ID_MAX) return -1;
    if (strlen(fields[1]) == 0 || strlen(fields[1]) >= PLATINUM_TOKEN_DID_MAX) return -1;
    if (strlen(fields[2]) != PLATINUM_TOKEN_HASH_HEX - 1) return -1;

    created = strtol(fields[3], &end, 10);
    if (*end != '\0' || created < 0) return -1;
    revoked = strtol(fields[4], &end, 10);
    if (*end != '\0' || (revoked != 0 && revoked != 1)) return -1;
    last = strtol(fields[5], &end, 10);
    if (*end != '\0' || last < 0) return -1;

    strcpy(slot->id, fields[0]);
    strcpy(slot->did, fields[1]);
    strcpy(slot->token_hash_hex, fields[2]);
    slot->created_at = created;
    slot->revoked = (int) revoked;
    slot->last_used = last;
    return 0;
}

int
platinum_token_store_deserialize(platinum_token_store *store, const char *buf, size_t len)
{
    platinum_token_store fresh;
    char *copy;
    char *line;
    size_t count = 0;
    int first = 1;

    if (store == NULL || buf == NULL) return -1;
    copy = malloc(len + 1);
    if (copy == NULL) return -1;
    memcpy(copy, buf, len);
    copy[len] = '\0';

    platinum_token_store_init(&fresh);
    line = copy;
    while (line != NULL && *line != '\0') {
        char *nl = strchr(line, '\n');
        if (nl != NULL) *nl = '\0';
        if (first) {
            if (strcmp(line, HEADER) != 0) {
                free(copy);
                return -1;
            }
            first = 0;
        } else if (line[0] != '\0') {
            if (count >= PLATINUM_TOKEN_SLOTS) {
                free(copy);
                return -1;
            }
            if (parse_line(line, &fresh.slots[count]) != 0) {
                free(copy);
                return -1;
            }
            count++;
        }
        line = (nl != NULL) ? nl + 1 : NULL;
    }
    free(copy);
    if (first) return -1;

    /* Ids must be unique, or authentication and revocation would be ambiguous. */
    {
        size_t a, b;
        for (a = 0; a < count; a++) {
            for (b = a + 1; b < count; b++) {
                if (strcmp(fresh.slots[a].id, fresh.slots[b].id) == 0) return -1;
            }
        }
    }
    *store = fresh;
    return 0;
}

int
platinum_token_store_save(const platinum_token_store *store, const char *dir)
{
    static char buf[PLATINUM_TOKEN_SLOTS * 320 + 64];
    long n = platinum_token_store_serialize(store, buf, sizeof buf);
    if (n < 0) return -1;
    return platinum_fs_write_atomic(dir, TOKEN_FILE, (const unsigned char *) buf, (size_t) n);
}

int
platinum_token_store_load(platinum_token_store *store, const char *dir)
{
    unsigned char *data = NULL;
    size_t len = 0;
    int rc;

    if (store == NULL) return -1;
    rc = platinum_fs_read_all(dir, TOKEN_FILE, &data, &len, PLATINUM_TOKEN_SLOTS * 320 + 64);
    if (rc == 1) {
        platinum_token_store_init(store);
        return 1;
    }
    if (rc != 0) return rc;
    if (platinum_token_store_deserialize(store, (const char *) data, len) != 0) {
        free(data);
        return -1;
    }
    free(data);
    return 0;
}
