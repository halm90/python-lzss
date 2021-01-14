#!/usr/bin/env python

"""
Proof-of-concept code for use in ZVT's file_processor.py.

Before get_ring_buffer_files_by_extension(...) is called, the compressed ZLC
files should be decompressed and saved to disk as ZDL files. The corresponding
*.ZLC.cooked files should be renamed to *.ZDL.cooked.
"""

import os
import sys
from glob import glob

import lzss

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print "Usage: preprocess.py <directory>"
        sys.exit()

    d = sys.argv[1]
    files = glob("%s/*.ZLC" % d)
    for zlc in files:
        zdl = zlc.split('/')[-1].split('.')[0]
        decoded = lzss.decode(zlc)
        f = open("decompressed/%s.ZDL" % zdl, "wb")
        f.write(decoded)
        f.close()
        os.unlink(zlc)

    files = glob("%s/*.ZLC.cooked" % d)
    for c in files:
        zdl = c.split('/')[-1].split('.')[0]
        os.rename(c, "decompressed/%s.ZDL.cooked" % zdl)
