/*
 * test_textfield.c -- host-side tests for the TextEdit wrapper's own logic: the
 * length limit, single-line fields, and copying the text out. TextEdit, the
 * Memory Manager and QuickDraw are replaced by a tiny fake that keeps the text
 * in a buffer; it checks the wrapper's behaviour, not TextEdit's. Not Classic
 * Mac OS 9 validation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "textfield.h"

static int failures;
static int checks;

static void check(int condition, const char *what)
{
    checks++;
    if (!condition) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

/* ---- a fake TextEdit ---------------------------------------------------- */

static char fake_text[2048];
static char *fake_text_ptr = fake_text;
static Handle fake_text_handle = &fake_text_ptr;
static TERec fake_rec;
static TERec *fake_rec_ptr = &fake_rec;
static int keys_delivered;

TEHandle TENew(const Rect *destRect, const Rect *viewRect)
{
    (void)destRect;
    memset(&fake_rec, 0, sizeof(fake_rec));
    fake_rec.viewRect = *viewRect;
    fake_text[0] = '\0';
    return &fake_rec_ptr;
}

void TEAutoView(Boolean fAuto, TEHandle hTE) { (void)fAuto; (void)hTE; }

void TESetSelect(long selStart, long selEnd, TEHandle hTE)
{
    (*hTE)->selStart = (short)selStart;
    (*hTE)->selEnd = (short)selEnd;
}

void TESetText(const void *text, long length, TEHandle hTE)
{
    memcpy(fake_text, text, (size_t)length);
    /* The real handle is bigger than the text, and not terminated. */
    memset(fake_text + length, 'Z', 16);
    (*hTE)->teLength = (short)length;
}

CharsHandle TEGetText(TEHandle hTE) { (void)hTE; return fake_text_handle; }

void TEKey(short key, TEHandle hTE)
{
    TERec *r = *hTE;
    ++keys_delivered;
    if (key == 8) {
        if (r->selStart != r->selEnd) {
            memmove(fake_text + r->selStart, fake_text + r->selEnd, (size_t)(r->teLength - r->selEnd));
            r->teLength = (short)(r->teLength - (r->selEnd - r->selStart));
            r->selEnd = r->selStart;
        } else if (r->selStart > 0) {
            memmove(fake_text + r->selStart - 1, fake_text + r->selStart, (size_t)(r->teLength - r->selStart));
            --r->teLength;
            --r->selStart;
            --r->selEnd;
        }
        return;
    }
    memmove(fake_text + r->selStart + 1, fake_text + r->selEnd, (size_t)(r->teLength - r->selEnd));
    fake_text[r->selStart] = (char)key;
    r->teLength = (short)(r->teLength - (r->selEnd - r->selStart) + 1);
    ++r->selStart;
    r->selEnd = r->selStart;
}

void SetRect(Rect *r, short left, short top, short right, short bottom)
{
    r->left = left;
    r->top = top;
    r->right = right;
    r->bottom = bottom;
}
void GetPort(GrafPtr *port) { *port = NULL; }
void SetPort(GrafPtr port) { (void)port; }
SignedByte HGetState(Handle h) { (void)h; return 0; }
void HSetState(Handle h, SignedByte s) { (void)h; (void)s; }
void HLock(Handle h) { (void)h; }

/* ---- tests -------------------------------------------------------------- */

static EventRecord key_event(int ch, unsigned long modifiers)
{
    EventRecord e;

    memset(&e, 0, sizeof(e));
    e.what = keyDown;
    e.message = (unsigned long)ch;
    e.modifiers = modifiers;
    return e;
}

static void type(TEHandle f, const char *s, long max, int single)
{
    for (; *s; ++s) {
        EventRecord e = key_event(*s, 0);
        (void)platinum_textfield_key(f, &e, max, single);
    }
}

static void test_all(void)
{
    Rect view;
    TEHandle f;
    char buffer[32];
    EventRecord e;

    SetRect(&view, 0, 0, 100, 20);
    f = platinum_textfield_new((WindowPtr)1, &view);
    check(f != NULL, "field created");
    check(platinum_textfield_new(NULL, &view) == NULL, "no window, no field");

    platinum_textfield_set(f, "hello");
    check(platinum_textfield_length(f) == 5, "set reports the length");
    check(platinum_textfield_copy(f, buffer, sizeof(buffer)) == 5 && strcmp(buffer, "hello") == 0,
          "copy uses teLength, not the bigger buffer behind the handle");
    check(platinum_textfield_copy(f, buffer, 5) == -1 && buffer[0] == '\0',
          "a capacity too small for the text is refused, not truncated");
    check(platinum_textfield_copy(f, buffer, 6) == 5, "capacity of length + 1 is enough");

    platinum_textfield_set(f, "");
    type(f, "abcdefgh", 6, 1);
    check(platinum_textfield_length(f) == 6, "typing stops at the limit");
    platinum_textfield_copy(f, buffer, sizeof(buffer));
    check(strcmp(buffer, "abcdef") == 0, "and the first characters are kept");

    e = key_event(8, 0);
    check(platinum_textfield_key(f, &e, 6, 1) == 1 && platinum_textfield_length(f) == 5,
          "delete is never limited");
    type(f, "xy", 6, 1);
    check(platinum_textfield_length(f) == 6, "room appears after a delete");

    TESetSelect(0, 6, f);
    e = key_event('Q', 0);
    check(platinum_textfield_key(f, &e, 6, 1) == 1 && platinum_textfield_length(f) == 1,
          "a selection counts as room for the character replacing it");

    keys_delivered = 0;
    e = key_event('\r', 0);
    check(platinum_textfield_key(f, &e, 6, 1) == 0 && keys_delivered == 0, "Return is refused in a single-line field");
    e = key_event(3, 0);
    check(platinum_textfield_key(f, &e, 6, 1) == 0, "Enter is refused in a single-line field");
    e = key_event('\r', 0);
    check(platinum_textfield_key(f, &e, 6, 0) == 1, "Return is a character in a multi-line field");
    e = key_event('c', cmdKey);
    check(platinum_textfield_key(f, &e, 6, 0) == 0, "command keys are not passed on");
    e = key_event('a', 0);
    e.what = mouseDown;
    check(platinum_textfield_key(f, &e, 6, 0) == 0, "only key events are handled");
    check(platinum_textfield_key(NULL, &e, 6, 0) == 0, "no field, no key");
}

int main(void)
{
    test_all();
    if (failures != 0) {
        printf("test_textfield: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_textfield: all %d checks passed\n", checks);
    return 0;
}
