/*
 * test_json_min.c -- host-side tests for the Mac OS 9 JSON scanner.
 *
 * json_min.c needs nothing from Wolfram at runtime (wf_status is an enum), so
 * these tests build and run on a development host with only Wolfram's public
 * include directory on the include path. That is a dialect and logic check on
 * a modern compiler: it is not Classic Mac OS 9 hardware validation.
 *
 * The cases below are deliberately hostile. A scanner that is lenient about
 * truncated strings, raw control characters or unknown escapes will happily
 * hand the client half a token or a DID.
 */

#include <stdio.h>
#include <string.h>

#include "json_min.h"

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

static void check_status(wf_status got, wf_status want, const char *what)
{
    checks++;
    if (got != want) {
        failures++;
        printf("FAIL: %s (got %d, want %d)\n", what, (int)got, (int)want);
    }
}

static void check_str(const char *got, const char *want, const char *what)
{
    checks++;
    if (got == NULL || strcmp(got, want) != 0) {
        failures++;
        printf("FAIL: %s (got \"%s\", want \"%s\")\n", what,
               got ? got : "(null)", want);
    }
}

static void test_escape(void)
{
    char buf[64];

    check_status(platinum_json_escape(buf, sizeof(buf), "ABC123"), WF_OK,
                 "escape plain");
    check_str(buf, "ABC123", "escape plain value");

    check_status(platinum_json_escape(buf, sizeof(buf), "a\"b\\c"), WF_OK,
                 "escape quotes");
    check_str(buf, "a\\\"b\\\\c", "escape quotes value");

    check_status(platinum_json_escape(buf, sizeof(buf), "line\none\ttab"),
                 WF_OK, "escape control shorthands");
    check_str(buf, "line\\none\\ttab", "escape control shorthands value");

    /* No shorthand exists for these, so they must become \u00XX. */
    check_status(platinum_json_escape(buf, sizeof(buf), "a\x01" "b"), WF_OK,
                 "escape other control");
    check_str(buf, "a\\u0001b", "escape other control value");

    /* Truncation must be reported, never silently shortened. */
    check_status(platinum_json_escape(buf, 4, "ABCDEF"), WF_ERR_ALLOC,
                 "escape truncation");
    check_str(buf, "ABC", "escape truncation keeps the prefix");

    check_status(platinum_json_escape(buf, 0, "A"), WF_ERR_INVALID_ARG,
                 "escape zero capacity");
    check_status(platinum_json_escape(NULL, 8, "A"), WF_ERR_INVALID_ARG,
                 "escape null destination");
    check_status(platinum_json_escape(buf, sizeof(buf), NULL), WF_OK,
                 "escape null source");
    check_str(buf, "", "escape null source value");

    /* The exact shape of a pairing request body. */
    {
        char body[32];
        const char *prefix = "{\"code\":\"";

        check_status(platinum_json_escape(buf, sizeof(buf), "K7Q2M9"), WF_OK,
                     "escape pairing code");
        strcpy(body, prefix);
        check_status(platinum_json_escape(body + strlen(prefix),
                                         sizeof(body) - strlen(prefix), buf),
                     WF_OK, "escape into request body");
        strcat(body, "\"}");
        check_str(body, "{\"code\":\"K7Q2M9\"}", "pairing request body");
    }
}

