/*
 * test_prefs.c -- host-side tests for the Preferences-folder files: the shared
 * reader and writer, the settings file and the unsent draft. The Toolbox calls
 * are replaced by a small in-memory file system defined below, written to
 * behave as the File Manager documents, in particular that FSMakeFSSpec reports
 * fnfErr for a file that does not exist yet while still giving a spec to create
 * it with. That is a model of the documented behaviour, not Mac OS 9. Not Classic
 * Mac OS 9 validation.
 */

#include <stdio.h>
#include <string.h>

#include <Files.h>
#include <Folders.h>

#include "config.h"
#include "draft.h"
#include "prefs_file.h"

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

/* ---- the fake file system ---------------------------------------------- */

#define FAKE_FILES 4
#define FAKE_BYTES 16384

static struct {
    int used;
    char name[64];
    char data[FAKE_BYTES];
    long size;
} files[FAKE_FILES];

static struct {
    int open;
    int file;
    long mark;
} handles[8];

static int fake_fail_write; /* make FSWrite fail with ioErr */
static OSType last_creator, last_type;

static void fake_reset(void)
{
    memset(files, 0, sizeof(files));
    memset(handles, 0, sizeof(handles));
    fake_fail_write = 0;
}

static int find_file(const FSSpec *spec)
{
    int i;
    char name[64];
    int length = (unsigned char)spec->nodeName[0];

    memcpy(name, spec->nodeName + 1, (size_t)length);
    name[length] = '\0';
    for (i = 0; i < FAKE_FILES; ++i)
        if (files[i].used && strcmp(files[i].name, name) == 0)
            return i;
    return -1;
}

OSErr FindFolder(short vRefNum, OSType folderType, Boolean createFolder,
                 short *foundVRefNum, long *foundDirID)
{
    (void)vRefNum;
    (void)folderType;
    (void)createFolder;
    *foundVRefNum = 0;
    *foundDirID = 1;
    return noErr;
}

OSErr FSMakeFSSpec(short vRefNum, long dirID, ConstStr255Param fileName,
                   FSSpec *spec)
{
    (void)vRefNum;
    (void)dirID;
    memset(spec, 0, sizeof(*spec));
    memcpy(spec->nodeName, fileName, (size_t)fileName[0] + 1);
    /* As documented: not found, but the spec is good for creating the file. */
    return find_file(spec) < 0 ? fnfErr : noErr;
}

OSErr FSpCreate(const FSSpec *spec, OSType creator, OSType fileType,
                ScriptCode scriptTag)
{
    int i;
    int length = (unsigned char)spec->nodeName[0];

    (void)scriptTag;
    if (find_file(spec) >= 0)
        return -48; /* dupFNErr */
    for (i = 0; i < FAKE_FILES; ++i) {
        if (!files[i].used) {
            files[i].used = 1;
            memcpy(files[i].name, spec->nodeName + 1, (size_t)length);
            files[i].name[length] = '\0';
            files[i].size = 0;
            last_creator = creator;
            last_type = fileType;
            return noErr;
        }
    }
    return -34; /* dskFulErr */
}

OSErr FSpOpenDF(const FSSpec *spec, SInt8 permission, short *refNum)
{
    int i;
    int f = find_file(spec);

    (void)permission;
    if (f < 0)
        return fnfErr;
    for (i = 0; i < 8; ++i) {
        if (!handles[i].open) {
            handles[i].open = 1;
            handles[i].file = f;
            handles[i].mark = 0;
            *refNum = (short)i;
            return noErr;
        }
    }
    return -42; /* tmfoErr */
}

OSErr FSpDelete(const FSSpec *spec)
{
    int f = find_file(spec);

    if (f < 0)
        return fnfErr;
    files[f].used = 0;
    return noErr;
}

OSErr FSClose(short refNum)
{
    handles[refNum].open = 0;
    return noErr;
}

OSErr SetEOF(short refNum, long logEOF)
{
    files[handles[refNum].file].size = logEOF;
    return noErr;
}

OSErr GetEOF(short refNum, long *logEOF)
{
    *logEOF = files[handles[refNum].file].size;
    return noErr;
}

OSErr FSWrite(short refNum, long *count, const void *buffPtr)
{
    int f = handles[refNum].file;

    if (fake_fail_write) {
        *count = 0;
        return ioErr;
    }
    memcpy(files[f].data + handles[refNum].mark, buffPtr, (size_t)*count);
    handles[refNum].mark += *count;
    if (handles[refNum].mark > files[f].size)
        files[f].size = handles[refNum].mark;
    return noErr;
}

OSErr FSRead(short refNum, long *count, void *buffPtr)
{
    int f = handles[refNum].file;
    long left = files[f].size - handles[refNum].mark;

    if (*count > left)
        *count = left;
    memcpy(buffPtr, files[f].data + handles[refNum].mark, (size_t)*count);
    handles[refNum].mark += *count;
    return noErr;
}

/* ---- tests ------------------------------------------------------------- */

static const unsigned char kName[] = { 4, 't', 'e', 's', 't' };

