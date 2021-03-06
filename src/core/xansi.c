/*
  Copyright (c) 2005-2009 Gordon Gremme <gremme@zbh.uni-hamburg.de>
  Copyright (c) 2005-2008 Center for Bioinformatics, University of Hamburg

  Permission to use, copy, modify, and distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "core/xansi.h"

void gt_xatexit(void (*function)(void))
{
  if (atexit(function)) {
    perror("cannot register exit function");
    exit(EXIT_FAILURE);
  }
}

void gt_xfclose(FILE *stream)
{
  if (fclose(stream)) {
    perror("cannot close stream");
    exit(EXIT_FAILURE);
  }
}

void gt_xfflush(FILE *stream)
{
  if (fflush(stream)) {
    perror("cannot fflush stream");
    exit(EXIT_FAILURE);
  }
}

int gt_xfgetc(FILE *stream)
{
  int cc;
  if ((cc = fgetc(stream)) == EOF) {
    if (ferror(stream)) {
      perror("cannot read char");
      exit(EXIT_FAILURE);
    }
  }
  return cc;
}

void gt_xfgetpos(FILE *stream, fpos_t *pos)
{
  if (fgetpos(stream, pos)) {
    perror("cannot get position of file");
    exit(EXIT_FAILURE);
  }
}

FILE* gt_xfopen(const char *path, const char *mode)
{
  FILE *file;
  if ((file = fopen(path, mode)) == NULL) {
    fprintf(stderr, "fopen(): cannot open file '%s': %s\n", path,
            strerror(errno));
    exit(EXIT_FAILURE);
  }
  return file;
}

void gt_xfputc(int c, FILE *stream)
{
  if (fputc(c, stream) == EOF) {
    perror("cannot fputc to stream");
    exit(EXIT_FAILURE);
  }
}

void gt_xfputs(const char *str, FILE *stream)
{
  gt_assert(str);
  if (fputs(str, stream) == EOF) {
    perror("cannot fputs to stream");
    exit(EXIT_FAILURE);
  }
}

size_t gt_xfread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t rval;
  if ((rval = fread(ptr, size, nmemb, stream)) < nmemb) {
    if (ferror(stream)) {
      perror("cannot read from stream");
      exit(EXIT_FAILURE);
    }
  }
  return rval;
}

void  gt_xfseek(FILE *stream, long offset, int whence)
{
  if (fseek(stream, offset, whence)) {
    perror("cannot seek of file");
    exit(EXIT_FAILURE);
  }
}

void gt_xfsetpos(FILE *stream, const fpos_t *pos)
{
  if (fsetpos(stream, pos)) {
    perror("cannot set position of file");
    exit(EXIT_FAILURE);
  }
}

void gt_xfwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  if (fwrite(ptr, size, nmemb, stream) != nmemb) {
    perror("cannot write to stream");
    exit(EXIT_FAILURE);
  }
}

void gt_xputchar(int c)
{
  if (putchar(c) == EOF) {
    perror("cannot putchar");
    exit(EXIT_FAILURE);
  }
}

void gt_xputs(const char *str)
{
  if (puts(str) == EOF) {
    perror("cannot puts");
    exit(EXIT_FAILURE);
  }
}

void gt_xremove(const char *path)
{
  if (remove(path)) {
    fprintf(stderr, "cannot remove file '%s': %s\n", path, strerror(errno));
    exit(EXIT_FAILURE);
  }
}

char* gt_xstrdup(const char *s)
{
  char *d;
  if ((d = strdup(s)) == NULL) {
    perror("cannot strdup");
    exit(EXIT_FAILURE);
  }
  return d;
}

void gt_xungetc(int c, FILE *stream)
{
  if (ungetc(c, stream) == EOF) {
    fprintf(stderr, "cannot ungetc character '%c'\n", c);
    exit(EXIT_FAILURE);
  }
}