static void test_get_string(void)
{
    char buf[64];
    const char *pair = "{\"protocol\":1,\"token\":\"TOK\",\"did\":\"did:plc:abc\"}";

    check_status(platinum_json_get_string(pair, "token", buf, sizeof(buf)),
                 WF_OK, "get token");
    check_str(buf, "TOK", "get token value");

    check_status(platinum_json_get_string(pair, "did", buf, sizeof(buf)),
                 WF_OK, "get did");
    check_str(buf, "did:plc:abc", "get did value");

    check_status(platinum_json_get_string(pair, "protocol", buf, sizeof(buf)),
                 WF_ERR_PARSE, "member of the wrong type");

    check_status(platinum_json_get_string(pair, "missing", buf, sizeof(buf)),
                 WF_ERR_NOT_FOUND, "absent member");

    /* A member that appears after an unrelated nested value must still be
     * found: skipping must not depend on the shape of what is skipped. */
    check_status(platinum_json_get_string(
                     "{\"a\":{\"b\":[1,2,{\"c\":null}]},\"token\":\"TOK\"}",
                     "token", buf, sizeof(buf)),
                 WF_OK, "get member after a nested value");
    check_str(buf, "TOK", "get member after a nested value result");

    check_status(platinum_json_get_string("{\"token\":\"TOK\",}", "token",
                                          buf, sizeof(buf)),
                 WF_ERR_PARSE, "trailing comma");
    check_status(platinum_json_get_string("{\"token\" \"TOK\"}", "token", buf,
                                          sizeof(buf)),
                 WF_ERR_PARSE, "missing colon");
    check_status(platinum_json_get_string("{}", "token", buf, sizeof(buf)),
                 WF_ERR_NOT_FOUND, "empty object");
    check_status(platinum_json_get_string("not json", "token", buf,
                                          sizeof(buf)),
                 WF_ERR_PARSE, "not an object");

    /* Escapes, including a surrogate pair, must decode to UTF-8. */
    check_status(platinum_json_get_string("{\"k\":\"a\\nb\"}", "k", buf,
                                          sizeof(buf)),
                 WF_OK, "decode newline escape");
    check_str(buf, "a\nb", "decode newline escape value");

    check_status(platinum_json_get_string("{\"k\":\"\\u0041\"}", "k", buf,
                                          sizeof(buf)),
                 WF_OK, "decode \\u escape");
    check_str(buf, "A", "decode \\u escape value");

    check_status(platinum_json_get_string("{\"k\":\"\\ud83d\\ude00\"}", "k",
                                          buf, sizeof(buf)),
                 WF_OK, "decode surrogate pair");
    check_str(buf, "\xf0\x9f\x98\x80", "decode surrogate pair value");

    /* Malformed input must be rejected rather than half-decoded. */
    check_status(platinum_json_get_string("{\"k\":\"abc}", "k", buf,
                                          sizeof(buf)),
                 WF_ERR_PARSE, "unterminated string");
    check_status(platinum_json_get_string("{\"k\":\"a\\qb\"}", "k", buf,
                                          sizeof(buf)),
                 WF_ERR_PARSE, "unknown escape");
    check_status(platinum_json_get_string("{\"k\":\"a\tb\"}", "k", buf,
                                          sizeof(buf)),
                 WF_ERR_PARSE, "raw control character");
    check_status(platinum_json_get_string("{\"k\":\"\\ud83d\"}", "k", buf,
                                          sizeof(buf)),
                 WF_ERR_PARSE, "lone high surrogate");
    check_status(platinum_json_get_string("{\"k\":\"\\ude00\"}", "k", buf,
                                          sizeof(buf)),
                 WF_ERR_PARSE, "lone low surrogate");

    check_status(platinum_json_get_string("{\"k\":\"abcdefghij\"}", "k", buf,
                                          4),
                 WF_ERR_ALLOC, "string truncation");

    /* A member name longer than the comparison buffer must not match a short
     * name by truncated prefix. */
    {
        char body[128];

        /* Well-formed, but the first key is longer than any name this client
         * looks up. */
        strcpy(body, "{\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                     "\":\"LONG\",\"token\":\"SHORT\"}");
        check_status(platinum_json_get_string(body, "token", buf, sizeof(buf)),
                     WF_OK, "long key skipped");
        check_str(buf, "SHORT", "long key skipped result");
    }
}

