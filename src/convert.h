#ifndef ENCODING_CONVERT_H
#define ENCODING_CONVERT_H

#include <stdbool.h>
#include <sys/types.h>
#include "encoding.h"
#include "util/macros.h"

typedef struct FileDecoder FileDecoder;
typedef struct FileEncoder FileEncoder;

bool conversion_supported_by_iconv(const char *from, const char *to) NONNULL_ARGS;

FileDecoder *new_file_decoder(const char *encoding, const unsigned char *buf, size_t size);
void free_file_decoder(FileDecoder *dec);
bool file_decoder_read_line(FileDecoder *dec, char **line, size_t *len) NONNULL_ARGS;
const char *file_decoder_get_encoding(const FileDecoder *dec) NONNULL_ARGS;

FileEncoder *new_file_encoder(const Encoding *encoding, bool crlf, int fd) NONNULL_ARGS;
void free_file_encoder(FileEncoder *enc);
ssize_t file_encoder_write(FileEncoder *enc, const unsigned char *buf, size_t size) NONNULL_ARGS;
size_t file_encoder_get_nr_errors(const FileEncoder *enc) NONNULL_ARGS;

#endif
