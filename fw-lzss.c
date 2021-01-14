#include "lzss.h"
#include <malloc.h>
// MAIN
////////////////////////////////////////////////////////////////////////
int lzss_encode(fdtype ifd, fdtype ofd)
{
    lzss_ctx lzctx;
    u8 c, *buf = malloc(WSIZE);
    if (!buf)
        return -1;

    lzss_encopen(&lzctx, ofd, buf);
    while (0 < read(ifd, &c, 1))
        lzss_putc(&lzctx, c);
    lzss_putc(&lzctx, -1);
    free(buf);
    return 0;
}

int lzss_decode(fdtype ifd, fdtype ofd)
{
    lzss_ctx lzctx;
    u8 *buf = malloc(N);
    if (!buf)
        return -1;
#if 1
    lzss_block_decode(&lzctx, ifd, ofd, buf);
#else
    {
        int val;
        u8 b;

        lzss_decopen(&lzctx, ifd, buf);
        while ((val = lzss_getc(&lzctx)) >= 0) {
            b = val;
            // unbuffered writes are slow, but the code is provided to show usage of lzss_getc()
            write(ofd, &b, 1);
        }
    }
#endif
    free(buf);
    return 0;
}

#include <stdio.h>
#include <fcntl.h>
/* Original LZSS encoder-decoder  (c) Haruhiko Okumura
*/
int main(int argc, char *argv[])
{
    int ret = -1;
    int ifd = 0, ofd = 1; // default to stdin/out for filter use
    if (argc != 4 && argc != 2)
        printf("Usage: %s (enc|dec) <sourcefile> <destinationfile>\n", argv[0]);
    else {
        if (!strcmp(argv[1], "enc")) {
            if (argc > 2)
                ifd = open(argv[2], O_BINARY | O_RDONLY);
            if (ifd >= 0) {
                ofd = open(argv[3], O_BINARY | O_WRONLY | O_CREAT, 0644);
                if (argc > 2)
                    if (ofd >= 0) {
                        lzss_encode(ifd, ofd);
                        close(ofd);
                        ret = 0;
                    }
                    else
                        printf("Can't open output file\n");
                close(ifd);
            }
            else
                printf("Can't open input file\n");
        }
        else if (!strcmp(argv[1], "dec")) {
            if (argc > 2)
                ifd = open(argv[2], O_BINARY | O_RDONLY);
            if (ifd >= 0) {
                if (argc > 2)
                    ofd = open(argv[3], O_BINARY | O_WRONLY | O_CREAT, 0644);
                if (ofd >= 0) {
                    lzss_decode(ifd, ofd);
                    close(ofd);
                    ret = 0;
                }
                else
                    printf("Can't open output file\n");
                close(ifd);
            }
            else
                printf("Can't open input file\n");
        }
    }
    return ret;
}
