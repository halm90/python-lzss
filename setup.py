from distutils.core import setup, Extension
import os


def read(fname):
    return open(os.path.join(os.path.dirname(__file__), fname)).read()

ext = Extension('lzss', sources=['pylzss.c'])
setup(
    name='lzss',
    version=read('VERSION').strip(),
    description='lzss python bindings',
    ext_modules=[ext]
    )
