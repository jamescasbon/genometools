/*
  Copyright (c) 2009 Stefan Kurtz <kurtz@zbh.uni-hamburg.de>
  Copyright (c) 2009 Center for Bioinformatics, University of Hamburg

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

#ifndef SFX_REMAINSORT_H
#define SFX_REMAINSORT_H

#include "core/error_api.h"
#include "seqpos-def.h"
#include "readmode-def.h"
#include "bcktab.h"
#include "intcode-def.h"
#include "defined-types.h"
#include "compressedtab.h"

typedef struct Rmnsufinfo Rmnsufinfo;

Rmnsufinfo *newRmnsufinfo(Seqpos *presortedsuffixes,
                          int mmapfiledesc,
                          const Encodedsequence *encseq,
                          Bcktab *bcktab,
                          Codetype maxcode,
                          unsigned int numofchars,
                          unsigned int prefixlength,
                          Readmode readmode,
                          Seqpos partwidth,
                          bool hashexceptions,
                          bool absoluteinversesuftab);

void rmnsufinfo_addunsortedrange(Rmnsufinfo *rmnsufinfo,
                                 Seqpos left,Seqpos right,Seqpos depth);

void bcktab2firstlevelintervals(Rmnsufinfo *rmnsufinfo );
Compressedtable *rmnsufinfo_wrap(Seqpos *longest,
                                 Rmnsufinfo **rmnsufinfoptr,bool withlcptab);

#endif
