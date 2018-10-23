#ifndef TERMINAL_TERMINFO_H
#define TERMINAL_TERMINFO_H

#include <stdbool.h>
#include <sys/types.h>
#include "color.h"
#include "key.h"
#include "../util/string-view.h"

typedef struct {
    StringView init;
    StringView deinit;
    StringView reset_colors;
    StringView reset_attrs;
    StringView keypad_off;
    StringView keypad_on;
    StringView cup_mode_off;
    StringView cup_mode_on;
    StringView show_cursor;
    StringView hide_cursor;
} TermControlCodes;

typedef struct {
    bool back_color_erase;
    TermColorCapabilityType color_type;
    int width;
    int height;
    unsigned short ncv_attributes;
    TermControlCodes control_codes;
    ssize_t (*parse_key_sequence)(const char *buf, size_t length, KeyCode *key);
    void (*clear_to_eol)(void);
    void (*set_color)(const TermColor *color);
    void (*move_cursor)(int x, int y);
    void (*repeat_byte)(char ch, size_t count);
    void (*raw)(void);
    void (*cooked)(void);
    void (*save_title)(void);
    void (*restore_title)(void);
    void (*set_title)(const char *title);
} TerminalInfo;

extern TerminalInfo terminal;

void term_init(void);

#endif
