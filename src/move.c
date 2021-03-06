#include "move.h"
#include "buffer.h"
#include "indent.h"
#include "util/ascii.h"
#include "util/utf8.h"

typedef enum {
    CT_SPACE,
    CT_NEWLINE,
    CT_WORD,
    CT_OTHER,
} CharTypeEnum;

void move_to_preferred_x(long preferred_x)
{
    StringView line;
    view->preferred_x = preferred_x;
    block_iter_bol(&view->cursor);
    fill_line_ref(&view->cursor, &line);

    if (buffer->options.emulate_tab && view->preferred_x < line.length) {
        const size_t iw = buffer->options.indent_width;
        const size_t ilevel = view->preferred_x / iw;
        for (size_t i = 0; i < line.length && line.data[i] == ' '; i++) {
            if (i + 1 == (ilevel + 1) * iw) {
                // Force cursor to beginning of the indentation level
                view->cursor.offset += ilevel * iw;
                return;
            }
        }
    }

    const unsigned int tw = buffer->options.tab_width;
    unsigned long x = 0;
    size_t i = 0;
    while (x < view->preferred_x && i < line.length) {
        CodePoint u = line.data[i++];
        if (u < 0x80) {
            if (!ascii_iscntrl(u)) {
                x++;
            } else if (u == '\t') {
                x = (x + tw) / tw * tw;
            } else if (u == '\n') {
                break;
            } else {
                x += 2;
            }
        } else {
            const size_t next = i;
            i--;
            u = u_get_nonascii(line.data, line.length, &i);
            x += u_char_width(u);
            if (x > view->preferred_x) {
                i = next;
                break;
            }
        }
    }
    if (x > view->preferred_x) {
        i--;
    }
    view->cursor.offset += i;

    // If cursor stopped on a zero-width char, move to the next spacing char.
    CodePoint u;
    if (block_iter_get_char(&view->cursor, &u) && u_is_zero_width(u)) {
        block_iter_next_column(&view->cursor);
    }
}

void move_cursor_left(void)
{
    if (buffer->options.emulate_tab) {
        size_t size = get_indent_level_bytes_left();
        if (size) {
            block_iter_back_bytes(&view->cursor, size);
            view_reset_preferred_x(view);
            return;
        }
    }
    block_iter_prev_column(&view->cursor);
    view_reset_preferred_x(view);
}

void move_cursor_right(void)
{
    if (buffer->options.emulate_tab) {
        size_t size = get_indent_level_bytes_right();
        if (size) {
            block_iter_skip_bytes(&view->cursor, size);
            view_reset_preferred_x(view);
            return;
        }
    }
    block_iter_next_column(&view->cursor);
    view_reset_preferred_x(view);
}

void move_bol(void)
{
    block_iter_bol(&view->cursor);
    view_reset_preferred_x(view);
}

void move_bol_smart(void)
{
    StringView line;
    const size_t cursor_offset = fetch_this_line(&view->cursor, &line);

    size_t indent_bytes = 0;
    while (ascii_isblank(*line.data++)) {
        indent_bytes++;
    }

    size_t move_bytes;
    if (cursor_offset > indent_bytes) {
        move_bytes = cursor_offset - indent_bytes;
    } else {
        move_bytes = cursor_offset;
    }

    block_iter_back_bytes(&view->cursor, move_bytes);
    view_reset_preferred_x(view);
}

void move_eol(void)
{
    block_iter_eol(&view->cursor);
    view_reset_preferred_x(view);
}

void move_up(long count)
{
    const long x = view_get_preferred_x(view);
    while (count > 0) {
        if (!block_iter_prev_line(&view->cursor)) {
            break;
        }
        count--;
    }
    move_to_preferred_x(x);
}

void move_down(long count)
{
    const long x = view_get_preferred_x(view);
    while (count > 0) {
        if (!block_iter_eat_line(&view->cursor)) {
            break;
        }
        count--;
    }
    move_to_preferred_x(x);
}

void move_bof(void)
{
    block_iter_bof(&view->cursor);
    view_reset_preferred_x(view);
}

void move_eof(void)
{
    block_iter_eof(&view->cursor);
    view_reset_preferred_x(view);
}

void move_to_line(View *v, size_t line)
{
    block_iter_goto_line(&v->cursor, line - 1);
    v->center_on_scroll = true;
}

void move_to_column(View *v, size_t column)
{
    block_iter_bol(&v->cursor);
    while (column-- > 1) {
        CodePoint u;
        if (!block_iter_next_char(&v->cursor, &u)) {
            break;
        }
        if (u == '\n') {
            block_iter_prev_char(&v->cursor, &u);
            break;
        }
    }
    view_reset_preferred_x(v);
}

static CharTypeEnum get_char_type(CodePoint u)
{
    if (u == '\n') {
        return CT_NEWLINE;
    }
    if (u_is_breakable_whitespace(u)) {
        return CT_SPACE;
    }
    if (u_is_word_char(u)) {
        return CT_WORD;
    }
    return CT_OTHER;
}

static bool get_current_char_type(BlockIter *bi, CharTypeEnum *type)
{
    CodePoint u;
    if (!block_iter_get_char(bi, &u)) {
        return false;
    }

    *type = get_char_type(u);
    return true;
}

static size_t skip_fwd_char_type(BlockIter *bi, CharTypeEnum type)
{
    size_t count = 0;
    CodePoint u;
    while (block_iter_next_char(bi, &u)) {
        if (get_char_type(u) != type) {
            block_iter_prev_char(bi, &u);
            break;
        }
        count += u_char_size(u);
    }
    return count;
}

static size_t skip_bwd_char_type(BlockIter *bi, CharTypeEnum type)
{
    size_t count = 0;
    CodePoint u;
    while (block_iter_prev_char(bi, &u)) {
        if (get_char_type(u) != type) {
            block_iter_next_char(bi, &u);
            break;
        }
        count += u_char_size(u);
    }
    return count;
}

size_t word_fwd(BlockIter *bi, bool skip_non_word)
{
    size_t count = 0;
    CharTypeEnum type;

    while (1) {
        count += skip_fwd_char_type(bi, CT_SPACE);
        if (!get_current_char_type(bi, &type)) {
            return count;
        }

        if (
            count
            && (!skip_non_word || (type == CT_WORD || type == CT_NEWLINE))
        ) {
            return count;
        }

        count += skip_fwd_char_type(bi, type);
    }
}

size_t word_bwd(BlockIter *bi, bool skip_non_word)
{
    size_t count = 0;
    CharTypeEnum type;
    CodePoint u;

    do {
        count += skip_bwd_char_type(bi, CT_SPACE);
        if (!block_iter_prev_char(bi, &u)) {
            return count;
        }

        type = get_char_type(u);
        count += u_char_size(u);
        count += skip_bwd_char_type(bi, type);
    } while (skip_non_word && type != CT_WORD && type != CT_NEWLINE);
    return count;
}
