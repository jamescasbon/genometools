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

#ifdef INLINEDSequentialsuffixarrayreader

#include "core/error.h"
#include "core/str.h"
#include "verbose-def.h"
#include "seqpos-def.h"

int testmaxpairs(GT_UNUSED const GtStr *indexname,
                 GT_UNUSED unsigned long samples,
                 GT_UNUSED unsigned int minlength,
                 GT_UNUSED Seqpos substringlength,
                 GT_UNUSED Verboseinfo *verboseinfo,
                 GT_UNUSED GtError *err)
{
  return 0;
}

#else

#include "core/alphabet.h"
#include "core/array.h"
#include "core/arraydef.h"
#include "core/divmodmul.h"
#include "core/unused_api.h"
#include "spacedef.h"
#include "esa-mmsearch-def.h"
#include "format64.h"
#include "echoseq.h"
#include "encseq-def.h"

#include "esa-selfmatch.pr"
#include "arrcmp.pr"

static unsigned long getrecordnumulong(const unsigned long *recordseps,
                                       unsigned long numofrecords,
                                       unsigned long totalwidth,
                                       unsigned long position,
                                       GtError *err)
{
  unsigned long left, mid, right, len;

  gt_assert(numofrecords > 0);
  if (numofrecords == 1UL || position < recordseps[0])
  {
    return 0;
  }
  if (position > recordseps[numofrecords-2])
  {
    if (position < totalwidth)
    {
      return numofrecords - 1;
    }
    gt_error_set(err,"getrecordnumulong: cannot find position %lu",position);
    return numofrecords; /* failure */
  }
  left = 0;
  right = numofrecords - 2;
  while (left<=right)
  {
    len = (unsigned long) (right-left);
    mid = left + GT_DIV2(len);
#ifdef SKDEBUG
    printf("left=%lu,right = %lu\n",left,right);
    printf("mid=%lu\n",mid);
#endif
    if (recordseps[mid] < position)
    {
      if (position < recordseps[mid+1])
      {
        return mid + 1;
      }
      left = mid + 1;
    } else
    {
      if (recordseps[mid-1] < position)
      {
        return mid;
      }
      right = mid-1;
    }
  }
  gt_error_set(err,"getrecordnumulong: cannot find position %lu",position);
  return numofrecords; /* failure */
}

static Seqpos samplesubstring(GtUchar *seqspace,
                              const Encodedsequence *encseq,
                              Seqpos substringlength)
{
  Seqpos start, totallength;

  totallength = getencseqtotallength(encseq);
  start = (Seqpos) (drand48() * (double) totallength);
  if (start + substringlength > totallength)
  {
    substringlength = totallength - start;
  }
  gt_assert(substringlength > 0);
  encseqextract(seqspace,encseq,start,start+substringlength-1);
  return substringlength;
}

typedef struct
{
  unsigned long len, querystart;
  uint64_t queryseqnum;
  Seqpos dbstart;
} Substringmatch;

static int storemaxmatchquery(void *info,
                              unsigned long len,
                              Seqpos dbstart,
                              uint64_t queryseqnum,
                              unsigned long querystart,
                              GT_UNUSED GtError *err)
{
  GtArray *tab = (GtArray *) info;
  Substringmatch subm;

  subm.len = len;
  subm.dbstart = dbstart;
  subm.queryseqnum = queryseqnum;
  subm.querystart = querystart;
  gt_array_add(tab,subm);
  return 0;
}

typedef struct
{
  GtArray *results;
  Seqpos dblen;
  unsigned long *markpos,
                numofquerysequences,
                querylen;
} Maxmatchselfinfo;

