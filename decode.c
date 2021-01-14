/**
 * Original LZSS encoder-decoder  (c) Haruhiko Okumura
 */

#include "lzss.h"

#include <unistd.h>
#include <string.h>

static int getbits(int fd, int n, uint32_t *out);

void lzss_decode(int ifd, int ofd)
{
    uint8_t window[WSIZE];
    uint32_t r, c;
    memset(window, ' ', (N-F));
    r = (N-F); /* first index of lookahead buffer */

    while (getbits(ifd, 1, &c) != -1) {
        if (c) {
            /* first bit indicates a literal byte */
            if (getbits(ifd, 8, &c) == -1)
                break;
            write(ofd, &c, sizeof(uint8_t));
            window[r++] = (uint8_t)c;
            r &= (N - 1);
        } else {
            /* next few bits are the (offset, length) pair */
            uint32_t offset = 0, length = 0, k;
            if (getbits(ifd, OFFSET_BITS, &offset) == -1)
                break;
            if (getbits(ifd, LENGTH_BITS, &length) == -1)
                break;

            /**
             * Write length+P+1 bytes to disk starting at `offset` and copy
             * the bytes to window for later lookup.
             */
            for (k = 0; k < length + (P+1); k++) {
                c = window[(offset + k) & (N - 1)];
                write(ofd, &c, sizeof(uint8_t));
                window[r++] = (uint8_t)c;
                r &= (N - 1);
            }
        }
    }
}

static int getbits(int fd, int n, uint32_t *out)
{
    /**
     * This function maintains some static variables. The reason for this is
     * because the smallest piece of data you can read from disk is a single
     * byte. When the mask is 0, all bits in `c` have been handled (or `c` needs
     * to be initialized), so you read a single byte from disk and process it.
     * The variable `x` maintains the value of the n bits being processed and
     * returns it.
     */
    int i;
    uint32_t x;
    static uint8_t c;
    static uint32_t mask = 0;

    memset(out, 0, sizeof *out);

    x = 0;
    for (i = 0; i < n; i++) {
        /* get n bits */
        if (mask == 0) {
            if (!read(fd, &c, sizeof(uint8_t)))
                return -1;
            mask = 128;
        }

        x <<= 1;
        if (c & mask)
            x++;
        mask >>= 1;
    }
    *out = x;
    return 0;
}
