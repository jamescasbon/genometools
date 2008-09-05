/*
  Copyright (c) 2008 Stefan Kurtz <kurtz@zbh.uni-hamburg.de>
  Copyright (c) 2008 Center for Bioinformatics, University of Hamburg

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

#ifndef DIST_SHORT_H
#define DIST_SHORT_H
#include "libgtcore/symboldef.h"
#include "defined-types.h"

unsigned long distanceofshortstringsbytearray(unsigned long *eqsvector,
                                     unsigned int alphasize,
                                     const Uchar *useq,
                                     unsigned long ulen,
                                     const Uchar *vseq,
                                     unsigned long vlen);

unsigned long distanceofshortstringsencseq(unsigned long *eqsvector,
                                           unsigned int alphasize,
                                           const Uchar *useq,
                                           unsigned long ulen,
                                           const Encodedsequence *encseq,
                                           Seqpos vstartpos,
                                           Seqpos vlen);

unsigned long reversesuffixmatch(unsigned long *eqsvector,
                                 unsigned int alphasize,
                                 const Uchar *useq,
                                 unsigned long ulen,
                                 const Uchar *vseq,
                                 unsigned long vlen,
                                 unsigned long maxdistance);

Definedunsignedlong forwardprefixmatch(const Encodedsequence *encseq,
                                       unsigned int alphasize,
                                       Seqpos startpos,
                                       bool nowildcards,
                                       unsigned long *eqsvector,
                                       const Uchar *useq,
                                       unsigned long ulen,
                                       unsigned long maxdistance);

#endif