static int storemaxmatchself(void *info,
                             Seqpos len,
                             Seqpos pos1,
                             Seqpos pos2,
                             GtError *err)
{
  Maxmatchselfinfo *maxmatchselfinfo = (Maxmatchselfinfo *) info;
  Seqpos dbstart, querystart;

  if (pos1 < pos2)
  {
    dbstart = pos1;
    querystart = pos2;
  } else
  {
    dbstart = pos2;
    querystart = pos1;
  }
  if (dbstart < maxmatchselfinfo->dblen &&
      maxmatchselfinfo->dblen < querystart)
  {
    Substringmatch subm;
    unsigned long pos;

    subm.len = (unsigned long) len;
    subm.dbstart = dbstart;
    pos = (unsigned long) (querystart - (maxmatchselfinfo->dblen + 1));
    if (maxmatchselfinfo->markpos == 0)
    {
      subm.queryseqnum = 0;
      subm.querystart = pos;
    } else
    {
      unsigned long queryseqnum
        = getrecordnumulong(maxmatchselfinfo->markpos,
                            maxmatchselfinfo->numofquerysequences,
                            maxmatchselfinfo->querylen,
                            pos,
                            err);
      if (queryseqnum == maxmatchselfinfo->numofquerysequences)
      {
        return -1;
      }
      if (queryseqnum == 0)
      {
        subm.querystart = pos;
      } else
      {
        subm.querystart = pos - (maxmatchselfinfo->markpos[queryseqnum-1] + 1);
      }
      subm.queryseqnum = (uint64_t) queryseqnum;
    }
    gt_array_add(maxmatchselfinfo->results,subm);
  }
  return 0;
}

static int orderSubstringmatch(const void *a,const void *b)
{
  Substringmatch *m1 = (Substringmatch *) a,
                 *m2 = (Substringmatch *) b;

  if (m1->queryseqnum < m2->queryseqnum)
  {
    return -1;
  }
  if (m1->queryseqnum > m2->queryseqnum)
  {
    return 1;
  }
  if (m1->querystart < m2->querystart)
  {
    return -1;
  }
  if (m1->querystart > m2->querystart)
  {
    return 1;
  }
  if (m1->dbstart < m2->dbstart)
  {
    return -1;
  }
  if (m1->dbstart > m2->dbstart)
  {
    return 1;
  }
  if (m1->len < m2->len)
  {
    return -1;
  }
  if (m1->len > m2->len)
  {
    return 1;
  }
  return 0;
}

static int showSubstringmatch(void *a, GT_UNUSED void *info,
                              GT_UNUSED GtError *err)
{
  Substringmatch *m = (Substringmatch *) a;

  printf("%lu " FormatSeqpos " " Formatuint64_t " %lu\n",
           m->len,
           PRINTSeqposcast(m->dbstart),
           PRINTuint64_tcast(m->queryseqnum),
           m->querystart);
  return 0;
}

static unsigned long *sequence2markpositions(unsigned long *numofsequences,
                                             const GtUchar *seq,
                                             unsigned long seqlen)
{
  unsigned long *spacemarkpos, i, allocatedmarkpos, nextfreemarkpos;

  *numofsequences = 1UL;
  for (i=0; i<seqlen; i++)
  {
    if (seq[i] == (GtUchar) SEPARATOR)
    {
      (*numofsequences)++;
    }
  }
  if (*numofsequences == 1UL)
  {
    return NULL;
  }
  allocatedmarkpos = (*numofsequences)-1;
  ALLOCASSIGNSPACE(spacemarkpos,NULL,unsigned long,allocatedmarkpos);
  for (i=0, nextfreemarkpos = 0; i<seqlen; i++)
  {
    if (seq[i] == (GtUchar) SEPARATOR)
    {
      spacemarkpos[nextfreemarkpos++] = i;
    }
  }
  return spacemarkpos;
}

