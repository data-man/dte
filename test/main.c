#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "test.h"
#include "bind.h"
#include "block.h"
#include "buffer.h"
#include "commands.h"
#include "editor.h"
#include "regexp.h"
#include "util/path.h"
#include "util/str-util.h"
#include "util/xmalloc.h"

void test_cmdline(void);
void test_command(void);
void test_editorconfig(void);
void test_encoding(void);
void test_filetype(void);
void test_history(void);
void test_options(void);
void test_syntax(void);
void test_terminal(void);
void test_util(void);
void init_headless_mode(void);
void test_config(void);

static void test_handle_binding(void)
{
    handle_command(&commands, "bind ^A 'insert zzz'; open", false);

    // Bound command should be cached
    const KeyBinding *binding = lookup_binding(MOD_CTRL | 'A');
    const Command *insert = find_normal_command("insert");
    ASSERT_NONNULL(binding);
    ASSERT_NONNULL(insert);
    EXPECT_PTREQ(binding->cmd, insert);
    EXPECT_EQ(binding->a.nr_flags, 0);
    EXPECT_EQ(binding->a.nr_args, 1);
    EXPECT_STREQ(binding->a.args[0], "zzz");
    EXPECT_NULL(binding->a.args[1]);

    handle_binding(MOD_CTRL | 'A');
    const Block *block = BLOCK(buffer->blocks.next);
    ASSERT_NONNULL(block);
    ASSERT_EQ(block->size, 4);
    EXPECT_EQ(block->nl, 1);
    EXPECT_TRUE(mem_equal(block->data, "zzz\n", 4));
    EXPECT_TRUE(undo());
    EXPECT_EQ(block->size, 0);
    EXPECT_EQ(block->nl, 0);
    EXPECT_FALSE(undo());
    handle_command(&commands, "close", false);
}

static void test_posix_sanity(void)
{
    // This is not guaranteed by ISO C99, but it is required by POSIX
    // and is relied upon by this codebase:
    ASSERT_EQ(CHAR_BIT, 8);
    ASSERT_TRUE(sizeof(int) >= 4);

    IGNORE_WARNING("-Wformat-truncation")

    // Some snprintf(3) implementations historically returned -1 in case of
    // truncation. C99 and POSIX 2001 both require that it return the full
    // size of the formatted string, as if there had been enough space.
    char buf[8] = "........";
    ASSERT_EQ(snprintf(buf, 8, "0123456789"), 10);
    ASSERT_EQ(buf[7], '\0');
    EXPECT_STREQ(buf, "0123456");

    // C99 and POSIX 2001 also require the same behavior as above when the
    // size argument is 0 (and allow the buffer argument to be NULL).
    ASSERT_EQ(snprintf(NULL, 0, "987654321"), 9);

    UNIGNORE_WARNINGS
}

static void init_test_environment(void)
{
    char *home = path_absolute("build/test/HOME");
    char *dte_home = path_absolute("build/test/DTE_HOME");
    ASSERT_NONNULL(home);
    ASSERT_NONNULL(dte_home);

    ASSERT_TRUE(mkdir(home, 0755) == 0 || errno == EEXIST);
    ASSERT_TRUE(mkdir(dte_home, 0755) == 0 || errno == EEXIST);

    ASSERT_EQ(setenv("HOME", home, true), 0);
    ASSERT_EQ(setenv("DTE_HOME", dte_home, true), 0);
    ASSERT_EQ(setenv("XDG_RUNTIME_DIR", dte_home, true), 0);

    free(home);
    free(dte_home);

    ASSERT_EQ(unsetenv("TERM"), 0);
    ASSERT_EQ(unsetenv("COLORTERM"), 0);

    init_editor_state();

    const char *ver = getenv("DTE_VERSION");
    EXPECT_NONNULL(ver);
    EXPECT_STREQ(ver, editor.version);
}

int main(void)
{
    test_posix_sanity();
    init_test_environment();

    test_command();
    test_options();
    test_editorconfig();
    test_encoding();
    test_filetype();
    test_util();
    test_terminal();
    test_cmdline();
    test_history();
    test_syntax();

    init_headless_mode();
    test_config();
    test_handle_binding();
    free_syntaxes();

    return failed ? 1 : 0;
}