static void test_prefs_file(void)
{
    char buffer[64];
    long length;

    fake_reset();
    check(platinum_prefs_read(kName, buffer, sizeof(buffer), &length) == fnfErr && length == 0,
          "reading a missing file is fnfErr");
    check(platinum_prefs_write(kName, 'PTLM', 'TEXT', "hello", 5) == noErr,
          "a file that does not exist yet can be written (FSMakeFSSpec says fnfErr first)");
    check(last_creator == 'PTLM' && last_type == 'TEXT', "created with the given creator and type");
    check(platinum_prefs_read(kName, buffer, sizeof(buffer), &length) == noErr && length == 5 &&
              strcmp(buffer, "hello") == 0,
          "it reads back, NUL-terminated");

    check(platinum_prefs_write(kName, 'PTLM', 'TEXT', "hi", 2) == noErr &&
              platinum_prefs_read(kName, buffer, sizeof(buffer), &length) == noErr && length == 2 &&
              strcmp(buffer, "hi") == 0,
          "a shorter write replaces the file; nothing of the old text is left");

    check(platinum_prefs_write(kName, 'PTLM', 'TEXT', "", 0) == noErr &&
              platinum_prefs_read(kName, buffer, sizeof(buffer), &length) == noErr && length == 0 &&
              buffer[0] == '\0',
          "an empty write gives an empty file");

    platinum_prefs_write(kName, 'PTLM', 'TEXT', "0123456789", 10);
    check(platinum_prefs_read(kName, buffer, 10, &length) == paramErr && buffer[0] == '\0',
          "a file that does not fit is refused, not truncated");
    check(platinum_prefs_read(kName, buffer, 11, &length) == noErr && length == 10, "exactly fitting is fine");

    fake_fail_write = 1;
    check(platinum_prefs_write(kName, 'PTLM', 'TEXT', "x", 1) == ioErr, "a failed write is reported");
    fake_fail_write = 0;

    check(platinum_prefs_delete(kName) == noErr && platinum_prefs_delete(kName) == noErr,
          "delete, and delete again, are both fine");
    check(platinum_prefs_read(kName, buffer, sizeof(buffer), &length) == fnfErr, "gone after delete");
    check(platinum_prefs_write(NULL, 'A', 'B', "x", 1) == paramErr &&
              platinum_prefs_write(kName, 'A', 'B', NULL, 1) == paramErr &&
              platinum_prefs_read(kName, NULL, 4, NULL) == paramErr,
          "bad arguments are refused");
}

static void test_config(void)
{
    platinum_config out;
    platinum_config in;

    fake_reset();
    platinum_config_init(&in);
    platinum_config_set_bridge_url(&in, "https://bridge.example");
    platinum_config_set_token(&in, "tok123");
    platinum_config_set_did(&in, "did:plc:me");
    platinum_config_set_installation_id(&in, "inst1");

    check(platinum_config_load(&out) == fnfErr && out.bridge_token[0] == '\0',
          "no settings file: fnfErr and an empty config");
    check(platinum_config_save(&in) == noErr,
          "the first save works, creating the file (this is the first pairing)");
    check(last_creator == 'PTLM' && last_type == 'PREF', "created as the settings file");
    check(platinum_config_load(&out) == noErr && strcmp(out.bridge_token, "tok123") == 0 &&
              strcmp(out.did, "did:plc:me") == 0 && strcmp(out.installation_id, "inst1") == 0 &&
              strcmp(out.bridge_url, "https://bridge.example") == 0 && platinum_config_is_paired(&out),
          "it loads back complete");

    platinum_config_set_token(&in, "newer");
    check(platinum_config_save(&in) == noErr && platinum_config_load(&out) == noErr &&
              strcmp(out.bridge_token, "newer") == 0,
          "saving again replaces it");

    check(platinum_config_clear() == noErr && platinum_config_clear() == noErr &&
              platinum_config_load(&out) == fnfErr,
          "clear removes it, twice is fine");
}

static void test_draft(void)
{
    char text[PLATINUM_DRAFT_MAX + 1];
    static char big[400];

    fake_reset();
    check(platinum_draft_load(text, sizeof(text)) == fnfErr, "no draft to begin with");
    check(platinum_draft_save("half a thought\rsecond line") == noErr, "a draft saves");
    check(last_type == 'TEXT', "as a text file");
    check(platinum_draft_load(text, sizeof(text)) == noErr &&
              strcmp(text, "half a thought\rsecond line") == 0,
          "and loads back exactly, line break included");

    check(platinum_draft_save("") == noErr && platinum_draft_load(text, sizeof(text)) == fnfErr,
          "saving nothing clears the draft");
    platinum_draft_save("again");
    check(platinum_draft_clear() == noErr && platinum_draft_load(text, sizeof(text)) == fnfErr,
          "clear removes it");

    memset(big, 'a', 300);
    big[300] = '\0';
    check(platinum_draft_save(big) == noErr && platinum_draft_load(text, sizeof(text)) == noErr &&
              strlen(text) == 300,
          "300 characters is the limit and fits");
    big[300] = 'a';
    big[301] = '\0';
    check(platinum_draft_save(big) == paramErr, "301 is refused");

    /* A file left by something else, too big to be a draft. */
    platinum_prefs_write((const unsigned char *)"\016Platinum Draft", 'PTLM', 'TEXT', big, 301);
    check(platinum_draft_load(text, sizeof(text)) == paramErr && text[0] == '\0',
          "an over-long draft file is refused, not returned");
    check(platinum_draft_load(text, 10) == paramErr && platinum_draft_load(NULL, 400) == paramErr,
          "a buffer that is too small is refused");
    check(platinum_draft_save(NULL) == noErr, "NULL clears");
}

int main(void)
{
    test_prefs_file();
    test_config();
    test_draft();
    if (failures != 0) {
        printf("test_prefs: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_prefs: all %d checks passed\n", checks);
    return 0;
}