int testmaxpairs(const GtStr *indexname,
                 unsigned long samples,
                 unsigned int minlength,
                 Seqpos substringlength,
                 Verboseinfo *verboseinfo,
                 GtError *err)
{
  Encodedsequence *encseq;
  Seqpos totallength = 0, dblen, querylen;
  GtUchar *dbseq = NULL, *query = NULL;
  bool haserr = false;
  unsigned long s;
  GtArray *tabmaxquerymatches;
  Maxmatchselfinfo maxmatchselfinfo;

  showverbose(verboseinfo,"draw %lu samples",samples);
  encseq = mapencodedsequence(true,
                              indexname,
                              true,
                              false,
                              false,
                              false,
                              verboseinfo,
                              err);
  if (encseq == NULL)
  {
    haserr = true;
  } else
  {
    totallength = getencseqtotallength(encseq);
  }
  if (!haserr)
  {
    srand48(42349421);
    if (substringlength > totallength/2)
    {
      substringlength = totallength/2;
    }
    ALLOCASSIGNSPACE(dbseq,NULL,GtUchar,substringlength);
    ALLOCASSIGNSPACE(query,NULL,GtUchar,substringlength);
  }
  for (s=0; s<samples && !haserr; s++)
  {
    dblen = samplesubstring(dbseq,encseq,substringlength);
    querylen = samplesubstring(query,encseq,substringlength);
    showverbose(verboseinfo,"run query match for dblen=" FormatSeqpos
                            ",querylen= " FormatSeqpos ", minlength=%u",
           PRINTSeqposcast(dblen),PRINTSeqposcast(querylen),minlength);
    tabmaxquerymatches = gt_array_new(sizeof (Substringmatch));
    if (sarrquerysubstringmatch(dbseq,
                                dblen,
                                query,
                                (unsigned long) querylen,
                                minlength,
                                getencseqAlphabet(encseq),
                                storemaxmatchquery,
                                tabmaxquerymatches,
                                verboseinfo,
                                err) != 0)
    {
      haserr = true;
      break;
    }
    showverbose(verboseinfo,"run self match for dblen=" FormatSeqpos
                            ",querylen= " FormatSeqpos ", minlength=%u",
           PRINTSeqposcast(dblen),PRINTSeqposcast(querylen),minlength);
    maxmatchselfinfo.results = gt_array_new(sizeof (Substringmatch));
    maxmatchselfinfo.dblen = dblen;
    maxmatchselfinfo.querylen = (unsigned long) querylen;
    maxmatchselfinfo.markpos
      = sequence2markpositions(&maxmatchselfinfo.numofquerysequences,
                               query,(unsigned long) querylen);
    if (sarrselfsubstringmatch(dbseq,
                               dblen,
                               query,
                               (unsigned long) querylen,
                               minlength,
                               getencseqAlphabet(encseq),
                               storemaxmatchself,
                               &maxmatchselfinfo,
                               verboseinfo,
                               err) != 0)
    {
      haserr = true;
      break;
    }
    gt_array_sort(tabmaxquerymatches,orderSubstringmatch);
    gt_array_sort(maxmatchselfinfo.results,orderSubstringmatch);
    if (array_compare(tabmaxquerymatches,maxmatchselfinfo.results,
                      orderSubstringmatch) != 0)
    {
      const unsigned long width = 60UL;
      printf("querymatches\n");
      (void) gt_array_iterate(tabmaxquerymatches,showSubstringmatch,NULL,
                           err);
      printf("dbmatches\n");
      (void) gt_array_iterate(maxmatchselfinfo.results,showSubstringmatch,
                           NULL,err);
      symbolstring2fasta(stdout,"dbseq",
                         getencseqAlphabet(encseq),
                         dbseq,
                         (unsigned long) dblen,
                         width);
      symbolstring2fasta(stdout,"queryseq",
                         getencseqAlphabet(encseq),
                         query,
                         (unsigned long) querylen,
                         width);
      exit(GT_EXIT_PROGRAMMING_ERROR);
    }
    FREESPACE(maxmatchselfinfo.markpos);
    printf("# numberofmatches=%lu\n",gt_array_size(tabmaxquerymatches));
    gt_array_delete(tabmaxquerymatches);
    gt_array_delete(maxmatchselfinfo.results);
  }
  FREESPACE(dbseq);
  FREESPACE(query);
  encodedsequence_free(&encseq);
  return haserr ? -1 : 0;
}

#endif /* INLINEDSequentialsuffixarrayreader */
