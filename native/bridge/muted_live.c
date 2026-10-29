#include "muted_live.h"

#include <stdlib.h>
#include <string.h>

#include <wolfram/actor_prefs_typed.h>
#include <wolfram/agent.h>

/* strdup is not in C89, and the bridge builds with -std=c2x -pedantic, so copy
 * explicitly rather than relying on a POSIX declaration. */
static char *
copy_string(const char *s)
{
    size_t n;
    char *out;

    if (s == NULL) return NULL;
    n = strlen(s) + 1;
    out = malloc(n);
    if (out != NULL) memcpy(out, s, n);
    return out;
}

static int
fold(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

static int
same_word(const char *a, const char *b)
{
    if (a == NULL || b == NULL) return 0;
    while (*a != '\0' && *b != '\0') {
        if (fold((unsigned char) *a) != fold((unsigned char) *b)) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int
has_target(const wf_actor_pref_muted_word *word, const char *name)
{
    size_t i;

    for (i = 0; i < word->target_count; i++) {
        if (word->targets[i] != NULL && strcmp(word->targets[i], name) == 0) return 1;
    }
    return 0;
}

/* Copies src into dst, cutting to fit and never splitting a UTF-8 code point. */
static void
clip(const char *src, char *dst, size_t cap)
{
    size_t n;

    if (cap == 0) return;
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

static void
free_word(wf_actor_pref_muted_word *word)
{
    size_t i;

    free(word->id);
    free(word->value);
    for (i = 0; i < word->target_count; i++) free(word->targets[i]);
    free(word->targets);
    free(word->actor_target);
    free(word->expires_at);
    memset(word, 0, sizeof *word);
}

/* Appends one word that applies to content and tags, to everyone. The id is
 * left out: the server assigns one, as it does for entries written by older
 * clients. Returns 0, or -1 without changing the list. */
static int
append_word(wf_actor_preferences *prefs, const char *value)
{
    wf_actor_pref_muted_word *words;
    wf_actor_pref_muted_word *word;
    char **targets;
    char *copy;

    words = realloc(prefs->muting_keywords,
                    (prefs->muting_keyword_count + 1) * sizeof *words);
    if (words == NULL) return -1;
    prefs->muting_keywords = words;
    word = &words[prefs->muting_keyword_count];
    memset(word, 0, sizeof *word);

    targets = calloc(2, sizeof *targets);
    if (targets == NULL) return -1;
    word->targets = targets;

    copy = copy_string(value);
    if (copy == NULL) return -1;
    word->value = copy;
    copy = copy_string("content");
    if (copy == NULL) return -1;
    targets[0] = copy;
    copy = copy_string("tag");
    if (copy == NULL) {
        word->target_count = 1;
        return -1;
    }
    targets[1] = copy;
    word->target_count = 2;
    copy = copy_string("all");
    if (copy == NULL) return -1;
    word->actor_target = copy;

    prefs->muting_keyword_count++;
    return 0;
}

/* Drops every word whose value matches, ignoring case. Returns how many were
 * dropped; the kept entries stay in order. */
static size_t
remove_words(wf_actor_preferences *prefs, const char *value)
{
    size_t i;
    size_t kept = 0;
    size_t removed = 0;

    for (i = 0; i < prefs->muting_keyword_count; i++) {
        wf_actor_pref_muted_word *word = &prefs->muting_keywords[i];

        if (same_word(word->value, value)) {
            free_word(word);
            removed++;
            continue;
        }
        if (kept != i) prefs->muting_keywords[kept] = *word;
        kept++;
    }
    prefs->muting_keyword_count = kept;
    return removed;
}

platinum_content_result
platinum_muted_list_live(void *ctx, platinum_muted_view *views, size_t max, size_t *count)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_actor_preferences prefs;
    size_t i;

    if (agent == NULL || views == NULL || count == NULL) return PLATINUM_CONTENT_ERROR;
    *count = 0;

    memset(&prefs, 0, sizeof prefs);
    if (wf_agent_get_actor_prefs_typed(agent, &prefs) != WF_OK) {
        wf_actor_preferences_free(&prefs);
        return PLATINUM_CONTENT_ERROR;
    }

    for (i = 0; i < prefs.muting_keyword_count && *count < max; i++) {
        const wf_actor_pref_muted_word *word = &prefs.muting_keywords[i];
        platinum_muted_view *view;

        if (word->value == NULL || word->value[0] == '\0') continue;
        view = &views[*count];
        memset(view, 0, sizeof *view);
        clip(word->value, view->value, sizeof view->value);
        view->content = has_target(word, "content");
        view->tag = has_target(word, "tag");
        /* A word with no target applies to content, which is the default. */
        if (!view->content && !view->tag) view->content = 1;
        (*count)++;
    }

    wf_actor_preferences_free(&prefs);
    return PLATINUM_CONTENT_OK;
}

platinum_account_result
platinum_muted_set_live(void *ctx, const char *word, int on)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_actor_preferences prefs;
    size_t i;
    int changed = 0;

    if (agent == NULL || word == NULL || word[0] == '\0') return PLATINUM_ACCOUNT_ERROR;

    memset(&prefs, 0, sizeof prefs);
    if (wf_agent_get_actor_prefs_typed(agent, &prefs) != WF_OK) {
        wf_actor_preferences_free(&prefs);
        return PLATINUM_ACCOUNT_ERROR;
    }

    if (on) {
        for (i = 0; i < prefs.muting_keyword_count; i++) {
            if (same_word(prefs.muting_keywords[i].value, word)) break;
        }
        if (i == prefs.muting_keyword_count) {
            if (append_word(&prefs, word) != 0) {
                wf_actor_preferences_free(&prefs);
                return PLATINUM_ACCOUNT_ERROR;
            }
            changed = 1;
        }
    } else {
        changed = remove_words(&prefs, word) > 0;
    }

    /* An unchanged list needs no write, which is what makes both directions
     * idempotent without touching preferences the account may be editing. */
    if (changed && wf_agent_put_actor_prefs_typed(agent, &prefs) != WF_OK) {
        wf_actor_preferences_free(&prefs);
        return PLATINUM_ACCOUNT_ERROR;
    }

    wf_actor_preferences_free(&prefs);
    return PLATINUM_ACCOUNT_OK;
}
