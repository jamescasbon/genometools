/*
  Copyright (C) 2007 Thomas Jahns <Thomas.Jahns@gmx.net>

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

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libgtcore/dataalign.h"
#include "libgtcore/fa.h"
#include "libgtcore/filelengthvalues.h"
#include "libgtcore/minmax.h"
#include "libgtcore/seqiterator.h"
#include "libgtcore/str.h"
#include "libgtcore/strarray.h"
#include "libgtcore/symboldef.h"
#include "libgtcore/unused.h"
#include "libgtmatch/seqpos-def.h"
#include "libgtmatch/verbose-def.h"
#include "libgtmatch/esafileend.h"
#include "libgtmatch/intcode-def.h"
#include "libgtmatch/encseq-def.h"

#include "libgtmatch/eis-encidxseq.h"
#include "libgtmatch/eis-sequencemultiread.h"
#include "libgtmatch/eis-suffixerator-interface.h"

struct sfxInterface
{
  Readmode readmode;
  unsigned int prefixlength, numofparts;
  const Sfxstrategy *sfxstrategy;
  Measuretime *mtime;
  Seqpos length;
  const Alphabet *alpha;
  const Encodedsequence *encseq;
  struct seqStats *stats;
  Sfxiterator *sfi;
  DefinedSeqpos longest;
  bool specialsuffixes;
  /* data relevant to holding portions of the suffix array */
  Seqpos lastGeneratedLen, lastGeneratedStart;
  const Seqpos *lastGeneratedSufTabSegment;
  /* data relevant to listeners */
  struct seqReaderSet readerSet;
  struct saTaggedXltorStateList xltorStates;
};

#if 0
static Seqpos
sfxIReadAdvance(sfxInterface *sfxi,
                Seqpos requestMaxPos);
#endif

static SeqDataTranslator
sfxIRequest2XltorFunc(UNUSED sfxInterface *sfxi,
                      UNUSED enum sfxDataRequest request)
{
  SeqDataTranslator tr = { { NULL }, NULL };
  switch (request)
  {
    union saXltorState readState;
    struct saTaggedXltorState *stateStore;
  case SFX_REQUEST_BWTTAB:
    readState.encSeqTr.readmode = sfxi->readmode;
    readState.encSeqTr.encseq = sfxi->encseq;
    stateStore = addSuffixarrayXltor(&sfxi->xltorStates,
                                     request, readState);
    tr.state.ref = &stateStore->state.encSeqTr;
    tr.translateData = (seqDataTranslateFunc)translateSuftab2BWT;
    break;
  case SFX_REQUEST_SUFTAB:
    tr.state.elemSize = sizeof (Seqpos);
    break;
  case SFX_REQUEST_LCPTAB:
    readState.lcpState.readmode = sfxi->readmode;
    readState.lcpState.encseq = sfxi->encseq;
    readState.lcpState.lastSufIdx = -1;
    stateStore = addSuffixarrayXltor(&sfxi->xltorStates,
                                     request, readState);
    tr.state.ref = &stateStore->state.lcpState;
    tr.translateData = (seqDataTranslateFunc)translateSuftab2LCP;
    break;
  default:
    fprintf(stderr, "error: unimplemented request!\n");
    abort();
    break;
  }
  return tr;
}

static size_t
sfxIGenerate(void *iface, void *backlogState,
             move2BacklogFunc move2Backlog, void *output, Seqpos generateStart,
             size_t len, SeqDataTranslator xltor, Error *err);

extern sfxInterface *
newSfxInterface(Readmode readmode,
                unsigned int prefixlength,
                unsigned int numofparts,
                const Sfxstrategy *sfxstrategy,
                const Encodedsequence *encseq,
                const Specialcharinfo *specialcharinfo,
                unsigned long numofsequences,
                Measuretime *mtime,
                Seqpos length,
                const Alphabet *alpha,
                const unsigned long *characterdistribution,
                Verboseinfo *verbosity,
                Error *err)
{
  return newSfxInterfaceWithReaders(readmode, prefixlength,
                                    numofparts,
                                    sfxstrategy, 0, NULL, NULL, encseq,
                                    specialcharinfo, numofsequences, mtime,
                                    length, alpha, characterdistribution,
                                    verbosity, err);
}