static void test_get_int(void)
{
    long value = 0;

    check_status(platinum_json_get_int("{\"protocol\":1}", "protocol", &value),
                 WF_OK, "get int");
    check(value == 1, "get int value");

    check_status(platinum_json_get_int("{\"n\":-20}", "n", &value), WF_OK,
                 "get negative int");
    check(value == -20, "get negative int value");

    check_status(platinum_json_get_int("{\"n\":0}", "n", &value), WF_OK,
                 "get zero");
    check(value == 0, "get zero value");

    /* A fractional value is not an integer. */
    check_status(platinum_json_get_int("{\"n\":1.5}", "n", &value),
                 WF_ERR_PARSE, "reject fraction");
    check_status(platinum_json_get_int("{\"n\":1e3}", "n", &value),
                 WF_ERR_PARSE, "reject exponent");
    check_status(platinum_json_get_int("{\"n\":\"1\"}", "n", &value),
                 WF_ERR_PARSE, "reject numeric string");
    check_status(platinum_json_get_int("{\"n\":99999999999999}", "n", &value),
                 WF_ERR_PARSE, "reject overlong integer");
    check_status(platinum_json_get_int("{\"n\":1}", "missing", &value),
                 WF_ERR_NOT_FOUND, "absent int");

    check_status(platinum_json_get_int("{\"a\":[1,2],\"n\":7}", "n", &value),
                 WF_OK, "get int after a nested value");
    check(value == 7, "get int after a nested value result");

    check_status(platinum_json_get_int("{\"a\":true,\"n\":7}", "n", &value),
                 WF_OK, "get int after a literal");
    check(value == 7, "get int after a literal result");
}

static void test_valid(void)
{
    check_status(platinum_json_valid("{\"a\":1}"), WF_OK, "valid object");
    check_status(platinum_json_valid("  {\"a\" : [ 1 , 2 ] }  "), WF_OK,
                 "valid with whitespace");
    check_status(platinum_json_valid("[]"), WF_OK, "valid empty array");
    check_status(platinum_json_valid("{}"), WF_OK, "valid empty object");
    check_status(platinum_json_valid("\"s\""), WF_OK, "valid bare string");
    check_status(platinum_json_valid("-1.5e10"), WF_OK, "valid number");
    check_status(platinum_json_valid("true"), WF_OK, "valid literal");
    check_status(platinum_json_valid("null"), WF_OK, "valid null");

    check_status(platinum_json_valid(""), WF_ERR_PARSE, "empty document");
    check_status(platinum_json_valid("{"), WF_ERR_PARSE, "unclosed object");
    check_status(platinum_json_valid("{\"a\":1"), WF_ERR_PARSE,
                 "unclosed member");
    check_status(platinum_json_valid("{\"a\":}"), WF_ERR_PARSE, "missing value");
    check_status(platinum_json_valid("{\"a\":1,}"), WF_ERR_PARSE,
                 "trailing comma");
    check_status(platinum_json_valid("{\"a\":1} extra"), WF_ERR_PARSE,
                 "trailing content");
    check_status(platinum_json_valid("{}{}"), WF_ERR_PARSE,
                 "two documents");
    check_status(platinum_json_valid("{\"a\":01}"), WF_ERR_PARSE,
                 "leading zero");
    check_status(platinum_json_valid("{\"a\":.5}"), WF_ERR_PARSE,
                 "missing integer part");
    check_status(platinum_json_valid("{\"a\":+1}"), WF_ERR_PARSE,
                 "leading plus");
    check_status(platinum_json_valid("{\"a\":1,}"), WF_ERR_PARSE,
                 "trailing comma in object");
    check_status(platinum_json_valid("[1 2]"), WF_ERR_PARSE,
                 "missing comma");
    check_status(platinum_json_valid("{\"a\":\"b}"), WF_ERR_PARSE,
                 "unterminated string in valid");

    /* Nesting past the depth cap must be refused rather than recursed into.
     * The cap counts array/object levels, so 15 levels are accepted and 16
     * are not. */
    check_status(platinum_json_valid("[[[[[[[[[[[[[[[1]]]]]]]]]]]]]]]"),
                 WF_OK, "at the depth limit");
    check_status(platinum_json_valid("[[[[[[[[[[[[[[[[1]]]]]]]]]]]]]]]]"),
                 WF_ERR_PARSE, "one level too deep");
}


