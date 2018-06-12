#include <langinfo.h>
#include <locale.h>
#include "test.h"
#include "../src/editor.h"
#include "../src/path.h"
#include "../src/encoding.h"
#include "../src/filetype.h"
#include "../src/lookup/xterm-keys.c"

unsigned int failed;

void fail(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    failed += 1;
}

static void test_relative_filename(void)
{
    static const struct rel_test {
        const char *cwd, *path, *result;
    } tests[] = { // NOTE: at most 2 ".." components allowed in relative name
        { "/", "/", "/" },
        { "/", "/file", "file" },
        { "/a/b/c/d", "/a/b/file", "../../file" },
        { "/a/b/c/d/e", "/a/b/file", "/a/b/file" },
        { "/a/foobar", "/a/foo/file", "../foo/file" },
    };
    for (size_t i = 0; i < ARRAY_COUNT(tests); i++) {
        const struct rel_test *t = &tests[i];
        char *result = relative_filename(t->path, t->cwd);
        EXPECT_STREQ(t->result, result);
        free(result);
    }
}

static void test_detect_encoding_from_bom(void)
{
    static const struct bom_test {
        const char *encoding;
        const unsigned char *text;
        size_t size;
    } tests[] = {
        {"UTF-8", "\xef\xbb\xbfHello", 8},
        {"UTF-32BE", "\x00\x00\xfe\xffHello", 9},
        {"UTF-32LE", "\xff\xfe\x00\x00Hello", 9},
        {"UTF-16BE", "\xfe\xffHello", 7},
        {"UTF-16LE", "\xff\xfeHello", 7},
        {NULL, "\x00\xef\xbb\xbfHello", 9},
        {NULL, "\xef\xbb", 2},
    };
    for (size_t i = 0; i < ARRAY_COUNT(tests); i++) {
        const struct bom_test *t = &tests[i];
        const char *result = detect_encoding_from_bom(t->text, t->size);
        EXPECT_STREQ(result, t->encoding);
    }
}

static void test_find_ft(void)
{
    static const struct ft_test {
        const char *filename, *expected_filetype;
    } tests[] = {
        {"/usr/local/include/lib.h", "c"},
        {"test.cc~", "c"},
        {"test.c.pacnew", "c"},
        {"test.c.pacnew~", "c"},
        {"test.lua", "lua"},
        {"test.py", "python"},
        {"makefile", "make"},
        {"GNUmakefile", "make"},
        {".file.yml", "yaml"},
        {"/etc/nginx.conf", "nginx"},
        {"file.c.old~", "c"},
        {"file..rb", "ruby"},
        {"file.rb", "ruby"},
        {"", NULL},
        {"/", NULL},
        {"/etc../etc.c.old/c.old", NULL},
    };
    for (size_t i = 0; i < ARRAY_COUNT(tests); i++) {
        const struct ft_test *t = &tests[i];
        const char *result = find_ft(t->filename, NULL, NULL, 0);
        EXPECT_STREQ(result, t->expected_filetype);
    }
}

static void test_parse_xterm_key_sequence(void)
{
    Key key;
    ssize_t size;

    size = parse_xterm_key_sequence("\033[Z", 3, &key);
    EXPECT_EQ(size, 3);
    EXPECT_EQ(key, MOD_SHIFT | '\t');

    size = parse_xterm_key_sequence("\033[1;2A", 6, &key);
    EXPECT_EQ(size, 6);
    EXPECT_EQ(key, MOD_SHIFT | KEY_UP);

    size = parse_xterm_key_sequence("\033[6;8~", 6, &key);
    EXPECT_EQ(size, 6);
    EXPECT_EQ(key, MOD_SHIFT | MOD_META | MOD_CTRL | KEY_PAGE_DOWN);

    size = parse_xterm_key_sequence("\033[1;2", 5, &key);
    EXPECT_EQ(size, -1);

    size = parse_xterm_key_sequence("\033[", 2, &key);
    EXPECT_EQ(size, -1);

    size = parse_xterm_key_sequence("\033O", 2, &key);
    EXPECT_EQ(size, -1);

    size = parse_xterm_key_sequence("\033O\033", 3, &key);
    EXPECT_EQ(size, 0);
}

void test_key_to_string(void);

int main(void)
{
    init_editor_state();

    test_relative_filename();
    test_detect_encoding_from_bom();
    test_find_ft();
    test_parse_xterm_key_sequence();
    test_key_to_string();

    return failed ? 1 : 0;
}
