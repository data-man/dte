#ifndef TERMINAL_XTERM_H
#define TERMINAL_XTERM_H

#include <sys/types.h>
#include "color.h"
#include "key.h"

void xterm_save_title(void);
void xterm_restore_title(void);
void xterm_set_title(const char *title);
ssize_t xterm_parse_key(const char *buf, size_t length, KeyCode *k);

#endif
