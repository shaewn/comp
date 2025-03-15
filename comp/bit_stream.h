#ifndef BIT_STREAM_H_
#define BIT_STREAM_H_

#include <stdio.h>

typedef struct {
    FILE *file;
    unsigned char byte;
    int bit_pos;
} BitStream;

BitStream bit_stream_writer(FILE *file);
BitStream bit_stream_reader(FILE *file);

void bit_stream_flush(BitStream *writer);
void bit_stream_write(BitStream *writer, int bit);
int bit_stream_read(BitStream *reader);

#endif