static void test_open(void)
{
    platinum_json root;

    check_status(platinum_json_open(&root, "{\"a\":1}"), WF_OK, "open object");
    check(root.text != NULL && root.at != NULL, "open sets both fields");

    check_status(platinum_json_open(&root, NULL), WF_ERR_INVALID_ARG,
                 "open rejects null body");
    check_status(platinum_json_open(NULL, "{}"), WF_ERR_INVALID_ARG,
                 "open rejects null cursor");

    /* The whole document is validated at open, not as it is walked, so a body
     * that is well formed where the wanted member sits and truncated after it
     * is rejected rather than half-read. */
    check_status(platinum_json_open(&root, "{\"a\":1"), WF_ERR_PARSE,
                 "open rejects truncated object");
    check_status(platinum_json_open(&root, "{\"a\":1} trailing"), WF_ERR_PARSE,
                 "open rejects trailing content");
    check_status(platinum_json_open(&root, "{\"a\":\"\\x\"}"), WF_ERR_PARSE,
                 "open rejects unknown escape");

    check_status(platinum_json_open(&root, ""), WF_ERR_PARSE,
                 "open rejects empty body");
}

static void test_member(void)
{
    static const char *doc =
        "{\"protocol\":1,\"author\":{\"did\":\"did:plc:abc\",\"n\":2},"
        "\"cursor\":\"c1\"}";
    platinum_json root;
    platinum_json author;
    platinum_json nested;
    long value = 0;

    check_status(platinum_json_open(&root, doc), WF_OK, "member: open");
    check_status(platinum_json_member(root, "author", &author), WF_OK,
                 "member: object found");

    check_status(platinum_json_string(author, "did", NULL, 0),
                 WF_ERR_INVALID_ARG, "member: zero capacity rejected");
    check_status(platinum_json_member(root, "author", NULL),
                 WF_ERR_INVALID_ARG, "member: null cursor rejected");

    check_status(platinum_json_int(author, "n", &value), WF_OK,
                 "member: nested int");
    check(value == 2, "member: nested int value");

    check_status(platinum_json_member(root, "missing", &nested),
                 WF_ERR_NOT_FOUND, "member: absent member");

    /* Looking for a member inside something that is not an object is a shape
     * error, not a silently absent member: the caller asked for a sub-object
     * and got a string, which is a different response than one without it. */
    check_status(platinum_json_member(root, "cursor", &nested), WF_OK,
                 "member: cursor member found");
    check_status(platinum_json_member(nested, "anything", &author),
                 WF_ERR_PARSE, "member: string is not an object");
}

static void test_element(void)
{
    static const char *doc =
        "{\"posts\":[{\"uri\":\"at://a\"},{\"uri\":\"at://b\"},"
        "{\"uri\":\"at://c\"}],\"cursor\":\"z\"}";
    platinum_json root;
    platinum_json posts;
    platinum_json item;
    long count = 0;
    char uri[64];

    check_status(platinum_json_open(&root, doc), WF_OK, "element: open");
    check_status(platinum_json_member(root, "posts", &posts), WF_OK,
                 "element: array found");
    check_status(platinum_json_count(posts, &count), WF_OK, "element: count");
    check(count == 3, "element: count value");

    check_status(platinum_json_element(posts, 0, &item), WF_OK,
                 "element: first");
    check_status(platinum_json_string(item, "uri", uri, sizeof(uri)), WF_OK,
                 "element: first uri read");
    check_str(uri, "at://a", "element: first uri value");

    check_status(platinum_json_element(posts, 2, &item), WF_OK,
                 "element: last");
    check_status(platinum_json_string(item, "uri", uri, sizeof(uri)), WF_OK,
                 "element: last uri read");
    check_str(uri, "at://c", "element: last uri value");

    check_status(platinum_json_element(posts, 3, &item), WF_ERR_NOT_FOUND,
                 "element: past the end");
    check_status(platinum_json_element(posts, -1, &item), WF_ERR_INVALID_ARG,
                 "element: negative index rejected");

    check_status(platinum_json_count(root, &count), WF_ERR_PARSE,
                 "element: count on an object is a shape error");
}

static void test_empty_array(void)
{
    static const char *doc = "{\"posts\":[]}";
    platinum_json root;
    platinum_json posts;
    platinum_json item;
    long count = -1;

    check_status(platinum_json_open(&root, doc), WF_OK, "empty: open");
    check_status(platinum_json_member(root, "posts", &posts), WF_OK,
                 "empty: array found");
    check_status(platinum_json_count(posts, &count), WF_OK,
                 "empty: count succeeds");
    check(count == 0, "empty: count is zero");
    check_status(platinum_json_element(posts, 0, &item), WF_ERR_NOT_FOUND,
                 "empty: no first element");
}

