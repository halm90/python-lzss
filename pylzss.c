#include <Python.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define PYLZSS /* don't include function prototypes from lzss.h */
#include "lzss.h"


typedef struct
{
    uint8_t  * buffer;
    uint32_t   index;
    uint8_t    bitbuf;
    uint32_t   mask;
    int        buflen;
    int        fd;
} lzss_t;

typedef struct
{
    uint8_t * data;
    uint32_t  index;
    uint32_t  length;
} encode_data_t;

uint8_t  window[WSIZE];


/*
** General utility / support functions
*/
static PyObject *
_buildValue(uint8_t * output, int nbytes)
{
    char *str_encoding;
    str_encoding = (PY_MAJOR_VERSION >= 3) ? "y#" : "s#";
    PyObject *obj = Py_BuildValue(str_encoding, output, nbytes);
    return obj;
}



/*
** Decode support functions
*/
static int
readByteFromBuffer(const uint8_t * buffer, uint8_t * out, const uint32_t buflen, uint32_t * index)
{
int retn = 0;

  if( buffer && out && buflen && index && (*index < buflen) )
  {
    *out = buffer[(*index)++];
    retn = 1;
  }
  return(retn);
}


static int
getbits_r(int n, uint32_t *out, lzss_t *state)
{
int i;
int rc;
uint32_t x = 0;

  *out = 0;
  for( i = 0; i < n; i++ )
  {
    if( state->mask == 0 )
    {
      if( state->buffer != NULL )
      {
        if( !readByteFromBuffer(state->buffer, &state->bitbuf, state->buflen, &state->index) )
          return -1;
      }
      else
      {
        do
        {
          rc = (int)read(state->fd, &state->bitbuf, sizeof(uint8_t));
        } while( rc == -1 && errno == EINTR );
        if (rc <= 0)
        {
          return -1;
        }
      }
      state->mask = 128;
    }
    x <<= 1;
    if( state->bitbuf & state->mask )
    {
      x++;
    }
    state->mask >>= 1;
  }
  *out = x;
  return 0;
}


/*
** Decoder
*/
static int
_do_decode(lzss_t * bs, char **decoded)
{
int        dsize = 8192;
uint32_t   r = (N - F);
uint32_t   c;
int        bytes = 0;
char     * unpacked_str = malloc(dsize);

  memset(window, ' ', r);

  while( getbits_r(1, &c, bs) != -1 )
  {
    if( c )
    {
      if( getbits_r(8, &c, bs) == -1 )
      {
        break;
      }
      if( bytes >= dsize )
      {
        unpacked_str = realloc(unpacked_str, dsize*2);
        dsize *= 2;
      }
      unpacked_str[bytes++] = (uint8_t)c;
      window[r++] = (uint8_t)c;
      r &= (N-1);
    }
    else
    {
      uint32_t offset = 0;
      uint32_t length = 0;
      uint32_t k;
        if( getbits_r(OFFSET_BITS, &offset, bs) == -1 )
        {
          break;
        }
        if( getbits_r(LENGTH_BITS, &length, bs) == -1 )
        {
          break;
        }

        for( k = 0; k < length + (P+1); k++ )
        {
          c = window[(offset + k) & (N-1)];
          if( bytes >= dsize )
          {
            unpacked_str = realloc(unpacked_str, dsize*2);
            dsize *= 2;
          }
          unpacked_str[bytes++] = (uint8_t)c;
          window[r++] = (uint8_t)c;
          r &= (N-1);
        }
      }
    }
    *decoded = unpacked_str;
    return(bytes);
}


/*
** Encoder and encode support functions
*/
static void
putbit0(lzss_t * encObj)
{
  if( (encObj->mask >>= 1) == 0 )
  { /* no more bits left in `bitbuf` so write it to disk */
    encObj->buffer[encObj->index++] = encObj->bitbuf;
    encObj->bitbuf = 0;
    encObj->mask   = 128;
  }
}


static void
putbit1(lzss_t * encObj)
{
  encObj->bitbuf |= encObj->mask;
  putbit0(encObj);
}


static void
flush_bit_buffer(lzss_t * encObj)
{
  if( encObj->mask != 128 )
  {
    encObj->buffer[encObj->index++] = encObj->bitbuf;
  }
}