static struct seqStats *
newSeqStatsFromCharDist(const Alphabet *alpha, Seqpos len, unsigned numOfSeqs,
                        const unsigned long *characterdistribution)
{
  struct seqStats *stats = NULL;
  unsigned i, mapSize;
  Seqpos regularSymsSum = 0;
  stats = ma_malloc(offsetAlign(sizeof (*stats), sizeof (Seqpos))
                    + (UINT8_MAX + 1) * sizeof (Seqpos));
  stats->sourceAlphaType = sourceUInt8;
  stats->symbolDistributionTable =
    (Seqpos *)((char *)stats + offsetAlign(sizeof (*stats), sizeof (Seqpos)));
  memset(stats->symbolDistributionTable, 0, sizeof (Seqpos) * (UINT8_MAX + 1));
  mapSize = getmapsizeAlphabet(alpha);
  for (i = 0; i < mapSize - 1; ++i)
    regularSymsSum +=
      (stats->symbolDistributionTable[i] = characterdistribution[i]);
  stats->symbolDistributionTable[WILDCARD] = len - regularSymsSum - numOfSeqs;
  stats->symbolDistributionTable[SEPARATOR] += numOfSeqs;
  stats->symbolDistributionTable[UNDEFBWTCHAR] += 1;
  return stats;
}

static void
deleteSeqStats(struct seqStats *stats)
{
  ma_free(stats);
}

#define newSfxInterfaceWithReadersErrRet()        \
  do {                                            \
    if (sfxi->stats)                             \
      deleteSeqStats(sfxi->stats);               \
    if (sfxi) ma_free(sfxi);                    \
    sfxi = NULL;                                 \
  } while (0)

extern sfxInterface *
newSfxInterfaceWithReaders(Readmode readmode,
                           unsigned int prefixlength,
                           unsigned int numofparts,
                           const Sfxstrategy *sfxstrategy,
                           size_t numReaders,
                           enum sfxDataRequest readerRequests[],
                           SeqDataReader readers[],
                           const Encodedsequence *encseq,
                           const Specialcharinfo *specialcharinfo,
                           unsigned long numofsequences,
                           Measuretime *mtime,
                           Seqpos length,
                           const Alphabet *alpha,
                           const unsigned long *characterdistribution,
                           Verboseinfo *verbosity, Error *err)
{
  sfxInterface *sfxi = NULL;
  error_check(err);

  sfxi = ma_calloc(1, sizeof (*sfxi));
  sfxi->readmode = readmode;
  sfxi->mtime = mtime;
  sfxi->length = length;
  sfxi->alpha = alpha;
  sfxi->encseq = encseq;
  sfxi->stats = newSeqStatsFromCharDist(sfxi->alpha, sfxi->length,
                                         numofsequences,
                                         characterdistribution);
  if (!(sfxi->sfi = newSfxiterator(specialcharinfo->specialcharacters,
                                    specialcharinfo->realspecialranges,
                                    encseq,
                                    readmode,
                                    getnumofcharsAlphabet(alpha),
                                    getcharactersAlphabet(alpha),
                                    prefixlength,
                                    numofparts,
                                    NULL,
                                    sfxstrategy,
                                    sfxi->mtime,
                                    verbosity, err)))
    newSfxInterfaceWithReadersErrRet();
  sfxi->longest.defined = false;
  sfxi->specialsuffixes = false;

  sfxi->lastGeneratedStart = sfxi->lastGeneratedLen = 0;
  sfxi->lastGeneratedSufTabSegment = NULL;

  initEmptySeqReaderSet(&sfxi->readerSet, SFX_REQUEST_NONE, sizeof (Seqpos),
                        sfxIGenerate, sfxi);

  initSATaggedXltorStateList(&sfxi->xltorStates);

  {
    size_t i;
    for (i = 0; i < numReaders; ++i)
    {
      readers[i] = SfxIRegisterReader(sfxi, readerRequests[i]);
      if (!readers[i].readData)
        newSfxInterfaceWithReadersErrRet();
    }
  }
  return sfxi;
}

extern const Sfxiterator *SfxInterface2Sfxiterator(const sfxInterface *sfxi)
{
  return sfxi->sfi;
}

extern void
deleteSfxInterface(sfxInterface *sfxi)
{
  destructSATaggedXltorStateList(&sfxi->xltorStates);
  destructSeqReaderSet(&sfxi->readerSet);
  freeSfxiterator(&sfxi->sfi);
  deleteSeqStats(sfxi->stats);
  ma_free(sfxi);
}

