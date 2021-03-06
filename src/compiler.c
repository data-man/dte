#include <stdlib.h>
#include "compiler.h"
#include "completion.h"
#include "error.h"
#include "regexp.h"
#include "util/hashmap.h"
#include "util/macros.h"
#include "util/str-util.h"
#include "util/xmalloc.h"

static HashMap compilers = HASHMAP_INIT;

static Compiler *add_compiler(const char *name)
{
    Compiler *c = find_compiler(name);
    if (c) {
        return c;
    }
    c = xnew0(Compiler, 1);
    hashmap_insert(&compilers, xstrdup(name), c);
    return c;
}

Compiler *find_compiler(const char *name)
{
    return hashmap_get(&compilers, name);
}

void add_error_fmt (
    const char *compiler,
    bool ignore,
    const char *format,
    char **desc
) {
    static const char names[][8] = {"file", "line", "column", "message"};
    int idx[ARRAY_COUNT(names)] = {-1, -1, -1, 0};

    for (size_t i = 0, j = 0; desc[i]; i++) {
        for (j = 0; j < ARRAY_COUNT(names); j++) {
            if (streq(desc[i], names[j])) {
                idx[j] = ((int)i) + 1;
                break;
            }
        }
        if (streq(desc[i], "_")) {
            continue;
        }
        if (unlikely(j == ARRAY_COUNT(names))) {
            error_msg("Unknown substring name %s.", desc[i]);
            return;
        }
    }

    ErrorFormat *f = xnew(ErrorFormat, 1);
    f->ignore = ignore;
    f->msg_idx = idx[3];
    f->file_idx = idx[0];
    f->line_idx = idx[1];
    f->column_idx = idx[2];

    if (!regexp_compile(&f->re, format, 0)) {
        free(f);
        return;
    }

    for (size_t i = 0; i < ARRAY_COUNT(idx); i++) {
        // NOTE: -1 is larger than 0UL
        if (unlikely(idx[i] > (int)f->re.re_nsub)) {
            error_msg("Invalid substring count.");
            regfree(&f->re);
            free(f);
            return;
        }
    }

    ptr_array_append(&add_compiler(compiler)->error_formats, f);
}

void collect_compilers(const char *prefix)
{
    for (HashMapIter it = hashmap_iter(&compilers); hashmap_next(&it); ) {
        const char *name = it.entry->key;
        if (str_has_prefix(name, prefix)) {
            add_completion(xstrdup(name));
        }
    }
}