static void
write_bits(lzss_t * encObj, uint32_t c, uint32_t mask)
{
  while (mask >>= 1)
  {
    if( c & mask )
    {
      putbit1(encObj);
    }
    else
    {
      putbit0(encObj);
    }
  }
}


static void
write_literal(lzss_t * encObj, uint8_t c)
{
uint32_t mask = 256;
  putbit1(encObj);    /* indicates a literal byte follows */
  write_bits(encObj, c, mask);
}


static void
write_literals(lzss_t * encObj, uint32_t offset, uint32_t length)
{
uint32_t i;
  for( i = offset; i < offset+length; i++ )
  {
    write_literal(encObj, window[i]);
  }
}

static void
write_offset_length(lzss_t * encObj, uint32_t offset, uint32_t length)
{
uint32_t mask;
  /* indicates that an (offset, length) pair follows */
  putbit0(encObj);

  /* write offset */
  mask = (1 << OFFSET_BITS);
  write_bits(encObj, offset, mask);

  /* write length */
  mask = (1 << LENGTH_BITS);
  write_bits(encObj, length, mask);
}


int32_t
encode_read(lzss_t * encObj, uint8_t * dest, int readLen, encode_data_t * encData)
{
int32_t rtnCount = 0;

    if( encData )
    {
        rtnCount = MIN(readLen, (encData->length - encData->index));
        // Copy "rtnCount" bytes from source to destination.
        memcpy(dest, &encData->data[encData->index], rtnCount);
        encData->index += rtnCount;
    }
    else
    {
        rtnCount = read(encObj->fd, dest, readLen);
    }
    return rtnCount;
}

static int
_do_encode(lzss_t * encObj, uint8_t ** encoded, encode_data_t * encData)
{ // Utility function to do the actual encoding.
uint32_t  r  = N-F;
int32_t   s  = 0;
uint32_t  nb = WSIZE - r;
int32_t   bufferend;
int32_t   readlen;

  encObj->buflen = (WSIZE << 1);
  encObj->buffer = (uint8_t *)calloc(encObj->buflen, sizeof(uint8_t));
  memset(window, ' ', sizeof(window));

  readlen   = encode_read(encObj, &window[(N-F)], nb, encData);
  bufferend = (N-F) + readlen;

  while( r < bufferend )
  {
  int32_t  i;
  int32_t  remaining;
  uint8_t  c;
  uint32_t offset;
  uint32_t length;

    remaining = MIN(F, (bufferend-r));
    offset = 0;
    length = 1;
    c = window[r];      // current char in lookahead buffer

    /* go backwards in the search buffer looking for a match */
    for( i = r-1; i >= s; i--)
    {
      if( window[i] == c )
      {
      int32_t j;
        // Found a match in the search buffer, now search forward.
        for( j=1; j < remaining; j++ )
        {
          if( window[i+j] != window[r+j]) break;
        }
        // track the longest match
        if((uint32_t)j > length)
        {
          offset = (uint32_t)i;
          length = (uint32_t)j;
        }
      }
    }
    if( encObj->index >= encObj->buflen )
    {
      encObj->buflen += WSIZE;
      encObj->buffer = realloc(encObj->buffer, encObj->buflen);
    }
    if( length == 1)
    { // possibly wasn't a match, so output single literal 'c'
      write_literal(encObj, c);
    }
    else if(length > 1 && length <= P)
    {
      write_literals(encObj, offset, length);
    }
    else
    { // if length == P+1, length - (P+1) brings length to 0.
      // Decoder determines lengh of match in lookup table by
      // copying length + (P+1) characters to output.
      write_offset_length(encObj, offset & (N-1), length - (P+1));
    }
    r += (int32_t)length;
    s += (int32_t)length;
    if( r >= WSIZE - F )
    {
      memmove(window, &window[N], N);  // copy 2nd half of window to 1st half
      bufferend -= N;
      r -= N;
      s -= N;
      // overwrite 2nd half of window with new bytes
      readlen = encode_read(encObj, &window[bufferend], (uint32_t)(WSIZE - bufferend), encData);
      bufferend += readlen;
    }
  }
  flush_bit_buffer(encObj);
  *encoded = encObj->buffer;
  return(encObj->index);
}