extern const Alphabet *
SfxIGetAlphabet(const sfxInterface *si)
{
  return si->alpha;
}

extern MRAEnc *
newMRAEncFromSfxI(const sfxInterface *si)
{
  MRAEnc *alphabet;
  assert(si);
  alphabet = MRAEncGTAlphaNew(SfxIGetAlphabet(si));
  MRAEncAddSymbolToRange(alphabet, SEPARATOR, 1);
  return alphabet;
}

Seqpos
getSfxILength(const sfxInterface *si)
{
  assert(si);
  return si->length;
}

extern const struct seqStats *
getSfxISeqStats(const sfxInterface *si)
{
  return si->stats;
}

extern DefinedSeqpos
getSfxILongestPos(const struct sfxInterface *si)
{
  return si->longest;
}

extern const Encodedsequence *
SfxIGetEncSeq(const sfxInterface *si)
{
  return si->encseq;
}

extern SeqDataReader
SfxIRegisterReader(sfxInterface *sfxi, enum sfxDataRequest request)
{
  return seqReaderSetRegisterConsumer(
    &sfxi->readerSet, request, sfxIRequest2XltorFunc(sfxi, request));
}

extern size_t
SfxIGetOrigSeq(void *state, Symbol *dest, Seqpos pos, size_t len)
{
  struct sfxInterface *sfxi;
  size_t i;
  assert(state);
  sfxi = state;
  for (i = 0; i < len; ++i)
    dest[i] = getencodedchar(sfxi->encseq, pos + i, sfxi->readmode);
  return len;
}

/** writes substring of suffix table to output, puts older data into
 * cache if necessary */
static size_t
sfxIGenerate(void *iface, void *backlogState,
             move2BacklogFunc move2Backlog, void *output, Seqpos generateStart,
             size_t len, SeqDataTranslator xltor, UNUSED Error *err)
{
  sfxInterface *sfxi = iface;
  size_t elemsLeft = len;
  assert(sfxi && backlogState && move2Backlog && output);
  assert(generateStart + len <= sfxi->length);
  do
  {
    if (generateStart < sfxi->lastGeneratedStart + sfxi->lastGeneratedLen)
    {
      size_t copyLen = MIN(elemsLeft, sfxi->lastGeneratedStart
                           + sfxi->lastGeneratedLen - generateStart),
        charsWritten =
        SDRTranslate(xltor, output, sfxi->lastGeneratedSufTabSegment
                     + generateStart - sfxi->lastGeneratedStart, copyLen);
      generateStart += copyLen;
      elemsLeft -= copyLen;
      output = (char *)output + charsWritten;
    }
    /* 1. read next region of sequence by calling nextSfxIterator */
    if (elemsLeft)
    {
      move2Backlog(backlogState, sfxi->lastGeneratedSufTabSegment,
                   sfxi->lastGeneratedStart, sfxi->lastGeneratedLen);
      sfxi->lastGeneratedStart += sfxi->lastGeneratedLen;
      if ((sfxi->lastGeneratedSufTabSegment =
           nextSfxiterator(&sfxi->lastGeneratedLen, &sfxi->specialsuffixes,
                           sfxi->mtime, sfxi->sfi)))
      {
        size_t pos;
        /* size_t because the current approach cannot generate more
         * than memory will hold anyway */
        size_t lastGeneratedLen = sfxi->lastGeneratedLen;
        const Seqpos *suftab = sfxi->lastGeneratedSufTabSegment;
        if (!sfxi->longest.defined)
          for (pos=0; pos < lastGeneratedLen; pos++)
          {
            if (suftab[pos] == 0)
            {
              sfxi->longest.defined = true;
              sfxi->longest.valueseqpos = sfxi->lastGeneratedStart + pos;
              break;
            }
          }
        /* uncomment this to reenable synchronous writing of tables */
/*if (sfxi->lastGeneratedSufTabSegment == NULL */
/*    || suftab2file(&sfxi->outfileinfo, sfxi->lastGeneratedSufTabSegment, */
/*                   sfxi->so.readmode, sfxi->lastGeneratedLen, err) != 0) */
/*       break; */
      }
      else
        break;
    }
    /* 5. if positions in region don't suffice go back to step 3. */
  } while (elemsLeft);
  return len - elemsLeft;
}