#include "ascii.h"

enum {
    S = ASCII_SPACE,
    L = ASCII_LOWER,
    U = ASCII_UPPER,
    D = ASCII_DIGIT,
    u = ASCII_UNDERSCORE,
    N = ASCII_NONASCII,
};

const uint8_t ascii_table[256] = {
    ['\t'] = S, ['\n'] = S, ['\r'] = S, [' '] = S,
    ['_'] = u,

    ['0'] = D, ['1'] = D, ['2'] = D, ['3'] = D, ['4'] = D,
    ['5'] = D, ['6'] = D, ['7'] = D, ['8'] = D, ['9'] = D,

    ['A'] = U, ['B'] = U, ['C'] = U, ['D'] = U, ['E'] = U, ['F'] = U,
    ['G'] = U, ['H'] = U, ['I'] = U, ['J'] = U, ['K'] = U, ['L'] = U,
    ['M'] = U, ['N'] = U, ['O'] = U, ['P'] = U, ['Q'] = U, ['R'] = U,
    ['S'] = U, ['T'] = U, ['U'] = U, ['V'] = U, ['W'] = U, ['X'] = U,
    ['Y'] = U, ['Z'] = U,

    ['a'] = L, ['b'] = L, ['c'] = L, ['d'] = L, ['e'] = L, ['f'] = L,
    ['g'] = L, ['h'] = L, ['i'] = L, ['j'] = L, ['k'] = L, ['l'] = L,
    ['m'] = L, ['n'] = L, ['o'] = L, ['p'] = L, ['q'] = L, ['r'] = L,
    ['s'] = L, ['t'] = L, ['u'] = L, ['v'] = L, ['w'] = L, ['x'] = L,
    ['y'] = L, ['z'] = L,

    [0x7F] = 0,
    N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, // 0x80 .. 0x8F
    N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, // 0x90 .. 0x9F
    N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, // 0xA0 .. 0xAF
    N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, // 0xB0 .. 0xBF
    N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, // 0xC0 .. 0xCF
    N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, // 0xD0 .. 0xDF
    N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, // 0xE0 .. 0xEF
    N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, // 0xF0 .. 0xFF
};

int hex_decode(int ch)
{
    switch (ch) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        return ch - '0';
    case 'a': case 'b': case 'c': case 'd': case 'e':
    case 'f':
        return ch - 'a' + 10;
    case 'A': case 'B': case 'C': case 'D': case 'E':
    case 'F':
        return ch - 'A' + 10;
    default:
        return -1;
    }
}
