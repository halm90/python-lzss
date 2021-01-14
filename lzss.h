/**
 * Original LZSS encoder-decoder  (c) Haruhiko Okumura
 */

#ifndef LZSS_H
#define LZSS_H

#include <stdint.h>

#define OFFSET_BITS 11
#define LENGTH_BITS 7

#define P     2                  /* If match length <= P, output P literals */
#define N     (1 << OFFSET_BITS) /* half of the buffer size */
#define WSIZE (N*2)              /* total size of the buffer */

#define F   ((1 << LENGTH_BITS) + P)    /* lookahead buffer size */

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#ifndef PYLZSS
 extern uint8_t g_Window[WSIZE];

 void lzss_encode(int ifd, int ofd);
 void lzss_decode(int ifd, int ofd);
#endif

#endif
