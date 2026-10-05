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

int main(void)
{
    test_escape();
    test_get_string();
    test_get_int();
    test_valid();

    if (failures != 0) {
        printf("test_json_min: %d of %d checks failed\n", failures, checks);
        return 1;
    }

    printf("test_json_min: all checks passed\n");
    return 0;
}