static void test_nested_array_of_objects(void)
{
    static const char *doc =
        "{\"items\":["
        "{\"author\":{\"handle\":\"alice\",\"displayName\":\"Alice A\"},"
        "\"likeCount\":3,\"read\":true},"
        "{\"author\":{\"handle\":\"bob\"},\"likeCount\":0,\"read\":false}"
        "]}";
    platinum_json root;
    platinum_json items;
    platinum_json item;
    platinum_json author;
    char handle[32];
    char name[32];
    long likes;
    int read_flag;

    check_status(platinum_json_open(&root, doc), WF_OK, "nested: open");
    check_status(platinum_json_member(root, "items", &items), WF_OK,
                 "nested: array found");

    check_status(platinum_json_element(items, 0, &item), WF_OK,
                 "nested: first element");
    check_status(platinum_json_member(item, "author", &author), WF_OK,
                 "nested: author found");
    check_status(platinum_json_string(author, "handle", handle,
                                      sizeof(handle)), WF_OK,
                 "nested: handle read");
    check_str(handle, "alice", "nested: handle value");
    check_status(platinum_json_string(author, "displayName", name,
                                      sizeof(name)), WF_OK,
                 "nested: displayName read");
    check_str(name, "Alice A", "nested: displayName value");
    check_status(platinum_json_int(item, "likeCount", &likes), WF_OK,
                 "nested: likeCount read");
    check(likes == 3, "nested: likeCount value");
    check_status(platinum_json_bool(item, "read", &read_flag), WF_OK,
                 "nested: read read");
    check(read_flag == 1, "nested: read is true");

    check_status(platinum_json_element(items, 1, &item), WF_OK,
                 "nested: second element");
    check_status(platinum_json_member(item, "author", &author), WF_OK,
                 "nested: second author found");
    check_status(platinum_json_bool(item, "read", &read_flag), WF_OK,
                 "nested: second read read");
    check(read_flag == 0, "nested: second read is false");
    check_status(platinum_json_int(item, "likeCount", &likes), WF_OK,
                 "nested: second likeCount read");
    check(likes == 0, "nested: second likeCount is zero");
    check_status(platinum_json_string(author, "displayName", name,
                                      sizeof(name)), WF_ERR_NOT_FOUND,
                 "nested: absent displayName is not found");
}

static void test_bool_rejects_other_types(void)
{
    static const char *doc =
        "{\"a\":true,\"b\":false,\"c\":\"true\",\"d\":1,\"e\":null}";
    platinum_json root;
    int flag = -1;

    check_status(platinum_json_open(&root, doc), WF_OK, "bool: open");
    check_status(platinum_json_bool(root, "a", &flag), WF_OK, "bool: true");
    check(flag == 1, "bool: true value");
    check_status(platinum_json_bool(root, "b", &flag), WF_OK, "bool: false");
    check(flag == 0, "bool: false value");

    /* JSON has no truthiness: a string that reads "true" is not a boolean and
     * a number is not either. Coercing either would let a response turn a
     * notification's read flag into whatever it liked. */
    check_status(platinum_json_bool(root, "c", &flag), WF_ERR_PARSE,
                 "bool: string is not a boolean");
    check(flag == 0, "bool: rejected read leaves the output clear");
    check_status(platinum_json_bool(root, "d", &flag), WF_ERR_PARSE,
                 "bool: number is not a boolean");
    check_status(platinum_json_bool(root, "e", &flag), WF_ERR_PARSE,
                 "bool: null is not a boolean");
    check_status(platinum_json_bool(root, "missing", &flag),
                 WF_ERR_NOT_FOUND, "bool: absent member");
    check_status(platinum_json_bool(root, "a", NULL), WF_ERR_INVALID_ARG,
                 "bool: null out rejected");
}