/*
** Top level interface functions
*/
static PyObject *
_encode(PyObject *self, PyObject *args)
{ /*
  ** Takes the filename of the file to encode as an argument.
  */
char    * filename;
lzss_t    encodeObj;
uint8_t * encoded = NULL;
int       nbytes;

  if (!PyArg_ParseTuple(args, "s", &filename))
  {
    return NULL;
  }

  memset(&encodeObj, 0, sizeof(lzss_t));
  encodeObj.mask = 128;
  if( (encodeObj.fd = open(filename, O_RDONLY, 0644)) == -1 )
  {
    PyErr_SetFromErrno(PyExc_OSError);
    return NULL;
  }

  nbytes = _do_encode(&encodeObj, &encoded, NULL);

  if( encodeObj.fd ) close(encodeObj.fd);
  PyObject *obj = _buildValue(encoded, nbytes);
  if( encoded ) free(encoded);
  return(obj);
}


static PyObject *
_encode_bytes(PyObject *self, PyObject *args)
{ /* Encode from 'bytes' rather than file.
  */
PyObject      * obj = NULL;
lzss_t          encObj;
int             bytes;
uint8_t       * encoded;
encode_data_t   inputData;

  memset(&inputData, 0, sizeof inputData);
  memset(&encObj, 0, sizeof encObj);
  encObj.mask = 128;

  if (!PyArg_ParseTuple(args, "s#", &inputData.data, &inputData.length))
  {
    return NULL;
  }

  if(!inputData.data || !inputData.length)
  { /* Set some sort of error indicating a failure in providing input */
    PyErr_SetFromErrno(PyExc_OSError);
    return NULL;
  }

  bytes = _do_encode(&encObj, &encoded, &inputData);
  obj   = _buildValue(encoded, bytes);
  if( encoded ) free(encoded);
  return obj;
}


static PyObject *
_decode(PyObject *self, PyObject *args)
{ /*
  ** Takes the filename of the encoded file as an argument.
  */
char *filename;
lzss_t bs;
int bytes;
char *decoded;

  memset(&bs, 0, sizeof bs);

  if (!PyArg_ParseTuple(args, "s", &filename))
  {
    return NULL;
  }

  if( (bs.fd = open(filename, O_RDONLY, 0644)) == -1 )
  {
    PyErr_SetFromErrno(PyExc_OSError);
    return NULL;
  }

  bytes = _do_decode(&bs, &decoded);

  if( bs.fd ) close(bs.fd);
  PyObject * obj = _buildValue((uint8_t *)decoded, bytes);
  if( decoded ) free(decoded);
  return(obj);
}


static PyObject *
_decode_bytes(PyObject *self, PyObject *args)
{ /* Decode from 'bytes' rather than file.
  ** Implementation incomplete.
  */
lzss_t bs;
int bytes;
char *decoded;

  memset(&bs, 0, sizeof bs);

  if (!PyArg_ParseTuple(args, "s#", &bs.buffer, &bs.buflen))
  {
    return NULL;
  }

  if(!bs.buffer || !bs.buflen)
  { /* Set some sort of error indicating a failure in providing input */
    PyErr_SetFromErrno(PyExc_OSError);
    return NULL;
  }

  bytes = _do_decode(&bs, &decoded);
  PyObject *obj = _buildValue((uint8_t *)decoded, bytes);
  if( decoded ) free(decoded);
  return(obj);
}

static PyMethodDef LZSSMethods[] = {
    {"decode", _decode, METH_VARARGS},
    {"decode_bytes", _decode_bytes, METH_VARARGS},
    {"encode", _encode, METH_VARARGS},
    {"encode_bytes", _encode_bytes, METH_VARARGS},
    {NULL, NULL, 0, NULL}
};


#if PY_MAJOR_VERSION >= 3

static struct PyModuleDef lzssmodule =
{
   PyModuleDef_HEAD_INIT,
   "lzss",
   NULL,
   -1,
   LZSSMethods
};

PyMODINIT_FUNC
PyInit_lzss(void)
{
  return(PyModule_Create(&lzssmodule));
}

#elif PY_MAJOR_VERSION == 2

PyMODINIT_FUNC initlzss(void)
{
  Py_InitModule("lzss", LZSSMethods);
}

#endif
