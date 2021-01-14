/**
 * Original LZSS encoder-decoder  (c) Haruhiko Okumura
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

#include "lzss.h"

#define TRUE    (1==1)
#define FALSE   !TRUE
#define ENCODE  1
#define DECODE  -1

struct clopts {
    int mode;
    char *out;
    char *in;
};

static void usage(const char *name)
{
    const char *s =
        "Usage: %s [options] <infile>\n"
        "Options:\n"
        "  -h, --help      Print this help message.\n"
        "  -e, --encode    Encode <infile>.\n"
        "  -d, --decode    Decode <infile>.\n"
        "  -o, --out=FILE  Output result to FILE.\n"
    fprintf(stderr, s, name);
}

static int parse_args(int argc, char **argv, struct clopts *cli)
{
    int decode = FALSE;

    static struct option long_opts[] = {
        {"help",   no_argument,       0, 'h'},
        {"encode", required_argument, 0, 'e'},
        {"decode", required_argument, 0, 'd'},
        {"out",    required_argument, 0, 'o'},
        {0, 0, 0, 0}
    };

    if (!cli)
        return -1;
    memset(cli, 0, sizeof(struct clopts));

    while (1) {
        int c, iopt = 0;
        c = getopt_long(argc, argv, "he:d:o:", long_opts, &iopt);
        if (c == -1)
            break;

        switch (c) {
        case 'h':
            usage(argv[0]);
            return -1;

        case 'e':
            cli->mode = ENCODE;
            cli->in = optarg;
            break;

        case 'd':
            decode = TRUE;
            if(cli->mode == ENCODE)
                break;
            cli->mode = DECODE;
            cli->in = optarg;
            break;

        case 'o':
            cli->out = optarg;
            break;

        default:
            usage(argv[0]);
            return -1;
        }
    }

    if (cli->mode == ENCODE && decode == TRUE) {
        fprintf(stderr, "--encode or --decode?\n");
        usage(argv[0]);
        return -1;
    }

    if(cli->mode != ENCODE && cli->mode != DECODE) {
        fprintf(stderr, "--encode or --decode?\n");
        usage(argv[0]);
        return -1;
    }

    if (!cli->out) {
        fprintf(stderr, "Missing --out=FILE\n");
        usage(argv[0]);
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    int ifd, ofd;
    struct clopts cli;

    if (parse_args(argc, argv, &cli) < 0)
        exit(1);

    if ((ifd = open(cli.in, O_RDONLY, 0644)) == -1) {
        fprintf(stderr, "Can't open file: %s\n", cli.in);
        exit(1);
    }

    if ((ofd = open(cli.out, O_CREAT | O_TRUNC | O_WRONLY, 0644)) == -1) {
        fprintf(stderr, "Can't open file: %s\n", cli.out);
        exit(1);
    }

    if( cli.mode == ENCODE )
        lzss_encode(ifd, ofd);
    else
        lzss_decode(ifd, ofd);

    close(ifd);
    close(ofd);

    return 0;
}
