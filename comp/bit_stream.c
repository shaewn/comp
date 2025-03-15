#include "bit_stream.h"

BitStream bit_stream_writer(FILE *file) {
    BitStream bs;
    bs.file = file;
    bs.bit_pos = 0;
    bs.byte = 0;

    return bs;
}

BitStream bit_stream_reader(FILE *file) {
    BitStream bs;
    bs.file = file;
    bs.bit_pos = 8;
    bs.byte = 0;

    return bs;
}

void bit_stream_flush(BitStream *writer) {
    fwrite(&writer->byte, 1, 1, writer->file);
    writer->bit_pos = 0;
    writer->byte = 0;
}

void bit_stream_write(BitStream *writer, int bit) {
    int shift;

    if (writer->bit_pos == 8) {
        bit_stream_flush(writer);
    }

    /* backwards, write to msb if bit_pos is 0 */

    shift = 7 - writer->bit_pos++;
    writer->byte |= (bit & 1) << shift;
}

int bit_stream_read(BitStream *reader) {
    int shift;

    if (reader->bit_pos == 8) {
        fread(&reader->byte, 1, 1, reader->file);
        reader->bit_pos = 0;
    }

    shift = 7 - reader->bit_pos++;
    int bit = (reader->byte >> shift) & 1;
    return bit;
}
