/*
  Copyright (c) 2007      Stefan Kurtz <kurtz@zbh.uni-hamburg.de>
  Copyright (c)      2009 Sascha Steinbiss <steinbiss@zbh.uni-hamburg.de>
  Copyright (c) 2007-2009 Center for Bioinformatics, University of Hamburg

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

#ifndef SEQUENCE_BUFFER_INLINE_H
#define SEQUENCE_BUFFER_INLINE_H

#include "core/file.h"
#include "core/sequence_buffer_rep.h"

static inline int process_char(GtSequenceBuffer *sb,
                               unsigned long currentoutpos, unsigned char cc,
                               GtError *err)
{
  GtSequenceBufferMembers *pvt;
  unsigned char charcode;
  pvt = sb->pvt;
  if (pvt->symbolmap) {
    charcode = pvt->symbolmap[(unsigned int) cc];
    if (charcode == UNDEFCHAR) {
      gt_error_set(err, "illegal character '%c': file \"%s\", line %llu",
                        cc, gt_str_array_get(pvt->filenametab, pvt->filenum),
                        (unsigned long long) pvt->linenum);
      return -1;
    }
    if (ISSPECIAL(charcode)) {
      pvt->lastspeciallength++;
    } else {
      if (pvt->lastspeciallength > 0)
        pvt->lastspeciallength = 0;
      if (pvt->chardisttab)
        pvt->chardisttab[(int) charcode]++;
    }
    pvt->outbuf[currentoutpos] = charcode;
  } else
    pvt->outbuf[currentoutpos] = cc;
  pvt->counter++;
  return 0;
}

static inline int inlinebuf_getchar(GtSequenceBuffer *sb, GtFile *f)
{
  GtSequenceBufferMembers *pvt;
  pvt = sb->pvt;
  if (pvt->use_ungetchar) {
    pvt->use_ungetchar = false;
    return pvt->ungetchar;
  } else {
    if (pvt->currentinpos >= pvt->currentfillpos) {
      pvt->currentfillpos = gt_file_xread(f, pvt->inbuf, INBUFSIZE);
      if (pvt->currentfillpos == 0)
         return EOF;
      pvt->currentinpos = 0;
    }
    pvt->ungetchar = pvt->inbuf[pvt->currentinpos++];
    return pvt->ungetchar;
  }
}

static inline void inlinebuf_ungetchar(GtSequenceBuffer *sb)
{
  gt_assert(!sb->pvt->use_ungetchar);
  sb->pvt->use_ungetchar = true;
}

#endif
