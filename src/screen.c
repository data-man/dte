#include <stdio.h>
#include "screen.h"
#include "editor.h"
#include "frame.h"
#include "terminal/no-op.h"
#include "terminal/output.h"
#include "terminal/terminal.h"
#include "terminal/winsize.h"
#include "util/xsnprintf.h"
#include "view.h"

void set_color(const TermColor *color)
{
    TermColor tmp = *color;
    // NOTE: -2 (keep) is treated as -1 (default)
    if (tmp.fg < 0) {
        tmp.fg = builtin_colors[BC_DEFAULT].fg;
    }
    if (tmp.bg < 0) {
        tmp.bg = builtin_colors[BC_DEFAULT].bg;
    }
    if (same_color(&tmp, &obuf.color)) {
        return;
    }
    terminal.set_color(&tmp);
    obuf.color = tmp;
}

void set_builtin_color(BuiltinColorEnum c)
{
    set_color(&builtin_colors[c]);
}

void update_term_title(const Buffer *b)
{
    if (!editor.options.set_window_title || terminal.set_title == no_op_s) {
        return;
    }

    // FIXME: title must not contain control characters
    char title[1024];
    snprintf (
        title,
        sizeof title,
        "%s %c dte",
        buffer_filename(b),
        buffer_modified(b) ? '+' : '-'
    );

    terminal.set_title(title);
}

void mask_color(TermColor *color, const TermColor *over)
{
    if (over->fg != -2) {
        color->fg = over->fg;
    }
    if (over->bg != -2) {
        color->bg = over->bg;
    }
    if (!(over->attr & ATTR_KEEP)) {
        color->attr = over->attr;
    }
}

static void print_separator(Window *win)
{
    if (win->x + win->w == terminal.width) {
        return;
    }
    for (int y = 0, h = win->h; y < h; y++) {
        term_move_cursor(win->x + win->w, win->y + y);
        term_add_byte('|');
    }
}

void update_separators(void)
{
    set_builtin_color(BC_STATUSLINE);
    for_each_window(print_separator);
}

void update_line_numbers(Window *win, bool force)
{
    const View *v = win->view;
    size_t lines = v->buffer->nl;
    int x = win->x;

    calculate_line_numbers(win);

    long first = v->vy + 1;
    long last = v->vy + win->edit_h;
    if (last > lines) {
        last = lines;
    }

    if (
        !force
        && win->line_numbers.first == first
        && win->line_numbers.last == last
    ) {
        return;
    }

    win->line_numbers.first = first;
    win->line_numbers.last = last;

    term_output_reset(win->x, win->w, 0);
    set_builtin_color(BC_LINENUMBER);
    for (int i = 0, n = win->edit_h; i < n; i++) {
        long line = v->vy + i + 1;
        int w = win->line_numbers.width - 1;
        char buf[32];

        if (line > lines) {
            xsnprintf(buf, sizeof(buf), "%*s ", w, "");
        } else {
            xsnprintf(buf, sizeof(buf), "%*ld ", w, line);
        }
        term_move_cursor(x, win->edit_y + i);
        term_add_bytes(buf, win->line_numbers.width);
    }
}

void update_window_sizes(void)
{
    set_frame_size(root_frame, terminal.width, terminal.height - 1);
    update_window_coordinates();
}

void update_screen_size(void)
{
    if (term_get_size(&terminal.width, &terminal.height)) {
        if (terminal.width < 3) {
            terminal.width = 3;
        }
        if (terminal.height < 3) {
            terminal.height = 3;
        }
        update_window_sizes();
    }
}
