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

#include "core/ma.h"
#include "encseq-def.h"
#include "iter-window.h"

struct Windowiterator
{
  GtUchar *buffer;
  unsigned long firstpos, bufsize, windowsize;
  Seqpos currentpos, endpos;
  const Encodedsequence *encseq;
  Encodedsequencescanstate *esr;
};

Windowiterator *windowiterator_new(const Encodedsequence *encseq,
                                   unsigned long windowsize,
                                   Seqpos startpos,
                                   Seqpos endpos)
{
  Windowiterator *wit;

  gt_assert(encseq != NULL);
  gt_assert(endpos <= getencseqtotallength(encseq));
  wit = gt_malloc(sizeof (*wit));
  wit->buffer = gt_malloc(sizeof (GtUchar) * windowsize);
  wit->firstpos = wit->bufsize = 0;
  wit->windowsize = windowsize;
  wit->currentpos = startpos;
  wit->endpos = endpos;
  wit->esr = newEncodedsequencescanstate();
  wit->encseq = encseq;
  initEncodedsequencescanstate(wit->esr,encseq,Forwardmode,startpos);
  return wit;
}

void windowiterator_delete(Windowiterator *wit)
{
  gt_free(wit->buffer);
  gt_free(wit);
}

const GtUchar *windowiterator_next(Seqpos *currentpos,unsigned long *firstpos,
                                 Windowiterator *wit)
{
  GtUchar currentchar;

  while (wit->currentpos < wit->endpos)
  {
    currentchar = sequentialgetencodedchar(wit->encseq,
                                           wit->esr,
                                           wit->currentpos,Forwardmode);
    if (ISSPECIAL(currentchar))
    {
      wit->bufsize = wit->firstpos = 0;
    } else
    {
      if (wit->bufsize < wit->windowsize)
      {
        wit->buffer[wit->bufsize++] = currentchar;
      } else
      {
        wit->buffer[wit->firstpos++] = currentchar;
        if (wit->firstpos == wit->windowsize)
        {
          wit->firstpos = 0;
        }
      }
    }
    if (wit->bufsize == wit->windowsize)
    {
      gt_assert(wit->currentpos >= (Seqpos) (wit->windowsize-1));
      gt_assert(wit->firstpos < wit->windowsize);
      *currentpos = wit->currentpos++;
      *firstpos = wit->firstpos;
      return wit->buffer;
    }
    wit->currentpos++;
  }
  return NULL;
}

#ifdef  WITHWINDOWCHECK
static void checkcurrentwindow(const Encodedsequence *encseq,
                               const GtUchar *buffer,
                               unsigned long windowsize,
                               unsigned long firstpos,
                               Seqpos currentpos)
{
  unsigned long idx, bufpos, bfbufpos;
  GtUchar cc1, cc2;

  bufpos = firstpos;
  for (idx= 0; idx<windowsize; idx++)
  {
    bfbufpos = (firstpos + idx) % windowsize;
    /*
    printf("bufpos=%lu,(firstpos=%lu + idx=%lu) %% windowsize=%lu)=%lu\n",
            bufpos,firstpos,idx,windowsize,bfbufpos);
    */
    gt_assert(bfbufpos == bufpos);
    cc1 = buffer[bfbufpos];
    cc2 = getencodedchar(encseq,currentpos-(windowsize-1)+idx,Forwardmode);
    gt_assert(cc1 == cc2);
    bufpos = (bufpos == windowsize-1) ? 0 : (bufpos + 1);
  }
}

static void iteroverallwords(const Encodedsequence *encseq,
                             unsigned long windowsize,
                             Seqpos startpos,
                             Seqpos endpos)
{
  unsigned long firstpos, bufsize;
  GtUchar currentchar;
  Seqpos currentpos;
  Encodedsequencescanstate *esr;
  GtUchar *buffer;
  unsigned long windowschecked = 0;

  gt_assert(endpos <= getencseqtotallength(encseq));
  esr = newEncodedsequencescanstate();
  initEncodedsequencescanstate(esr,encseq,Forwardmode,startpos);
  buffer = gt_malloc(sizeof(GtUchar) * windowsize);
  firstpos = bufsize = 0;
  for (currentpos=startpos; currentpos < endpos; currentpos++)
  {
    currentchar = sequentialgetencodedchar(encseq,esr,currentpos,Forwardmode);
    if (ISSPECIAL(currentchar))
    {
      bufsize = firstpos = 0;
    } else
    {
      if (bufsize < windowsize)
      {
        buffer[bufsize++] = currentchar;
      } else
      {
        buffer[firstpos++] = currentchar;
        if (firstpos == windowsize)
        {
          firstpos = 0;
        }
      }
    }
    if (bufsize == windowsize)
    {
      checkcurrentwindow(encseq,
                         buffer,
                         windowsize,
                         firstpos,
                         currentpos);
      windowschecked++;
    }
  }
  freeEncodedsequencescanstate(&esr);
  gt_free(buffer);
  printf("# %lu windows checked\n",windowschecked);
}

static void iteroverallwords2(const Encodedsequence *encseq,
                              unsigned long windowsize,
                              Seqpos startpos,
                              Seqpos endpos)
{
  Windowiterator *wit;
  const GtUchar *buffer;
  Seqpos currentpos;
  unsigned long firstpos, windowschecked = 0;

  wit = windowiterator_new(encseq,windowsize,startpos,endpos);
  while (true)
  {
    buffer = windowiterator_next(&currentpos,&firstpos,wit);
    if (buffer != NULL)
    {
      checkcurrentwindow(encseq,
                         buffer,
                         windowsize,
                         firstpos,
                         currentpos);
      windowschecked++;
    } else
    {
      break;
    }
  }
  windowiterator_delete(wit);
  printf("# %lu windows checked\n",windowschecked);
}
#endif