static void test_string_truncating(void)
{
    static const char *doc =
        "{\"short\":\"abc\",\"long\":\"abcdefghij\",\"esc\":\"a\\u00e9b\","
        "\"longEsc\":\"ab\\u00e9cdef\"}";
    platinum_json root;
    char small[5];
    char roomy[32];

    check_status(platinum_json_open(&root, doc), WF_OK, "clip: open");

    /* A value that fits is identical under both accessors. */
    check_status(platinum_json_string(root, "short", roomy, sizeof(roomy)),
                 WF_OK, "clip: short fits strictly");
    check_str(roomy, "abc", "clip: short value");

    check_status(platinum_json_string_truncating(root, "short", roomy,
                                                 sizeof(roomy)), WF_OK,
                 "clip: short fits loosely");
    check_str(roomy, "abc", "clip: short value loosely");

    /* The strict accessor refuses rather than half-write. */
    check_status(platinum_json_string(root, "long", small, sizeof(small)),
                 WF_ERR_ALLOC, "clip: strict refuses an oversized value");

    /* The truncating one clips, stays NUL-terminated and stays inside cap. */
    check_status(platinum_json_string_truncating(root, "long", small,
                                                 sizeof(small)), WF_OK,
                 "clip: truncating accepts an oversized value");
    check_str(small, "abcd", "clip: clipped value");
    check(strlen(small) == 4, "clip: clipped value is NUL-terminated in cap");

    /* Clipping must not cut an escape sequence in half: the longest prefix that
     * is whole units is returned, which here is "a" plus the two-byte é. */
    check_status(platinum_json_string_truncating(root, "longEsc", small,
                                                 sizeof(small)), WF_OK,
                 "clip: oversized escape clipped");
    check(strlen(small) == 4, "clip: escape clip stops on a unit boundary");
    check(small[0] == 'a' && small[1] == 'b' &&
          (unsigned char)small[2] == 0xc3 &&
          (unsigned char)small[3] == 0xa9,
          "clip: escaped byte is not split");

    check_status(platinum_json_string_truncating(root, "missing", roomy,
                                                 sizeof(roomy)),
                 WF_ERR_NOT_FOUND, "clip: absent member");
    check_status(platinum_json_string_truncating(root, "esc", roomy, 0),
                 WF_ERR_INVALID_ARG, "clip: zero capacity rejected");

    check_status(platinum_json_string_truncating(root, "esc", roomy,
                                                 sizeof(roomy)), WF_OK,
                 "clip: escape fits");
    check(strlen(roomy) == 4, "clip: escape decoded to four bytes");
}

static void test_cursor_against_bad_documents(void)
{
    platinum_json root;
    platinum_json value;
    char buf[32];

    /* A truncated array must not yield a partial element. */
    check_status(platinum_json_open(&root, "[{\"a\":1},{\"b\":"), WF_ERR_PARSE,
                 "bad: truncated array rejected at open");

    /* Same for a body that is fine up to the member and broken after it: the
     * flat accessor already refused this, and the cursor must too. */
    check_status(platinum_json_open(&root, "{\"a\":\"ok\",\"b\":tru"), WF_ERR_PARSE,
                 "bad: truncated tail rejected at open");
    check_status(platinum_json_open(&root, "{\"a\":\"ok\",\"b\":1}"),
                 WF_OK, "bad: intact body opens");
    check_status(platinum_json_string(root, "a", buf, sizeof(buf)), WF_OK,
                 "bad: intact body reads");

    /* A zeroed cursor is not a valid cursor. */
    {
        platinum_json empty;

        empty.text = NULL;
        empty.at = NULL;
        check_status(platinum_json_member(empty, "a", &value),
                     WF_ERR_INVALID_ARG, "bad: zeroed cursor rejected");
        check_status(platinum_json_element(empty, 0, &value),
                     WF_ERR_INVALID_ARG, "bad: zeroed cursor element rejected");
        check_status(platinum_json_count(empty, NULL), WF_ERR_INVALID_ARG,
                     "bad: zeroed cursor count rejected");
    }
}

int main(void)
{
    test_escape();
    test_get_string();
    test_get_int();
    test_valid();
    test_open();
    test_member();
    test_element();
    test_empty_array();
    test_nested_array_of_objects();
    test_bool_rejects_other_types();
    test_string_truncating();
    test_cursor_against_bad_documents();

    if (failures != 0) {
        printf("test_json_min: %d of %d checks failed\n", failures, checks);
        return 1;
    }

    printf("test_json_min: all checks passed\n");
    return 0;
}
