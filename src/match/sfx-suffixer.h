/*
  Copyright (c) 2007 Stefan Kurtz <kurtz@zbh.uni-hamburg.de>
  Copyright (c) 2007 Center for Bioinformatics, University of Hamburg

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

#ifndef SFX_SUFFIXER_H
#define SFX_SUFFIXER_H
#include "core/error.h"
#include "readmode-def.h"
#include "measure-time-if.h"
#include "sfx-strategy.h"
#include "sfx-bentsedg.h"
#include "verbose-def.h"
#include "seqpos-def.h"

typedef struct Sfxiterator Sfxiterator;

void freeSfxiterator(Sfxiterator **sfiptr);

Sfxiterator *newSfxiterator(const Encodedsequence *encseq,
                            Readmode readmode,
                            unsigned int prefixlength,
                            unsigned int numofparts,
                            Outlcpinfo *outlcpinfo,
                            const Sfxstrategy *sfxstrategy,
                            Measuretime *mtime,
                            Verboseinfo *verboseinfo,
                            GtError *err);

const Seqpos *nextSfxiterator(Seqpos *numberofsuffixes,
                              bool *specialsuffixes,
                              Sfxiterator *sfi);

int postsortsuffixesfromstream(Sfxiterator *sfi, const GtStr *str_indexname,
                               GtError *err);

bool sfi2longestsuffixpos(Seqpos *longest,const Sfxiterator *sfi);

int sfibcktab2file(FILE *fp,const Sfxiterator *sfi,GtError *err);

unsigned int getprefixlenbits(void);

#endif
