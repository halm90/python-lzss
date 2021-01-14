/**
 * Original LZSS encoder-decoder  (c) Haruhiko Okumura
 */

#include "lzss.h"

#include <unistd.h>
#include <string.h>

uint8_t  window[WSIZE];

/*
 * bit_buffer maintains the bits of a single char, including the first bit
 * flag that indicates a literal byte or (offset, length) pair. When bit_mask
 * decrements to zero, write bit_buffer to disk.
 */
static uint32_t bit_buffer = 0;
static uint32_t bit_mask = 128;

static void putbit1(int fd);
static void putbit0(int fd);
static void flush_bit_buffer(int fd);
static void write_literal(int fd, uint8_t c);
static void write_literals(int fd, uint32_t offset, uint32_t length);
static void write_offset_length(int fd, uint32_t offset, uint32_t length);
static void write_bits(int fd, uint32_t c, uint32_t mask);

void lzss_encode(int ifd, int ofd)
{
    int32_t  r, s, bufferend;
    uint32_t nb;
    int32_t  readlen;

    memset(window, ' ', (N-F));

    nb = WSIZE - (N-F);
    readlen = (int32_t)read(ifd, &window[(N-F)], nb);

    r = (N-F);
    s = 0;
    bufferend = (N-F) + readlen;

    while (r < bufferend) {
        int32_t i, remaining;
        uint8_t c;
        uint32_t offset, length;

        remaining = MIN(F, bufferend-r);
        offset = 0;
        length = 1;

        /* current char in lookahead buffer */
        c = window[r];

        /* go backwards in the search buffer looking for a match */
        for (i = r-1; i >= s; i--) {
            if (window[i] == c) {
                int32_t j;

                /*
                 * Found a match in the search buffer, now search _forward_
                 * through both buffers to determine how long the match is.
                 */
                for (j = 1; j < remaining; j++) {
                    if (window[i + j] != window[r + j])
                        break;
                }

                /* track the longest match */
                if ((uint32_t)j > length) {
                    offset = (uint32_t)i;
                    length = (uint32_t)j;
                }
            }
        }

        if (length == 1) {
            /* possibly wasn't a match, so output single literal `c` */
            write_literal(ofd, c);
        } else if (length > 1 && length <= P) {
            /* write `length` literals from window at `offset` */
            write_literals(ofd, offset, length);
        } else {
            /**
             * If length == P+1, length - (P+1) brings length down to zero.
             * The decoder determines length of a match in the lookup table
             * by copying length + (P+1) characters to the output.
             */
            write_offset_length(ofd, offset & (N - 1), length - (P+1));
        }

        r += (int32_t)length;
        s += (int32_t)length;

        if (r >= WSIZE - F) {
            int32_t rb;

            /* copy second half of window to the first half to make room */
            memmove(window, &window[N], N);

            /**
             * At the start of the main loop, bufferend == i, which is WSIZE.
             * Subtracting bufferend by N essentially halves it so new bytes
             * can be read into `window` starting from `bufferend` to WSIZE-1.
             */
            bufferend -= N;
            r -= N;
            s -= N;

            /* overwrite second half of the window with new bytes from disk */
            rb = (int32_t)read(ifd, &window[bufferend],
                               (uint32_t)(WSIZE - bufferend));
            bufferend += rb;
        }
    }

    flush_bit_buffer(ofd);
}

static void putbit1(int fd)
{
    bit_buffer |= bit_mask;
    if ((bit_mask >>= 1) == 0) {
        /* no more bits left in `bit_buffer` so write it to disk */
        write(fd, &bit_buffer, sizeof(uint8_t));
        bit_buffer = 0;
        bit_mask = 128;
    }
}

static void putbit0(int fd)
{
    if ((bit_mask >>= 1) == 0) {
        /* no more bits left in `bit_buffer` so write it to disk */
        write(fd, &bit_buffer, sizeof(uint8_t));
        bit_buffer = 0;
        bit_mask = 128;
    }
}

static void flush_bit_buffer(int fd)
{
    if (bit_mask != 128)
        write(fd, &bit_buffer, sizeof(uint8_t));
}

static void write_literal(int fd, uint8_t c)
{
    uint32_t mask = 256;
    putbit1(fd);    /* indicates a literal byte follows */
    write_bits(fd, c, mask);
}

static void write_literals(int fd, uint32_t offset, uint32_t length)
{
    uint32_t i;

    for (i = offset; i < offset+length; i++) {
        write_literal(fd, window[i]);
    }
}

static void write_offset_length(int fd, uint32_t offset, uint32_t length)
{
    uint32_t mask;

    /* indicates that a (offset, length) pair follows */
    putbit0(fd);

    /* write offset */
    mask = (1 << OFFSET_BITS);
    write_bits(fd, offset, mask);

    /* write length */
    mask = (1 << LENGTH_BITS);
    write_bits(fd, length, mask);
}

static void write_bits(int fd, uint32_t c, uint32_t mask)
{
    while (mask >>= 1) {
        if (c & mask)
            putbit1(fd);
        else
            putbit0(fd);
    }
}
