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

#include <limits.h>
#include <stdio.h>
#include "core/assert_api.h"
#include "core/chardef.h"
#include "core/divmodmul.h"
#include "core/minmax.h"
#include "core/xansi.h"
#include "core/fa.h"
#include "core/arraydef.h"
#include "core/unused_api.h"
#include "core/symboldef.h"
#include "spacedef.h"
#include "encseq-def.h"
#include "turnwheels.h"
#include "esa-fileend.h"
#include "sfx-bentsedg.h"
#include "seqpos-def.h"
#include "bcktab.h"
#include "bltrie-ssort.h"
#include "lcpoverflow.h"
#include "opensfxfile.h"
#include "sfx-remainsort.h"
#include "sfx-copysort.h"
#include "kmer2string.h"
#include "stamp.h"

#include "sfx-cmpsuf.pr"

#define UNIQUEINT(P)           ((Seqpos) ((P) + GT_COMPAREOFFSET))
#define ACCESSCHAR(POS)        getencodedchar(bsr->encseq,POS,bsr->readmode)
#define ACCESSCHARSEQ(POS,ESR) sequentialgetencodedchar(bsr->encseq,ESR,POS,\
                                                        bsr->readmode)
#define ISNOTEND(POS)          ((POS) < bsr->totallength &&\
                                ISNOTSPECIAL(ACCESSCHAR(POS)))

#define DEREFSTOPPOSSEQ(VAR,PTR,STOPPOS,ESR)\
        (((PTR) < (STOPPOS) && ISNOTSPECIAL(VAR = ACCESSCHARSEQ(PTR,ESR))) ?\
        ((Seqpos) VAR) : UNIQUEINT(PTR))

#define DEREFSEQ(VAR,PTR,ESR) DEREFSTOPPOSSEQ(VAR,PTR,bsr->totallength,ESR)

#define LCPINDEX(LCPSUBTAB,I)   (Seqpos) ((I) - (LCPSUBTAB)->suftabbase)

#define SWAP(TMP,A,B)\
        if ((A) != (B))\
        {\
          TMP = *(A);\
          *(A) = *(B);\
          *(B) = TMP;\
        }

#define VECSWAP(A,B,N)\
        aptr = A;\
        bptr = B;\
        while ((N)-- > 0)\
        {\
          temp = *aptr;\
          *aptr++ = *bptr;\
          *bptr++ = temp;\
        }

#define SUFTABINDEX(PTR) (Seqpos) ((PTR) + bsr->suftab->offset -\
                                           bsr->suftab->sortspace)

#define STACKTOP\
        bsr->mkvauxstack.spaceMKVstack[bsr->mkvauxstack.nextfreeMKVstack]

#define UPDATELCP(MINVAL,MAXVAL)\
        gt_assert(commonunits.common < (unsigned int) UNITSIN2BITENC);\
        if ((MINVAL) > commonunits.common)\
        {\
          MINVAL = commonunits.common;\
        }\
        if ((MAXVAL) < commonunits.common)\
        {\
          MAXVAL = commonunits.common;\
        }

typedef Seqpos Suffixptr;

GT_DECLAREARRAYSTRUCT(Largelcpvalue);

typedef struct
{
  void *reservoir;
  size_t sizereservoir;
  Seqpos *bucketoflcpvalues, /* pointer into reservoir */
         maxbranchdepth,
         numoflargelcpvalues,
         totalnumoflargelcpvalues,
         countoutputlcpvalues;
  uint8_t *smalllcpvalues; /* pointer into reservoir */
  const Compressedtable *completelcpvalues;
  GtArrayLargelcpvalue largelcpvalues;
  const Seqpos *suftabbase;
} Lcpsubtab;

typedef struct
{
  bool defined;
  Codetype code;
  unsigned int prefixindex;
#undef SKDEBUG
#ifdef SKDEBUG
  Seqpos startpos;
#endif
} Suffixwithcode;

struct Outlcpinfo
{
  FILE *outfplcptab,
       *outfpllvtab;
  Seqpos totallength;
  Turningwheel *tw;
  unsigned int minchanged;
  Lcpsubtab lcpsubtab;
  Suffixwithcode previoussuffix;
  bool previousbucketwasempty,
       assideeffect;
};

#define CMPCHARBYCHARPTR2INT(VAR,TMPVAR,I)\
        VAR = (((cptr = *(I)+depth) < bsr->totallength &&\
              ISNOTSPECIAL(TMPVAR = ACCESSCHAR(cptr))) ? ((Seqpos) TMPVAR)\
                                                       : UNIQUEINT(cptr))

typedef EndofTwobitencoding Sfxcmp;

#define PTR2INT(VAR,IDXPTR)\
        {\
          Seqpos pos = *(IDXPTR);\
          if (bsr->fwd)\
          {\
            if (pos + depth < bsr->totallength)\
            {\
              pos += depth;\
              initEncodedsequencescanstategeneric(bsr->esr1,bsr->encseq,\
                                                  true,pos);\
              extract2bitenc(true,&(VAR),bsr->encseq,bsr->esr1,pos);\
            } else\
            {\
              VAR.tbe = 0;\
              VAR.unitsnotspecial = 0;\
              VAR.position = pos;\
            }\
          } else\
          {\
            pos = REVERSEPOS(bsr->totallength,pos);\
            if (pos >= depth)\
            {\
              pos -= depth;\
              initEncodedsequencescanstategeneric(bsr->esr1,bsr->encseq,\
                                                  false,pos);\
              extract2bitenc(false,&(VAR),bsr->encseq,bsr->esr1,pos);\
            } else\
            {\
              VAR.tbe = 0;\
              VAR.unitsnotspecial = 0;\
              VAR.position = pos;\
            }\
          }\
        }

#define Sfxdocompare(COMMONUNITS,X,Y)\
        ret##X##Y = compareTwobitencodings(bsr->fwd,bsr->complement,\
                                           COMMONUNITS,&X,&Y)

#define SfxcmpEQUAL(X,Y)      (ret##X##Y == 0)
#define SfxcmpSMALLER(X,Y)    (ret##X##Y < 0)
#define SfxcmpGREATER(X,Y)    (ret##X##Y > 0)

#ifdef SKDEBUG
static Seqpos baseptr;

static void showsuffixrange(const Encodedsequence *encseq,
                            bool fwd,
                            bool complement,
                            const Lcpsubtab *lcpsubtab,
                            const Suffixptr *leftptr,
                            const Suffixptr *rightptr,
                            Seqpos depth)
{
  const Suffixptr *pi;

  if (lcpsubtab == NULL)
  {
    printf("of %d suffixes at depth %d:\n",
           (int) ((rightptr) - (leftptr) + 1),
           (int) depth);
  } else
  {
    printf("of %d suffixes [%d,%d] at depth %d:\n",
           (int) ((rightptr) - (leftptr) + 1),
           (int) baseptr + LCPINDEX(lcpsubtab,leftptr),
           (int) baseptr + LCPINDEX(lcpsubtab,rightptr),
           (int) depth);
  }
  for (pi = leftptr; pi <= rightptr; pi++)
  {
    printf("suffix %d:",*pi);
    showsequenceatstartpos(stdout,
                           fwd,
                           complement,
                           encseq,
                           *pi);
  }
}
#endif

#undef CHECKSUFFIXRANGE
#ifdef CHECKSUFFIXRANGE
static void checksuffixrange(const Encodedsequence *encseq,
                             bool fwd,
                             bool complement,
                             const Lcpsubtab *lcpsubtab,
                             Seqpos *left,
                             Seqpos *right,
                             Seqpos depth,
                             int line)
{
  Seqpos *sufptr, newdepth = depth, pos1, pos2;

#ifdef SKDEBUG
  printf("checksuffixrange ");
  showsuffixrange(encseq,
                  fwd,
                  complement,
                  lcpsubtab,
                  left,
                  right,
                  depth);
#endif
  for (sufptr=left; sufptr<right; sufptr++)
  {
    if (fwd)
    {
      pos1 = *sufptr;
      pos2 = *(sufptr+1);
    } else
    {
      pos1 = REVERSEPOS(getencseqtotallength(encseq),*sufptr);
      pos2 = REVERSEPOS(getencseqtotallength(encseq),*(sufptr+1));
    }
    (void) comparetwostrings(encseq,
                             fwd,
                             complement,
                             &newdepth,
                             pos1,
                             pos2);
    if (depth > newdepth)
    {
      fprintf(stderr,"line %d: "
                     "depth=" FormatSeqpos " > " FormatSeqpos "=newdepth\n",
                     line,
                     PRINTSeqposcast(depth),
                     PRINTSeqposcast(newdepth));
      fprintf(stderr,"suffix " FormatSeqpos " vs " FormatSeqpos "\n",
                     PRINTSeqposcast(*sufptr),
                     PRINTSeqposcast(*(sufptr+1)));
      fprintf(stderr,"in range of length " FormatSeqpos "\n",
                     PRINTSeqposcast(right - left + 1));
      exit(GT_EXIT_PROGRAMMING_ERROR);
    }
  }
}
#endif

#ifdef WITHCHECKSTARTPOINTER
static unsigned int checkstartpointorder(const Seqpos *left,
                                         const Seqpos *right)
{
  const Seqpos *ptr;
  bool ascending;

  gt_assert(left < right);
  gt_assert(*left != *(left+1));
  ascending = (*left < *(left+1)) ? true : false;
  for (ptr = left+1; ptr < right; ptr++)
  {
    gt_assert(*ptr != *(ptr+1));
    if (*ptr < *(ptr+1))
    {
      if (!ascending)
      {
        return 0;
      }
    } else
    {
      if (*ptr > *(ptr+1))
      {
        if (ascending)
        {
          return 0;
        }
      }
    }
  }
  return ascending ? 1U : 2U;
}
#endif

typedef struct
{
  Suffixptr *left,
            *right;
  Seqpos depth;
  Ordertype ordertype;
} MKVstack;

typedef struct
{
  EndofTwobitencoding etbe;
  Suffixptr *suffixptr;
} Medianinfo;

typedef Medianinfo MedianElem;

typedef struct
{
  Seqpos suffix;
  unsigned char lcpwithpivot;
  char cmpresult;
} Countingsortinfo;

GT_DECLAREARRAYSTRUCT(MKVstack);

typedef struct
{
  const Encodedsequence *encseq;
  Encodedsequencescanstate *esr1, /* XXX be carefull with threads */
                           *esr2;
  Readmode readmode;
  bool fwd, complement, assideeffect;
  Seqpos totallength;
  GtArrayMKVstack mkvauxstack; /* XXX be carefull with treads */
  Lcpsubtab *lcpsubtab;
  Medianinfo *medianinfospace;
  Countingsortinfo *countingsortinfo;
  const Sfxstrategy *sfxstrategy;
  Blindtrie *blindtrie;
  Rmnsufinfo *rmnsufinfo;
  unsigned long leftlcpdist[UNITSIN2BITENC],
                rightlcpdist[UNITSIN2BITENC];
  DefinedSeqpos *longest;
  Suftab *suftab;
  void (*dc_processunsortedrange)(void *,Seqpos *,Seqpos *,Seqpos);
  void *voiddcov;
  bool *equalwithprevious;
  unsigned long countinsertionsort,
                countqsort,
                countcountingsort,
                countbltriesort;
} Bentsedgresources;

static Suffixptr *medianof3cmpcharbychar(const Bentsedgresources *bsr,
                                         Seqpos depth,
                                         Suffixptr *a,
                                         Suffixptr *b,
                                         Suffixptr *c)
{
  Seqpos vala, valb, valc;
  Suffixptr cptr;
  GtUchar tmpavar, tmpbvar;

  CMPCHARBYCHARPTR2INT(vala,tmpavar,a);
  CMPCHARBYCHARPTR2INT(valb,tmpbvar,b);
  if (vala == valb)
  {
    return a;
  }
  CMPCHARBYCHARPTR2INT(valc,tmpavar,c);
  if (vala == valc || valb == valc)
  {
    return c;
  }
  return vala < valb ?
        (valb < valc ? b : (vala < valc ? c : a))
      : (valb > valc ? b : (vala < valc ? a : c));
}

static Suffixptr *medianof3(const Bentsedgresources *bsr,
                            Seqpos depth,
                            Suffixptr *a,
                            Suffixptr *b,
                            Suffixptr *c)
{
  Sfxcmp vala, valb, valc;
  GtCommonunits commonunits;
  int retvalavalb, retvalavalc, retvalbvalc;

  PTR2INT(vala,a);
  PTR2INT(valb,b);
  Sfxdocompare(&commonunits,vala,valb);
  if (SfxcmpEQUAL(vala,valb))
  {
    return a;
  }
  PTR2INT(valc,c);
  Sfxdocompare(&commonunits,vala,valc);
  if (SfxcmpEQUAL(vala,valc))
  {
    return c;
  }
  Sfxdocompare(&commonunits,valb,valc);
  if (SfxcmpEQUAL(valb,valc))
  {
    return c;
  }
  return SfxcmpSMALLER(vala,valb) ?
        (SfxcmpSMALLER(valb,valc) ? b : (SfxcmpSMALLER(vala,valc) ? c : a))
      : (SfxcmpGREATER(valb,valc) ? b : (SfxcmpSMALLER(vala,valc) ? a : c));
}

static void updatelcpvalue(Bentsedgresources *bsr,Seqpos idx,Seqpos value)
{
  if (value >= (Seqpos) LCPOVERFLOW)
  {
    bsr->lcpsubtab->numoflargelcpvalues++; /* this may overcount as
                                              there may be some value
                                              which was already overflowing */
  }
  bsr->lcpsubtab->bucketoflcpvalues[idx] = value;
}

static void insertionsort(Bentsedgresources *bsr,
                          Suffixptr *leftptr,
                          Suffixptr *rightptr,
                          Seqpos offset)
{
  Suffixptr *pi, *pj;
  Seqpos lcpindex, lcplen = 0;
  int retval;
  Suffixptr ptr1, ptr2, temp;
  GtCommonunits commonunits;

#ifdef SKDEBUG
  printf("insertion sort ");
  showsuffixrange(bsr->encseq,bsr->fwd,bsr->complement,bsr->lcpsubtab,
                  leftptr,rightptr,offset);
#endif
  bsr->countinsertionsort++;
  for (pi = leftptr + 1; pi <= rightptr; pi++)
  {
    for (pj = pi; pj > leftptr; pj--)
    {
      if (bsr->sfxstrategy->cmpcharbychar)
      {
        ptr1 = (*(pj-1))+offset;
        initEncodedsequencescanstate(bsr->esr1,bsr->encseq,bsr->readmode,ptr1);
        ptr2 = (*pj)+offset;
        initEncodedsequencescanstate(bsr->esr2,bsr->encseq,bsr->readmode,ptr2);
        for (;;)
        {
          Seqpos ccs, cct;
          GtUchar tmp1, tmp2;

          ccs = DEREFSEQ(tmp1,ptr1,bsr->esr1);
          cct = DEREFSEQ(tmp2,ptr2,bsr->esr2);
          if (ccs != cct)
          {
            lcplen = (Seqpos) (ptr2 - *pj);
            retval = (ccs < cct) ? -1 : 1;
            break;
          }
          ptr1++;
          ptr2++;
        }
      } else
      {
#ifdef SKDEBUG
        printf("compareEncseqsequences[%d,%d] at offset %d\n",
                       (int) *(pj-1),(int) *pj,(int) offset);
        showsequenceatstartpos(stdout,
                               bsr->fwd,
                               bsr->complement,
                               bsr->encseq,
                               *(pj-1));
        showsequenceatstartpos(stdout,
                               bsr->fwd,
                               bsr->complement,
                               bsr->encseq,
                               *pj);
#endif
        retval = compareEncseqsequences(&commonunits,bsr->encseq,bsr->fwd,
                                        bsr->complement,
                                        bsr->esr1,bsr->esr2,*(pj-1),*pj,offset);
        lcplen = commonunits.finaldepth;
      }
      gt_assert(retval != 0);
      if (bsr->lcpsubtab != NULL && bsr->assideeffect)
      {
        lcpindex = LCPINDEX(bsr->lcpsubtab,pj);
        if (pj < pi && retval > 0)
        {
          updatelcpvalue(bsr,lcpindex+1,
                         bsr->lcpsubtab->bucketoflcpvalues[lcpindex]);
        }
        updatelcpvalue(bsr,lcpindex,lcplen);
      }
      if (retval < 0)
      {
        break;
      }
      SWAP(temp,pj,pj-1);
    }
  }
}

static void insertionsortmaxdepth(Bentsedgresources *bsr,
                                  Suffixptr *leftptr,
                                  Suffixptr *rightptr,
                                  Seqpos offset,
                                  Seqpos maxdepth)
{
  Suffixptr *pi, *pj, ptr1, ptr2, temp;
  Seqpos lcpindex, lcplen = 0;
  int retval;
  unsigned long idx = 0;
  bool tempb;
  GtCommonunits commonunits;

#ifdef SKDEBUG
  printf("insertion sort (offset=%lu,maxdepth=%lu)\n",
            (unsigned long) offset,
            (unsigned long) maxdepth);
  showsuffixrange(bsr->encseq,bsr->fwd,bsr->complement,bsr->lcpsubtab,
                  leftptr,rightptr,offset);
#endif
  bsr->countinsertionsort++;
  for (pi = leftptr + 1; pi <= rightptr; pi++)
  {
    for (pj = pi; pj > leftptr; pj--)
    {
      if (bsr->sfxstrategy->cmpcharbychar)
      {
        Seqpos endpos1, endpos2;

        endpos1 = MIN(bsr->totallength,*(pj-1)+maxdepth);
        endpos2 = MIN(bsr->totallength,*pj+maxdepth);
        ptr1 = (*(pj-1))+offset;
        if (ptr1 < bsr->totallength)
        {
          initEncodedsequencescanstate(bsr->esr1,bsr->encseq,bsr->readmode,
                                       ptr1);
        }
        ptr2 = (*pj)+offset;
        if (ptr2 < bsr->totallength)
        {
          initEncodedsequencescanstate(bsr->esr2,bsr->encseq,bsr->readmode,
                                       ptr2);
        }
        for (;;)
        {
          Seqpos ccs, cct;
          GtUchar tmp1, tmp2;

          ccs = DEREFSTOPPOSSEQ(tmp1,ptr1,endpos1,bsr->esr1);
          cct = DEREFSTOPPOSSEQ(tmp2,ptr2,endpos2,bsr->esr2);
          lcplen = (Seqpos) (ptr2 - *pj);
          if (lcplen == maxdepth)
          {
            retval = 0;
            break;
          }
          gt_assert(lcplen < maxdepth);
          if (ccs != cct)
          {
            retval = (ccs < cct) ? -1 : 1;
            break;
          }
          ptr1++;
          ptr2++;
        }
      } else
      {
        gt_assert(offset < maxdepth);
        retval = compareEncseqsequencesmaxdepth(&commonunits,bsr->encseq,
                                                bsr->fwd,
                                                bsr->complement,
                                                bsr->esr1,bsr->esr2,
                                                *(pj-1),*pj,offset,
                                                maxdepth);
        lcplen = commonunits.finaldepth;
        gt_assert(lcplen <= maxdepth);
        if (lcplen == maxdepth)
        {
          gt_assert(retval == 0);
        }
      }
#ifdef SKDEBUG
      printf("cmp %lu and %lu: retval = %d, lcplen = %lu\n",
                  (unsigned long) *(pj-1),
                  (unsigned long) *pj,
                  retval,
                  (unsigned long) lcplen);
#endif
      if (retval != 0 && bsr->lcpsubtab != NULL && bsr->assideeffect)
      {
        lcpindex = LCPINDEX(bsr->lcpsubtab,pj);
        if (pj < pi && retval > 0)
        {
          updatelcpvalue(bsr,lcpindex+1,
                         bsr->lcpsubtab->bucketoflcpvalues[lcpindex]);
        }
        updatelcpvalue(bsr,lcpindex,lcplen);
      }
      if (retval < 0)
      {
        break;
      }
      idx = (unsigned long) (pj - leftptr);
      if (retval == 0)
      {
        gt_assert(idx > 0);
        bsr->equalwithprevious[idx] = true;
        break;
      }
      SWAP(temp,pj,pj-1);
      tempb = bsr->equalwithprevious[idx-1];
      bsr->equalwithprevious[idx-1] = bsr->equalwithprevious[idx];
      bsr->equalwithprevious[idx] = tempb;
    }
  }
  if (idx > 0)
  {
    unsigned long equalsrangewidth = 0,
                  width = (unsigned long) (rightptr - leftptr + 1);
#ifdef SKDEBUG
    printf("ordered suffix %lu\n",(unsigned long) *leftptr);
#endif
    for (idx = 1UL; idx < width; idx++)
    {
#ifdef SKDEBUG
      printf("ordered suffix %lu, equalwithprevious=%s\n",
              (unsigned long) leftptr[idx],
              bsr->equalwithprevious[idx] ? "true" : "false");
#endif
      if (bsr->equalwithprevious[idx])
      {
        bsr->equalwithprevious[idx] = false;
        equalsrangewidth++;
      } else
      {
        if (equalsrangewidth > 0)
        {
#ifdef SKDEBUG
          printf("process interval of width %lu\n",
                 equalsrangewidth + 1);
#endif
          bsr->dc_processunsortedrange(bsr->voiddcov,
                                       leftptr + idx - 1 - equalsrangewidth,
                                       leftptr + idx - 1,maxdepth);
          equalsrangewidth = 0;
        }
      }
    }
    if (equalsrangewidth > 0)
    {
#ifdef SKDEBUG
      printf("process interval of width %lu\n",
             equalsrangewidth + 1);
#endif
      bsr->dc_processunsortedrange(bsr->voiddcov,
                                   leftptr + width - 1 - equalsrangewidth,
                                   leftptr + width - 1,maxdepth);
    }
  }
}

#define DOMEDIANCOMPARE(A,B)\
        compareTwobitencodings(fwd,complement,&commonunits,\
                               &((A)->etbe),&((B)->etbe))

#define MedianElemGREATER(A,B)  (DOMEDIANCOMPARE(A,B) > 0)

#define MedianElemSWAP(A,B)     {\
                                  register MedianElem tmp = *(A);\
                                                      *(A) = *(B);\
                                                      *(B) = tmp;\
                                }

/*
 *  This Quickselect routine is based on the algorithm described in
 *  "Numerical recipes in C", Second Edition,
 *  Cambridge University Press, 1992, Section 8.5, ISBN 0-521-43108-5
 *  This code by Nicolas Devillard - 1998. Public domain.
 */

static MedianElem *quickmedian (bool fwd,bool complement,
                                MedianElem *arr,unsigned long width)
{
  MedianElem *low, *high, *median, *middle, *ll, *hh;
  GtCommonunits commonunits;

  gt_assert(width > 0);
  low = arr;
  high = arr + width - 1;
  median = low + GT_DIV2(width);
  for (;;)
  {
    if (high <= low)                   /* One element only */
    {
      return median;
    }
    if (high == low + 1)
    {                                  /* Two elements only */
      if (MedianElemGREATER(low,high))
      {
        MedianElemSWAP (low, high);
      }
      return median;
    }

    /* Find median of low, middle and high items; swap into position low */
    middle = low + GT_DIV2(high - low + 1);
    if (MedianElemGREATER(middle,high))
    {
      MedianElemSWAP (middle, high);
    }
    if (MedianElemGREATER(low,high))
    {
      MedianElemSWAP (low, high);
    }
    if (MedianElemGREATER(middle,low))
    {
      MedianElemSWAP (middle, low);
    }
    /* Swap low item (now in position middle) into position (low+1) */
    MedianElemSWAP (middle, low + 1);

    /* Nibble from each end towards middle, swapping items when stuck */
    ll = low + 1;
    hh = high;
    for (;;)
    {
      do
      {
        ll++;
      } while (MedianElemGREATER(low,ll));
      do
      {
        hh--;
      } while  (MedianElemGREATER(hh,low));
      if (hh < ll)
      {
        break;
      }
      MedianElemSWAP (ll, hh);
    }

    /* Swap middle item (in position low) back into correct position */
    MedianElemSWAP (low, hh);

    /* Re-set active partition */
    if (hh <= median)
    {
      low = ll;
    }
    if (hh >= median)
    {
      high = hh - 1;
    }
  }
}

#ifdef WITHcheckmedian

static void checkmedian(bool fwd,
                        bool complement,
                        const Medianinfo *median,
                        const Medianinfo *space,
                        unsigned long width)
{
  unsigned long sum1, sum2, idx, smaller = 0, larger = 0, equal = 0, equalpart;
  unsigned int commonunits;
  int cmp;

  for (idx = 0; idx < width; idx++)
  {
    cmp = DOMEDIANCOMPARE(space + idx,median);
    if (cmp > 0)
    {
     larger++;
    } else
    {
      if (cmp < 0)
      {
        smaller++;
      } else
      {
        equal++;
      }
    }
  }
  if (smaller == larger)
  {
    return;
  }
  for (equalpart = 0; equalpart < equal; equalpart++)
  {
    sum1 = smaller + equalpart;
    sum2 = larger + (equal-1) - equalpart;
    if (sum1 < sum2)
    {
      if (sum1 + 1 == sum2)
      {
        return;
      }
    } else
    {
      if (sum1 > sum2)
      {
        if (sum1 == sum2 + 1)
        {
          return;
        }
      } else
      {
        return;
      }
    }
  }
  fprintf(stderr,"problem with equal=%lu,smaller=%lu,larger=%lu\n",
                  equal,smaller,larger);
  exit(GT_EXIT_PROGRAMMING_ERROR);
}
#endif

static Suffixptr *realmedian(const Bentsedgresources *bsr,
                             Suffixptr *left,
                             Seqpos depth,
                             unsigned long width)
{
  Medianinfo *medianptr;
  unsigned long idx;

  for (idx = 0; idx < width; idx++)
  {
    bsr->medianinfospace[idx].suffixptr = left + idx;
    PTR2INT(bsr->medianinfospace[idx].etbe,left+idx);
  }
  medianptr = quickmedian(bsr->fwd,bsr->complement,bsr->medianinfospace,width);
/*
  checkmedian(bsr->fwd,bsr->complement,medianptr,medianinfospace,width);
*/
  gt_assert(medianptr != NULL);
  return medianptr->suffixptr;
}

#define MINMEDIANOF9WIDTH 31UL

static Suffixptr *cmpcharbychardelivermedian(const Bentsedgresources *bsr,
                                             Seqpos *left,
                                             Seqpos *right,
                                             Seqpos depth,
                                             unsigned long width)
{
  Seqpos *pl = left, *pm = left + GT_DIV2(width), *pr = right;

  if (width >= MINMEDIANOF9WIDTH)
  { /* On big arrays, pseudomedian of 9 */
    unsigned long offset, doubleoffset;
    offset = GT_DIV8(width);
    doubleoffset = GT_MULT2(offset);
    pl = medianof3cmpcharbychar(bsr,depth,pl,pl+offset,pl+doubleoffset);
    pm = medianof3cmpcharbychar(bsr,depth,pm-offset,pm,pm+offset);
    pr = medianof3cmpcharbychar(bsr,depth,pr-doubleoffset,pr-offset,pr);
  }
  return medianof3cmpcharbychar(bsr,depth,pl,pm,pr);
}

static Suffixptr *blockcmpdelivermedian(const Bentsedgresources *bsr,
                                        Seqpos *left,
                                        Seqpos *right,
                                        Seqpos depth,
                                        unsigned long width,
                                        unsigned long maxwidthrealmedian)
{
  Suffixptr *pl = left, *pm = left + GT_DIV2(width), *pr = right;

  if (width >= MINMEDIANOF9WIDTH)
  {
    if (width > maxwidthrealmedian)
    { /* On big arrays, pseudomedian of 9 */
      unsigned long offset, doubleoffset;
      offset = GT_DIV8(width);
      doubleoffset = GT_MULT2(offset);
      pl = medianof3(bsr,depth,pl,pl+offset,pl+doubleoffset);
      pm = medianof3(bsr,depth,pm-offset,pm,pm+offset);
      pr = medianof3(bsr,depth,pr-doubleoffset,pr-offset,pr);
      pm = medianof3(bsr,depth,pl,pm,pr);
    } else /* width <= maxwidthrealmedian */
    {
      pm = realmedian(bsr, left, depth, width);
    }
  } else
  {
    pm = medianof3(bsr,depth,pl,pm,pr);
  }
  return pm;
}

/*
static void showcountingsortinfo(const Countingsortinfo *countingsortinfo,
                              unsigned long idx)
{
  printf("countingsortinfo[%lu]=(%lu,",idx,
          (unsigned long) countingsortinfo[idx].suffix);
  printf("%lu,",(unsigned long) countingsortinfo[idx].lcpwithpivot);
  printf("%d)\n",countingsortinfo[idx].cmpresult);
}
*/

static Ordertype deriveordertype(Ordertype parentordertype,bool turn)
{
  switch (parentordertype)
  {
    case Noorder : return Noorder;
    case Descending: return turn ? Ascending : Descending;
    case Ascending: return turn ? Descending : Ascending;
  }
  /*@ignore@*/
  return Noorder;
  /*@end@*/
}

static bool comparisonsort(Bentsedgresources *bsr,
                           Suffixptr *left,
                           Suffixptr *right,
                           Seqpos depth,
                           unsigned long width,
                           Ordertype ordertype)
{
  gt_assert(width > 1UL && left < right);
  gt_assert(bsr->sfxstrategy->maxinsertionsort <=
            bsr->sfxstrategy->maxbltriesort);
  if (width <= bsr->sfxstrategy->maxinsertionsort)
  {
    insertionsort(bsr,left,right,depth);
    return true;
  }
  if (width <= bsr->sfxstrategy->maxbltriesort)
  {
    Seqpos numoflargelcpvalues;

    gt_assert(bsr->sfxstrategy->differencecover == 0);
    numoflargelcpvalues
      = blindtrie_suffixsort(bsr->blindtrie,left,
                             bsr->lcpsubtab == NULL
                               ? NULL
                               : bsr->lcpsubtab->bucketoflcpvalues +
                                 LCPINDEX(bsr->lcpsubtab,left),
                             width,depth,
                             (Seqpos) bsr->sfxstrategy->differencecover,
                             ordertype,
                             NULL,
                             NULL);
    if (bsr->lcpsubtab != NULL)
    {
      bsr->lcpsubtab->numoflargelcpvalues += numoflargelcpvalues;
    }
    bsr->countbltriesort++;
    return true;
  }
  return false;
}

static void subsort_bentleysedgewick(Bentsedgresources *bsr,
                                     unsigned long width,
                                     Seqpos *left,
                                     Seqpos *right,
                                     Seqpos depth,
                                     Ordertype ordertype)
{
  /*
  checksuffixrange(bsr->encseq,
                   bsr->fwd,
                   bsr->complement,
                   bsr->lcpsubtab,
                   left,
                   right,
                   depth,
                   __LINE__);
  */
  if (width > 1UL)
  {
    if (bsr->sfxstrategy->ssortmaxdepth.defined)
    {
      if (depth >= (Seqpos) bsr->sfxstrategy->ssortmaxdepth.valueunsignedint)
      {
        rmnsufinfo_addunsortedrange(bsr->rmnsufinfo,SUFTABINDEX(left),
                                    SUFTABINDEX(right),depth);
        return;
      }
    } else
    {
      if (bsr->sfxstrategy->differencecover > 0)
      {
        if (depth >= (Seqpos) bsr->sfxstrategy->differencecover)
        {
          bsr->dc_processunsortedrange(bsr->voiddcov,left,right,depth);
          return;
        }
        if (width <= bsr->sfxstrategy->maxinsertionsort)
        {
          insertionsortmaxdepth(bsr,left,right,depth,
                                (Seqpos) bsr->sfxstrategy->
                                              differencecover);
          return;
        }
        if (width <= bsr->sfxstrategy->maxbltriesort)
        {
          Seqpos numoflargelcpvalues;

          numoflargelcpvalues
            = blindtrie_suffixsort(bsr->blindtrie,
                                   left,
                                   bsr->lcpsubtab == NULL
                                     ? NULL
                                     : bsr->lcpsubtab->bucketoflcpvalues +
                                       LCPINDEX(bsr->lcpsubtab,left),
                                   width,
                                   depth,
                                   (Seqpos) bsr->sfxstrategy->differencecover,
                                   ordertype,
                                   bsr->voiddcov,
                                   bsr->dc_processunsortedrange);
          if (bsr->lcpsubtab != NULL)
          {
            bsr->lcpsubtab->numoflargelcpvalues += numoflargelcpvalues;
          }
          bsr->countbltriesort++;
          return;
        }
      } else
      {
        if (comparisonsort(bsr,left,right,depth,width,ordertype))
        {
          return;
        }
      }
    }
    /* push */
    GT_CHECKARRAYSPACE(&bsr->mkvauxstack,MKVstack,1024);
    STACKTOP.left = left;
    STACKTOP.right = right;
    STACKTOP.depth = depth;
    STACKTOP.ordertype = ordertype;
    bsr->mkvauxstack.nextfreeMKVstack++;
  }
}

static void sarrcountingsort(Bentsedgresources *bsr,
                             Seqpos *left,
                             const Sfxcmp *pivotcmpbits,
                             unsigned long pivotidx,
                             Ordertype parentordertype,
                             Seqpos depth,
                             unsigned long width)
{
  int cmp;
  unsigned int maxsmallerwithlcp = 0, maxlargerwithlcp = 0;
  GtCommonunits commonunits;
  EndofTwobitencoding etbecurrent;
  unsigned long idx, smaller = 0, larger = 0,
                insertindex, end, equaloffset, currentwidth;
  Countingsortinfo *csiptr;
  /* const bool cmpcharbychar = false; */

  bsr->countcountingsort++;
  for (idx = 0; idx < width; idx++)
  {
    if (idx != pivotidx)
    {
      PTR2INT(etbecurrent,left+idx);
      cmp = compareTwobitencodings(bsr->fwd,bsr->complement,&commonunits,
                                   &etbecurrent,pivotcmpbits);
      bsr->countingsortinfo[idx].suffix = left[idx];
      gt_assert(commonunits.common <= (unsigned int) UNITSIN2BITENC);
      bsr->countingsortinfo[idx].lcpwithpivot = commonunits.common;
      if (cmp > 0)
      {
        gt_assert(commonunits.common < (unsigned int) UNITSIN2BITENC);
        bsr->rightlcpdist[commonunits.common]++;
        if (maxlargerwithlcp < commonunits.common)
        {
          maxlargerwithlcp = commonunits.common;
        }
        bsr->countingsortinfo[idx].cmpresult = (char) 1;
        larger++;
      } else
      {
        if (cmp < 0)
        {
          gt_assert(commonunits.common < (unsigned int) UNITSIN2BITENC);
          bsr->leftlcpdist[commonunits.common]++;
          if (maxsmallerwithlcp < commonunits.common)
          {
            maxsmallerwithlcp = commonunits.common;
          }
          bsr->countingsortinfo[idx].cmpresult = (char) -1;
          smaller++;
        } else
        {
          gt_assert(commonunits.common == (unsigned int) UNITSIN2BITENC);
          bsr->countingsortinfo[idx].cmpresult = 0;
        }
      }
    } else
    {
      bsr->countingsortinfo[idx].suffix = left[idx];
      bsr->countingsortinfo[idx].lcpwithpivot = (unsigned char) UNITSIN2BITENC;
      bsr->countingsortinfo[idx].cmpresult = (char) 0;
    }
  }
  for (idx = 1UL; idx <= (unsigned long) maxsmallerwithlcp; idx++)
  {
    bsr->leftlcpdist[idx] += bsr->leftlcpdist[idx-1];
  }
  for (idx = 1UL; idx <= (unsigned long) maxlargerwithlcp; idx++)
  {
    bsr->rightlcpdist[idx] += bsr->rightlcpdist[idx-1];
  }
  equaloffset = width - larger;
  for (csiptr = bsr->countingsortinfo + width -1;
       csiptr >= bsr->countingsortinfo;
       csiptr--)
  {
    switch (csiptr->cmpresult)
    {
      case -1:
        insertindex = --(bsr->leftlcpdist[csiptr->lcpwithpivot]);
        left[insertindex] = csiptr->suffix;
        break;
      case 0:
        left[--equaloffset] = csiptr->suffix;
        break;
      case 1:
        insertindex = --(bsr->rightlcpdist[csiptr->lcpwithpivot]);
        left[width - 1 - insertindex] = csiptr->suffix;
        break;
    }
  }
  for (idx = 0; idx <= (unsigned long) maxsmallerwithlcp; idx++)
  {
    if (idx < (unsigned long) maxsmallerwithlcp)
    {
      end = bsr->leftlcpdist[idx+1];
    } else
    {
      end = smaller;
    }
    if (bsr->leftlcpdist[idx] + 1 < end) /* at least two elements */
    {
      currentwidth = end - bsr->leftlcpdist[idx];
      subsort_bentleysedgewick(bsr,currentwidth,left + bsr->leftlcpdist[idx],
                               left + end - 1,depth + idx,
                               deriveordertype(parentordertype,false));
    }
    if (bsr->lcpsubtab != NULL && bsr->assideeffect &&
        bsr->leftlcpdist[idx] < end)
    { /* at least one element */
      updatelcpvalue(bsr,LCPINDEX(bsr->lcpsubtab,left + end),depth + idx);
    }
    bsr->leftlcpdist[idx] = 0;
  }
  if (width - smaller - larger > 1UL)
  {
    currentwidth = width - smaller - larger;
    subsort_bentleysedgewick(bsr,currentwidth,left+smaller,left+width-larger-1,
                             depth + UNITSIN2BITENC,
                             deriveordertype(parentordertype,false));
  }
  for (idx = 0; idx <= (unsigned long) maxlargerwithlcp; idx++)
  {
    if (idx < (unsigned long) maxlargerwithlcp)
    {
      end = bsr->rightlcpdist[idx+1];
    } else
    {
      end = larger;
    }
    if (bsr->rightlcpdist[idx] + 1 < end) /* at least two elements */
    {
      currentwidth = end - bsr->rightlcpdist[idx];
      subsort_bentleysedgewick(bsr,currentwidth,left+width-end,
                               left + width - 1 - bsr->rightlcpdist[idx],
                               depth + idx,
                               deriveordertype(parentordertype,true));
    }
    if (bsr->lcpsubtab != NULL && bsr->assideeffect &&
        bsr->rightlcpdist[idx] < end)
    { /* at least one element */
      updatelcpvalue(bsr,LCPINDEX(bsr->lcpsubtab,left + width - end),
                     depth + idx);
    }
    bsr->rightlcpdist[idx] = 0;
  }
}

static void bentleysedgewick(Bentsedgresources *bsr,
                             Suffixptr *left,
                             Suffixptr *right,
                             Seqpos depth)
{
  bsr->mkvauxstack.nextfreeMKVstack = 0;
  subsort_bentleysedgewick(bsr, (unsigned long) (right - left + 1),
                           left,right,depth,Descending);
  while (bsr->mkvauxstack.nextfreeMKVstack > 0)
  {
    Suffixptr *leftplusw, *pa, *pb, *pc, *pd, *pm, *aptr, *bptr, cptr, temp;
    Seqpos pivotcmpcharbychar = 0, valcmpcharbychar;
    Sfxcmp pivotcmpbits, val;
    int retvalpivotcmpbits;
    GtUchar tmpvar;
    Ordertype parentordertype;
    unsigned long w, width;
    GtCommonunits commonunits;
    unsigned int smallermaxlcp, greatermaxlcp, smallerminlcp, greaterminlcp;
    const int commonunitsequal = bsr->sfxstrategy->cmpcharbychar
                                 ? 1
                                 : UNITSIN2BITENC;

    /* pop */
    bsr->mkvauxstack.nextfreeMKVstack--;
    left = STACKTOP.left;
    right = STACKTOP.right;
    depth = STACKTOP.depth;
    parentordertype = STACKTOP.ordertype;
    width = (unsigned long) (right - left + 1);

    if (bsr->sfxstrategy->cmpcharbychar)
    {
      pm = cmpcharbychardelivermedian(bsr,
                                      left,
                                      right,
                                      depth,
                                      width);
      SWAP(temp, left, pm);
      CMPCHARBYCHARPTR2INT(pivotcmpcharbychar,tmpvar,left);
    } else
    {
      pm = blockcmpdelivermedian(bsr,
                                 left,
                                 right,
                                 depth,
                                 width,
                                 bsr->sfxstrategy->maxwidthrealmedian);
      if (width <= (unsigned long) bsr->sfxstrategy->maxcountingsort &&
          width >= MINMEDIANOF9WIDTH)
      {
        PTR2INT(pivotcmpbits,pm);
        sarrcountingsort(bsr,
                         left,
                         &pivotcmpbits,
                         (unsigned long) (pm - left),
                         parentordertype,
                         depth,
                         width);
        /* new values for left, right, depth and parentordertype */
        continue;
      }
      SWAP(temp, left, pm);
      PTR2INT(pivotcmpbits,left);
    }
    bsr->countqsort++;
    /* now pivot element is at index left */
    /* all elements to be compared are between pb and pc */
    /* pa is the position at which the next element smaller than the
       pivot element is inserted at */
    /* pd is the position at which the next element greater than the
       pivot element is inserted at */
    pa = pb = left + 1;
    pc = pd = right;
    if (bsr->sfxstrategy->cmpcharbychar)
    {
      smallerminlcp = greaterminlcp = smallermaxlcp = greatermaxlcp = 0;
      for (;;)
      {
        while (pb <= pc)
        {
          CMPCHARBYCHARPTR2INT(valcmpcharbychar,tmpvar,pb);
          if (valcmpcharbychar > pivotcmpcharbychar)
          {
            break;
          }
          if (valcmpcharbychar == pivotcmpcharbychar)
          {
            SWAP(temp, pa, pb);
            pa++;
          }
          pb++;
        }
        while (pb <= pc)
        {
          CMPCHARBYCHARPTR2INT(valcmpcharbychar,tmpvar,pc);
          if (valcmpcharbychar < pivotcmpcharbychar)
          { /* stop for elements < pivot */
            break;
          }
          if (valcmpcharbychar == pivotcmpcharbychar)
          {
            SWAP(temp, pc, pd); /* exchange equal element and element
                                   at index pd */
            pd--;
          }
          pc--;
        }
        if (pb > pc)
        { /* no elements to compare to pivot */
          break;
        }
        SWAP(temp, pb, pc);
        pb++;
        pc--;
      }
    } else
    {
      smallermaxlcp = greatermaxlcp = 0;
      smallerminlcp = greaterminlcp = (unsigned int) UNITSIN2BITENC;
      for (;;)
      {
        /* look for elements identical or smaller than pivot from left */
        while (pb <= pc)
        {
          PTR2INT(val,pb);
          Sfxdocompare(&commonunits,val,pivotcmpbits);
          if (SfxcmpGREATER(val,pivotcmpbits))
          { /* stop for elements val > pivot */
            UPDATELCP(greaterminlcp,greatermaxlcp);
            break;
          }
          if (SfxcmpEQUAL(val,pivotcmpbits))
          {
            SWAP(temp, pa, pb); /* exchange equal element and element
                                   at index pa */
            pa++;
          } else /* smaller */
          {
            UPDATELCP(smallerminlcp,smallermaxlcp);
          }
          pb++;
        }
        /* look for elements identical or greater than pivot from right */
        while (pb <= pc)
        {
          PTR2INT(val,pc);
          Sfxdocompare(&commonunits,val,pivotcmpbits);
          if (SfxcmpSMALLER(val,pivotcmpbits))
          { /* stop for elements val < pivot */
            UPDATELCP(smallerminlcp,smallermaxlcp);
            break;
          }
          if (SfxcmpEQUAL(val,pivotcmpbits))
          {
            SWAP(temp, pc, pd); /* exchange equal element and element at
                                   index pa */
            pd--;
          } else /* greater */
          {
            UPDATELCP(greaterminlcp,greatermaxlcp);
          }
          pc--;
        }
        if (pb > pc)
        { /* interval is empty */
          break;
        }
        SWAP(temp, pb, pc);
        pb++;
        pc--;
      }
    }
    gt_assert(pa >= left);
    gt_assert(pb >= pa);
    w = MIN((unsigned long) (pa-left),(unsigned long) (pb-pa));
    /* move w elements at the left to the middle */
    VECSWAP(left,  pb-w, w);

    gt_assert(pd >= pc);
    gt_assert(right >= pd);
    w = MIN((unsigned long) (pd-pc), (unsigned long) (right-pd));
    /* move w elements at the right to the middle */
    VECSWAP(pb, right+1-w, w);

    /* all elements equal to the pivot are now in the middle namely in the
       range [left + (pb-pa) and right - (pd-pc)] */
    /* hence we have to sort the elements in the intervals
       [left..left+(pb-pa)-1] and
       [right-(pd-pc)+1..right] */

    gt_assert(pb >= pa);
    if ((w = (unsigned long) (pb-pa)) > 0)
    {
      leftplusw = left + w;
      if (bsr->lcpsubtab != NULL && bsr->assideeffect)
      {
        /*
          left part has suffix with lcp up to length smallermaxlcp w.r.t.
          to the pivot. This lcp belongs to a suffix on the left
          which is at a minimum distance to the pivot and thus to an
          element in the final part of the left side.
        */
        updatelcpvalue(bsr,LCPINDEX(bsr->lcpsubtab,leftplusw),
                       depth + smallermaxlcp);
      }
      subsort_bentleysedgewick(bsr,w,left,leftplusw-1,depth + smallerminlcp,
                               Noorder);
    } else
    {
      leftplusw = left;
    }

    cptr = *leftplusw + depth;
    if (ISNOTEND(cptr))
    {
      width = (unsigned long) (right-(pd-pb)-leftplusw);
      subsort_bentleysedgewick(bsr,width,leftplusw,right-(pd-pb)-1,
                               depth+commonunitsequal,Noorder);
    }

    gt_assert(pd >= pc);
    if ((w = (unsigned long) (pd-pc)) > 0)
    {
      if (bsr->lcpsubtab != NULL && bsr->assideeffect)
      {
        /*
          right part has suffix with lcp up to length largermaxlcp w.r.t.
          to the pivot. This lcp belongs to a suffix on the right
          which is at a minimum distance to the pivot and thus to an
          element in the first part of the right side.
        */
        updatelcpvalue(bsr,LCPINDEX(bsr->lcpsubtab,right-w+1),
                       depth + greatermaxlcp);
      }
      subsort_bentleysedgewick(bsr,w,right-w+1,right,depth + greaterminlcp,
                               Noorder);
    }
  }
}

#ifdef WITHbruteforcelcpvalue
static void showSuffixwithcode(FILE *fp,const Suffixwithcode *suffix)
{
  char buffer[18+1];

  fromkmercode2string(buffer,
                      suffix->code,
                      4,
                      8,
                      "acgt");
  fprintf(fp,"(startpos=%lu,code=%u,prefixindex=%u,\"%s\")",
              (unsigned long) suffix->startpos,
              (unsigned int) suffix->code,
              suffix->prefixindex,
              buffer);
}

static Seqpos bruteforcelcpvalue(const Encodedsequence *encseq,
                                 Readmode readmode,
                                 const Suffixwithcode *previoussuffix,
                                 const Suffixwithcode *currentsuffix,
                                 unsigned int minchanged,
                                 Encodedsequencescanstate *esr1,
                                 Encodedsequencescanstate *esr2)
{
  Seqpos lcpvalue;
  unsigned int lcpvalue2;
  int cmp;

  cmp = comparetwosuffixes(encseq,
                           readmode,
                           &lcpvalue,
                           false,
                           false,
                           0,
                           previoussuffix->startpos,
                           currentsuffix->startpos,
                           esr1,
                           esr2);
  if (cmp > 0)
  {
    fprintf(stderr,"cmp " FormatSeqpos
            " " FormatSeqpos " = %d, lcpval=" FormatSeqpos "\n",
            PRINTSeqposcast(previoussuffix->startpos),
            PRINTSeqposcast(currentsuffix->startpos),
            cmp,
            PRINTSeqposcast(lcpvalue));
    exit(GT_EXIT_PROGRAMMING_ERROR);
  }
  if (previoussuffix->code == currentsuffix->code)
  {
    gt_assert(lcpvalue == MIN(previoussuffix->prefixindex,
                           currentsuffix->prefixindex));
  } else
  {
    gt_assert(previoussuffix->code < currentsuffix->code);
    lcpvalue2 = MIN(minchanged,MIN(previoussuffix->prefixindex,
                                   currentsuffix->prefixindex));
    if (lcpvalue != lcpvalue2)
    {
      fprintf(stderr,"lcpvalue = %lu != %u = lcpvalue2\n",
              (unsigned long) lcpvalue,
              lcpvalue2);
      fprintf(stderr,"previoussuffix=");
      showSuffixwithcode(stderr,previoussuffix);
      fprintf(stderr,"\ncurrentsuffix=");
      showSuffixwithcode(stderr,currentsuffix);
      fprintf(stderr,"\n");
      exit(GT_EXIT_PROGRAMMING_ERROR);
    }
  }
  return lcpvalue;
}
#endif

static Seqpos computelocallcpvalue(const Suffixwithcode *previoussuffix,
                                   const Suffixwithcode *currentsuffix,
                                   unsigned int minchanged)
{
  unsigned int lcpvalue;

  if (previoussuffix->code == currentsuffix->code)
  {
    lcpvalue = MIN(previoussuffix->prefixindex,
                   currentsuffix->prefixindex);
  } else
  {
    gt_assert(previoussuffix->code < currentsuffix->code);
    lcpvalue = MIN(minchanged,MIN(previoussuffix->prefixindex,
                                  currentsuffix->prefixindex));
  }
  return (Seqpos) lcpvalue;
}

static unsigned int bucketends(Outlcpinfo *outlcpinfo,
                               Suffixwithcode *previoussuffix,
                               GT_UNUSED Seqpos firstspecialsuffix,
                               unsigned int minchanged,
                               unsigned long specialsinbucket,
                               Codetype code,
                               const Bcktab *bcktab)
{
  Seqpos lcpvalue;
  unsigned int maxprefixindex, minprefixindex;
  Suffixwithcode firstspecialsuffixwithcode;

  /*
     there is at least one element in the bucket. if there is more than
     one element in the bucket, then we insert them using the
     information from the bcktab
  */
  if (specialsinbucket > 1UL)
  {
    maxprefixindex = pfxidx2lcpvalues(&minprefixindex,
                                      outlcpinfo->lcpsubtab.smalllcpvalues,
                                      specialsinbucket,
                                      bcktab,
                                      code);
    if (outlcpinfo->lcpsubtab.maxbranchdepth < (Seqpos) maxprefixindex)
    {
      outlcpinfo->lcpsubtab.maxbranchdepth = (Seqpos) maxprefixindex;
    }
  } else
  {
    minprefixindex = maxprefixindex = singletonmaxprefixindex(bcktab,code);
  }
  firstspecialsuffixwithcode.code = code;
  firstspecialsuffixwithcode.prefixindex = maxprefixindex;
#ifdef SKDEBUG
  firstspecialsuffixwithcode.startpos = firstspecialsuffix;
  /*
  consistencyofsuffix(__LINE__,
                      encseq,readmode,bcktab,numofchars,
                      &firstspecialsuffixwithcode);
  */
#endif
  lcpvalue = computelocallcpvalue(previoussuffix,
                                  &firstspecialsuffixwithcode,
                                  minchanged);
  if (outlcpinfo->lcpsubtab.maxbranchdepth < lcpvalue)
  {
    outlcpinfo->lcpsubtab.maxbranchdepth = lcpvalue;
  }
  outlcpinfo->lcpsubtab.smalllcpvalues[0] = (uint8_t) lcpvalue;
  outlcpinfo->lcpsubtab.countoutputlcpvalues += specialsinbucket;
  gt_xfwrite(outlcpinfo->lcpsubtab.smalllcpvalues,
             sizeof (*outlcpinfo->lcpsubtab.smalllcpvalues),
             (size_t) specialsinbucket,
             outlcpinfo->outfplcptab);
  return minprefixindex;
}

Outlcpinfo *newOutlcpinfo(const GtStr *indexname,
                          unsigned int prefixlength,
                          unsigned int numofchars,
                          Seqpos totallength,
                          bool assideeffect,
                          GtError *err)
{
  bool haserr = false;
  Outlcpinfo *outlcpinfo;

  ALLOCASSIGNSPACE(outlcpinfo,NULL,Outlcpinfo,1);
  if (indexname == NULL)
  {
    outlcpinfo->outfplcptab = NULL;
    outlcpinfo->outfpllvtab = NULL;
  } else
  {
    outlcpinfo->outfplcptab = opensfxfile(indexname,LCPTABSUFFIX,"wb",err);
    if (outlcpinfo->outfplcptab == NULL)
    {
      haserr = true;
    }
    if (!haserr)
    {
      outlcpinfo->outfpllvtab
        = opensfxfile(indexname,LARGELCPTABSUFFIX,"wb",err);
      if (outlcpinfo->outfpllvtab == NULL)
      {
        haserr = true;
      }
    }
  }
  outlcpinfo->assideeffect = assideeffect;
  outlcpinfo->lcpsubtab.countoutputlcpvalues = 0;
  outlcpinfo->totallength = totallength;
  outlcpinfo->lcpsubtab.totalnumoflargelcpvalues = 0;
  outlcpinfo->lcpsubtab.maxbranchdepth = 0;
  outlcpinfo->lcpsubtab.reservoir = NULL;
  outlcpinfo->lcpsubtab.sizereservoir = 0;
  GT_INITARRAY(&outlcpinfo->lcpsubtab.largelcpvalues,Largelcpvalue);
  outlcpinfo->lcpsubtab.smalllcpvalues = NULL;
  outlcpinfo->minchanged = 0;
  if (assideeffect)
  {
    outlcpinfo->tw = newTurningwheel(prefixlength,numofchars);
  } else
  {
    outlcpinfo->tw = NULL;
  }
#ifdef SKDEBUG
  outlcpinfo->previoussuffix.startpos = 0;
#endif
  outlcpinfo->previoussuffix.code = 0;
  outlcpinfo->previoussuffix.prefixindex = 0;
  outlcpinfo->previoussuffix.defined = false;
  outlcpinfo->previousbucketwasempty = false;
  if (haserr)
  {
    FREESPACE(outlcpinfo);
    return NULL;
  }
  return outlcpinfo;
}

static void outlcpvalues(Lcpsubtab *lcpsubtab,
                         unsigned long bucketleft,
                         unsigned long bucketright,
                         Seqpos posoffset,
                         FILE *fplcptab,
                         FILE *fpllvtab)
{
  unsigned long idx;
  Seqpos lcpvalue;
  Largelcpvalue *largelcpvalueptr;

  lcpsubtab->largelcpvalues.nextfreeLargelcpvalue = 0;
  if (lcpsubtab->numoflargelcpvalues > 0 &&
      lcpsubtab->numoflargelcpvalues >=
      (Seqpos) lcpsubtab->largelcpvalues.allocatedLargelcpvalue)
  {
    lcpsubtab->largelcpvalues.spaceLargelcpvalue
      = gt_realloc(lcpsubtab->largelcpvalues.spaceLargelcpvalue,
                   sizeof (Largelcpvalue) * lcpsubtab->numoflargelcpvalues);
    lcpsubtab->largelcpvalues.allocatedLargelcpvalue
      = (unsigned long) lcpsubtab->numoflargelcpvalues;
  }
  for (idx=bucketleft; idx<=bucketright; idx++)
  {
    if (lcpsubtab->bucketoflcpvalues != NULL)
    {
      lcpvalue = lcpsubtab->bucketoflcpvalues[idx];
    } else
    {
      lcpvalue = compressedtable_get(lcpsubtab->completelcpvalues,(Seqpos) idx);
    }
    if (lcpsubtab->maxbranchdepth < lcpvalue)
    {
      lcpsubtab->maxbranchdepth = lcpvalue;
    }
    if (lcpvalue < (Seqpos) LCPOVERFLOW)
    {
      lcpsubtab->smalllcpvalues[idx-bucketleft] = (uint8_t) lcpvalue;
    } else
    {
      gt_assert(lcpsubtab->largelcpvalues.nextfreeLargelcpvalue <
                lcpsubtab->largelcpvalues.allocatedLargelcpvalue);
      largelcpvalueptr = lcpsubtab->largelcpvalues.spaceLargelcpvalue +
                         lcpsubtab->largelcpvalues.nextfreeLargelcpvalue++;
      largelcpvalueptr->position = posoffset+idx;
      largelcpvalueptr->value = lcpvalue;
      lcpsubtab->smalllcpvalues[idx-bucketleft] = LCPOVERFLOW;
    }
  }
  lcpsubtab->countoutputlcpvalues += (bucketright - bucketleft + 1);
  gt_xfwrite(lcpsubtab->smalllcpvalues,
             sizeof (*lcpsubtab->smalllcpvalues),
             (size_t) (bucketright - bucketleft + 1),fplcptab);
  if (lcpsubtab->largelcpvalues.nextfreeLargelcpvalue > 0)
  {
    lcpsubtab->totalnumoflargelcpvalues
      += lcpsubtab->largelcpvalues.nextfreeLargelcpvalue;
    gt_xfwrite(lcpsubtab->largelcpvalues.spaceLargelcpvalue,
               sizeof (Largelcpvalue),
               (size_t)
               lcpsubtab->largelcpvalues.nextfreeLargelcpvalue,
               fpllvtab);
  }
}

#define NUMBEROFZEROS 1024

static Seqpos outmany0lcpvalues(Seqpos countoutputlcpvalues,Seqpos totallength,
                                FILE *outfplcptab)
{
  Seqpos i, countout, many;
  uint8_t outvalues[NUMBEROFZEROS] = {0};

  many = totallength + 1 - countoutputlcpvalues;
  countout = many/NUMBEROFZEROS;
  for (i=0; i<countout; i++)
  {
    gt_xfwrite(outvalues,sizeof (uint8_t),(size_t) NUMBEROFZEROS,outfplcptab);
  }
  gt_xfwrite(outvalues,sizeof (uint8_t),(size_t) many % NUMBEROFZEROS,
             outfplcptab);
  return many;
}

static void multioutlcpvalues(Lcpsubtab *lcpsubtab,
                              Seqpos totallength,
                              const Compressedtable *lcptab,
                              unsigned long bucketsize,
                              FILE *fplcptab,
                              FILE *fpllvtab)
{
  unsigned long buffersize = 512UL, sizeforsmalllcpvalues;
  unsigned long remaining, left, width;
  bool mallocsmalllcpvalues;

  if ((Seqpos) buffersize > totallength + 1)
  {
    buffersize = (unsigned long) (totallength+1);
  }
  lcpsubtab->numoflargelcpvalues = (Seqpos) buffersize;
  lcpsubtab->bucketoflcpvalues = NULL;
  lcpsubtab->completelcpvalues = lcptab;
  sizeforsmalllcpvalues = (unsigned long)
                          sizeof (*lcpsubtab->smalllcpvalues) * buffersize;
  lcpsubtab->smalllcpvalues
    = compressedtable_unusedmem(lcptab,
                                (size_t) sizeforsmalllcpvalues);
  if (lcpsubtab->smalllcpvalues == NULL)
  {
    lcpsubtab->smalllcpvalues = gt_malloc((size_t) sizeforsmalllcpvalues);
    mallocsmalllcpvalues = true;
  } else
  {
    mallocsmalllcpvalues = false;
  }
  remaining = bucketsize;
  left = 0;
  gt_assert(fplcptab != NULL && fpllvtab != NULL);
  while (remaining > 0)
  {
    width = MIN(remaining, buffersize);
    outlcpvalues(lcpsubtab,
                 left,
                 left + width - 1,
                 0,
                 fplcptab,
                 fpllvtab);
    remaining -= width;
    left += width;
  }
  if (mallocsmalllcpvalues)
  {
    gt_free(lcpsubtab->smalllcpvalues);
  }
  lcpsubtab->countoutputlcpvalues = (Seqpos) bucketsize;
}

void freeOutlcptab(Outlcpinfo **outlcpinfoptr)
{
  Outlcpinfo *outlcpinfo = *outlcpinfoptr;

  if (outlcpinfo->assideeffect)
  {
    FREESPACE(outlcpinfo->lcpsubtab.reservoir);
    outlcpinfo->lcpsubtab.sizereservoir = 0;
    if (outlcpinfo->tw != NULL)
    {
      freeTurningwheel(&outlcpinfo->tw);
    }
  }
  if (outlcpinfo->lcpsubtab.countoutputlcpvalues < outlcpinfo->totallength+1)
  {
    outlcpinfo->lcpsubtab.countoutputlcpvalues
      += outmany0lcpvalues(outlcpinfo->lcpsubtab.countoutputlcpvalues,
                           outlcpinfo->totallength,
                           outlcpinfo->outfplcptab);
  }
  gt_assert(outlcpinfo->lcpsubtab.countoutputlcpvalues ==
            outlcpinfo->totallength + 1);
  GT_FREEARRAY(&outlcpinfo->lcpsubtab.largelcpvalues,Largelcpvalue);
  gt_fa_fclose(outlcpinfo->outfplcptab);
  gt_fa_fclose(outlcpinfo->outfpllvtab);
  FREESPACE(*outlcpinfoptr);
}

Seqpos getnumoflargelcpvalues(const Outlcpinfo *outlcpinfo)
{
  return outlcpinfo->lcpsubtab.totalnumoflargelcpvalues;
}

Seqpos getmaxbranchdepth(const Outlcpinfo *outlcpinfo)
{
  return outlcpinfo->lcpsubtab.maxbranchdepth;
}

static void initBentsedgresources(Bentsedgresources *bsr,
                                  Suftab *suftab,
                                  DefinedSeqpos *longest,
                                  const Encodedsequence *encseq,
                                  Readmode readmode,
                                  Bcktab *bcktab,
                                  Codetype mincode,
                                  Codetype maxcode,
                                  Seqpos partwidth,
                                  unsigned int numofchars,
                                  unsigned int prefixlength,
                                  Outlcpinfo *outlcpinfo,
                                  const Sfxstrategy *sfxstrategy)
{
  unsigned long idx;

  bsr->readmode = readmode;
  bsr->totallength = getencseqtotallength(encseq);
  bsr->sfxstrategy = sfxstrategy;
  bsr->suftab = suftab;
  bsr->encseq = encseq;
  bsr->longest = longest;
  bsr->fwd = ISDIRREVERSE(bsr->readmode) ? false : true;
  bsr->complement = ISDIRCOMPLEMENT(bsr->readmode) ? true : false;
  for (idx = 0; idx < (unsigned long) UNITSIN2BITENC; idx++)
  {
    bsr->leftlcpdist[idx] = bsr->rightlcpdist[idx] = 0;
  }
  if (outlcpinfo != NULL)
  {
    bsr->lcpsubtab = &outlcpinfo->lcpsubtab;
    bsr->assideeffect = outlcpinfo->assideeffect;
  } else
  {
    bsr->lcpsubtab = NULL;
  }
  if (hasfastspecialrangeenumerator(encseq) && hasspecialranges(encseq))
  {
    bsr->esr1 = newEncodedsequencescanstate();
    bsr->esr2 = newEncodedsequencescanstate();
  } else
  {
    bsr->esr1 = bsr->esr2 = NULL;
  }
  if (bcktab != NULL)
  {
    determinemaxbucketsize(bcktab,
                           mincode,
                           maxcode,
                           partwidth,
                           numofchars,
                           false,
                           0); /* not necesarry as hashexceptions = false */
    /* bcktab_showlog2info(bcktab,verboseinfo); */
    if (outlcpinfo != NULL && outlcpinfo->assideeffect)
    {
      size_t sizespeciallcps, sizelcps;

      gt_assert(bsr->lcpsubtab != NULL);
      sizespeciallcps = sizeof (*bsr->lcpsubtab->smalllcpvalues) *
                        bcktab_specialsmaxbucketsize(bcktab);
      sizelcps = sizeof (*bsr->lcpsubtab->bucketoflcpvalues) *
                 bcktab_nonspecialsmaxbucketsize(bcktab);
      if (bsr->lcpsubtab->sizereservoir < MAX(sizelcps,sizespeciallcps))
      {
        bsr->lcpsubtab->sizereservoir = MAX(sizelcps,sizespeciallcps);
        bsr->lcpsubtab->reservoir = gt_realloc(bsr->lcpsubtab->reservoir,
                                               bsr->lcpsubtab->sizereservoir);
        /* point to the same area, since this is not used simultaneously */
        /* be careful for the parallel version */
        bsr->lcpsubtab->smalllcpvalues = (uint8_t *) bsr->lcpsubtab->reservoir;
        bsr->lcpsubtab->bucketoflcpvalues
          = (Seqpos *) bsr->lcpsubtab->reservoir;
      }
    }
  }
  GT_INITARRAY(&bsr->mkvauxstack,MKVstack);
  if (sfxstrategy->cmpcharbychar)
  {
    bsr->countingsortinfo = NULL;
    bsr->medianinfospace = NULL;
  } else
  {
    ALLOCASSIGNSPACE(bsr->countingsortinfo,NULL,Countingsortinfo,
                     sfxstrategy->maxcountingsort);
    if (sfxstrategy->maxwidthrealmedian >= MINMEDIANOF9WIDTH)
    {
      ALLOCASSIGNSPACE(bsr->medianinfospace,NULL,Medianinfo,
                       sfxstrategy->maxwidthrealmedian);
    } else
    {
      bsr->medianinfospace = NULL;
    }
  }
  if (bcktab != NULL && sfxstrategy->ssortmaxdepth.defined)
  {
    bsr->rmnsufinfo = newRmnsufinfo(suftab->sortspace - suftab->offset,
                                    -1,
                                    bsr->encseq,
                                    bcktab,
                                    maxcode,
                                    numofchars,
                                    prefixlength,
                                    readmode,
                                    partwidth,
                                    false,
                                    true);
    gt_assert(bsr->rmnsufinfo != NULL);
  } else
  {
    bsr->rmnsufinfo = NULL;
  }
  if (sfxstrategy->ssortmaxdepth.defined)
  {
    bsr->blindtrie = NULL;
  } else
  {
    bsr->blindtrie = blindtrie_new(sfxstrategy->maxbltriesort,
                                   encseq,
                                   sfxstrategy->cmpcharbychar,
                                   bsr->esr1,
                                   bsr->esr2,
                                   readmode);
  }
  bsr->voiddcov = NULL;
  bsr->dc_processunsortedrange = NULL;
  if (bsr->sfxstrategy->ssortmaxdepth.defined ||
      bsr->sfxstrategy->differencecover > 0)
  {
    bsr->equalwithprevious = gt_malloc(sizeof(*bsr->equalwithprevious) *
                                       bsr->sfxstrategy->maxinsertionsort);
    for (idx=0; idx < bsr->sfxstrategy->maxinsertionsort; idx++)
    {
      bsr->equalwithprevious[idx] = false;
    }
  } else
  {
    bsr->equalwithprevious = NULL;
  }
  bsr->countinsertionsort = 0;
  bsr->countqsort = 0;
  bsr->countcountingsort = 0;
  bsr->countbltriesort = 0;
}

static void wrapBentsedgresources(Bentsedgresources *bsr,
                                  Seqpos partwidth,
                                  Lcpsubtab *lcpsubtab,
                                  FILE *outfplcptab,
                                  FILE *outfpllvtab,
                                  Verboseinfo *verboseinfo)
{
  FREESPACE(bsr->countingsortinfo);
  FREESPACE(bsr->medianinfospace);
  if (bsr->blindtrie != NULL)
  {
    blindtrie_delete(&bsr->blindtrie);
  }
  if (bsr->rmnsufinfo != NULL)
  {
    Compressedtable *lcptab;

    lcptab = rmnsufinfo_wrap(&bsr->longest->valueseqpos,
                             &bsr->rmnsufinfo,
                             bsr->lcpsubtab == NULL ? false : true);
    bsr->longest->defined = true;
    if (lcptab != NULL)
    {
      multioutlcpvalues(lcpsubtab,bsr->totallength,
                        lcptab,(unsigned long) partwidth,
                        outfplcptab,outfpllvtab);
      compressedtable_free(lcptab,true);
    }
  }
  if (bsr->esr1 != NULL)
  {
    freeEncodedsequencescanstate(&bsr->esr1);
  }
  if (bsr->esr2 != NULL)
  {
    freeEncodedsequencescanstate(&bsr->esr2);
  }
  gt_free(bsr->equalwithprevious);
  GT_FREEARRAY(&bsr->mkvauxstack,MKVstack);
  showverbose(verboseinfo,"countinsertionsort=%lu",bsr->countinsertionsort);
  showverbose(verboseinfo,"countbltriesort=%lu",bsr->countbltriesort);
  showverbose(verboseinfo,"countcountingsort=%lu",bsr->countcountingsort);
  showverbose(verboseinfo,"countqsort=%lu",bsr->countqsort);
}

void qsufsort(Seqpos *sortspace,
              int mmapfiledesc,
              Seqpos *longest,
              const Encodedsequence *encseq,
              Readmode readmode,
              GT_UNUSED Codetype mincode,
              Codetype maxcode,
              Seqpos partwidth,
              Bcktab *bcktab,
              unsigned int numofchars,
              unsigned int prefixlength,
              bool hashexceptions,
              bool absoluteinversesuftab,
              Outlcpinfo *outlcpinfo)
{
  Rmnsufinfo *rmnsufinfo;
  Compressedtable *lcptab;

  gt_assert(mincode == 0);
  rmnsufinfo = newRmnsufinfo(sortspace,
                             mmapfiledesc,
                             encseq,
                             bcktab,
                             maxcode,
                             numofchars,
                             prefixlength,
                             readmode,
                             partwidth,
                             hashexceptions,
                             absoluteinversesuftab);
  bcktab2firstlevelintervals(rmnsufinfo);
  lcptab = rmnsufinfo_wrap(longest,&rmnsufinfo,
                           outlcpinfo == NULL ? false : true);
  if (lcptab != NULL)
  {
    gt_assert(outlcpinfo != NULL);
    gt_assert(outlcpinfo->outfplcptab != NULL);
    gt_assert(outlcpinfo->outfpllvtab != NULL);

    multioutlcpvalues(&outlcpinfo->lcpsubtab,getencseqtotallength(encseq),
                      lcptab,(unsigned long) partwidth,
                      outlcpinfo->outfplcptab,outlcpinfo->outfpllvtab);
    compressedtable_free(lcptab,true);
  }
}

void sortallbuckets(Suftab *suftab,
                    GtBucketspec2 *bucketspec2,
                    const Encodedsequence *encseq,
                    Readmode readmode,
                    Codetype mincode,
                    Codetype maxcode,
                    Seqpos partwidth,
                    Bcktab *bcktab,
                    unsigned int numofchars,
                    unsigned int prefixlength,
                    Outlcpinfo *outlcpinfo,
                    const Sfxstrategy *sfxstrategy,
                    unsigned long long *bucketiterstep,
                    Verboseinfo *verboseinfo)
{
  Codetype code;
  unsigned int rightchar = (unsigned int) (mincode % numofchars),
               minprefixindex;
  Bucketspecification bucketspec;
  Seqpos lcpvalue;
  Suffixwithcode firstsuffixofbucket;
  Bentsedgresources bsr;
  Seqpos *suftabptr = suftab->sortspace - suftab->offset;

  initBentsedgresources(&bsr,
                        suftab,
                        &suftab->longest,
                        encseq,
                        readmode,
                        bcktab,
                        mincode,
                        maxcode,
                        partwidth,
                        numofchars,
                        prefixlength,
                        outlcpinfo,
                        sfxstrategy);
  for (code = mincode; code <= maxcode; code++)
  {
    if (bucketspec2 != NULL)
    {
      if (gt_hardworkbeforecopysort(bucketspec2,code))
      {
        rightchar = (unsigned int) (code % numofchars);
      } else
      {
        continue;
      }
    }
    (*bucketiterstep)++;
    rightchar = calcbucketboundsparts(&bucketspec,
                                      bcktab,
                                      code,
                                      maxcode,
                                      partwidth,
                                      rightchar,
                                      numofchars);
    if (outlcpinfo != NULL && outlcpinfo->assideeffect)
    {
      bsr.lcpsubtab->numoflargelcpvalues = 0;
      if (code > 0)
      {
        (void) nextTurningwheel(outlcpinfo->tw);
        if (outlcpinfo->previousbucketwasempty)
        {
          outlcpinfo->minchanged = MIN(outlcpinfo->minchanged,
                                       minchangedTurningwheel(outlcpinfo->tw));
        } else
        {
          outlcpinfo->minchanged = minchangedTurningwheel(outlcpinfo->tw);
        }
      }
    }
    if (bucketspec.nonspecialsinbucket > 0)
    {
      if (bucketspec.nonspecialsinbucket > 1UL)
      {
        if (outlcpinfo != NULL && outlcpinfo->assideeffect)
        {
          gt_assert(bsr.lcpsubtab != NULL);
          bsr.lcpsubtab->suftabbase = suftabptr + bucketspec.left;
        }
        bentleysedgewick(&bsr,
                         suftabptr + bucketspec.left,
                         suftabptr + bucketspec.left +
                                     bucketspec.nonspecialsinbucket - 1,
                         (Seqpos) prefixlength);
      }
      if (outlcpinfo != NULL && outlcpinfo->assideeffect)
      {
        if (outlcpinfo->previoussuffix.defined)
        {
          /* compute lcpvalue of first element of bucket with
             last element of previous bucket */
          firstsuffixofbucket.code = code;
          firstsuffixofbucket.prefixindex = prefixlength;
#ifdef SKDEBUG
          firstsuffixofbucket.startpos = suftabptr[bucketspec.left];
          /*
          consistencyofsuffix(__LINE__,
                              encseq,readmode,bcktab,numofchars,
                              &firstsuffixofbucket);
          */
#endif
          lcpvalue = computelocallcpvalue(&outlcpinfo->previoussuffix,
                                          &firstsuffixofbucket,
                                          outlcpinfo->minchanged);
        } else
        {
          /* first part first code */
          lcpvalue = 0;
        }
        gt_assert(bsr.lcpsubtab != NULL);
#ifdef SKDEBUG
        baseptr = bucketspec.left;
#endif
        updatelcpvalue(&bsr,0,lcpvalue);
        /* all other lcp-values are computed and they can be output */
        outlcpvalues(&outlcpinfo->lcpsubtab,
                     0,
                     bucketspec.nonspecialsinbucket-1,
                     bucketspec.left,
                     outlcpinfo->outfplcptab,
                     outlcpinfo->outfpllvtab);
        /* previoussuffix becomes last nonspecial element in current bucket */
        outlcpinfo->previoussuffix.code = code;
        outlcpinfo->previoussuffix.prefixindex = prefixlength;
#ifdef SKDEBUG
        outlcpinfo->previoussuffix.startpos
          = suftabptr[bucketspec.left + bucketspec.nonspecialsinbucket - 1];
        /*
        consistencyofsuffix(__LINE__,
                            encseq,readmode,bcktab,numofchars,
                            &outlcpinfo->previoussuffix);
        */
#endif
      }
    }
    if (outlcpinfo != NULL && outlcpinfo->assideeffect)
    {
      if (bucketspec.specialsinbucket > 0)
      {
        minprefixindex = bucketends(outlcpinfo,
                                    &outlcpinfo->previoussuffix,
                                    /* first special element in bucket */
                                    suftabptr[bucketspec.left +
                                              bucketspec.nonspecialsinbucket],
                                    outlcpinfo->minchanged,
                                    bucketspec.specialsinbucket,
                                    code,
                                    bcktab);
        /* there is at least one special element: this is the last element
           in the bucket, and thus the previoussuffix for the next round */
        outlcpinfo->previoussuffix.defined = true;
        outlcpinfo->previoussuffix.code = code;
        outlcpinfo->previoussuffix.prefixindex = minprefixindex;
#ifdef SKDEBUG
        outlcpinfo->previoussuffix.startpos
          = suftabptr[bucketspec.left + bucketspec.nonspecialsinbucket +
                                        bucketspec.specialsinbucket - 1];
        /*
         consistencyofsuffix(__LINE__,
                             encseq,readmode,bcktab,numofchars,
                             &outlcpinfo->previoussuffix);
        */
#endif
      } else
      {
        if (bucketspec.nonspecialsinbucket > 0)
        {
          /* if there is at least one element in the bucket, then the last
             one becomes the next previous suffix */
          outlcpinfo->previoussuffix.defined = true;
          outlcpinfo->previoussuffix.code = code;
          outlcpinfo->previoussuffix.prefixindex = prefixlength;
#ifdef SKDEBUG
          outlcpinfo->previoussuffix.startpos
            = suftabptr[bucketspec.left + bucketspec.nonspecialsinbucket - 1];
          /*
          consistencyofsuffix(__LINE__,
                              encseq,readmode,bcktab,numofchars,
                              &outlcpinfo->previoussuffix);
          */
#endif
        }
      }
      if (bucketspec.nonspecialsinbucket + bucketspec.specialsinbucket == 0)
      {
        outlcpinfo->previousbucketwasempty = true;
      } else
      {
        outlcpinfo->previousbucketwasempty = false;
      }
    }
  }
  wrapBentsedgresources(&bsr,
                        partwidth,
                        outlcpinfo == NULL ? NULL : &outlcpinfo->lcpsubtab,
                        outlcpinfo == NULL ? NULL : outlcpinfo->outfplcptab,
                        outlcpinfo == NULL ? NULL : outlcpinfo->outfpllvtab,
                        verboseinfo);
}

void sortbucketofsuffixes(Seqpos *suffixestobesorted,
                          GtBucketspec2 *bucketspec2,
                          unsigned long numberofsuffixes,
                          const Encodedsequence *encseq,
                          Readmode readmode,
                          Codetype mincode,
                          Codetype maxcode,
                          const Bcktab *bcktab,
                          unsigned int numofchars,
                          unsigned int prefixlength,
                          const Sfxstrategy *sfxstrategy,
                          void *voiddcov,
                          void (*dc_processunsortedrange)(void *,Seqpos *,
                                                          Seqpos *,Seqpos),
                          Verboseinfo *verboseinfo)
{
  Bentsedgresources bsr;
  Bucketspecification bucketspec;
  unsigned int rightchar =  (unsigned int) (mincode % numofchars);
  Codetype code;

  initBentsedgresources(&bsr,
                        NULL,
                        NULL,
                        encseq,
                        readmode,
                        NULL, /* bcktab unused */
                        0,    /* mincode unused */
                        0,    /* maxcode unused */
                        0,    /* partwidth unused */
                        numofchars,
                        prefixlength,
                        NULL,  /* outlcpinfo unused */
                        sfxstrategy);
  bsr.voiddcov = voiddcov;
  bsr.dc_processunsortedrange = dc_processunsortedrange;
  for (code = mincode; code <= maxcode; code++)
  {
    if (bucketspec2 != NULL)
    {
      if (gt_hardworkbeforecopysort(bucketspec2,code))
      {
        rightchar = (unsigned int) (code % numofchars);
      } else
      {
        continue;
      }
    }
    rightchar = calcbucketboundsparts(&bucketspec,
                                      bcktab,
                                      code,
                                      maxcode,
                                      (Seqpos) numberofsuffixes,
                                      rightchar,
                                      numofchars);
    if (bucketspec.nonspecialsinbucket > 1UL)
    {
      bentleysedgewick(&bsr,
                       suffixestobesorted + bucketspec.left,
                       suffixestobesorted + bucketspec.left +
                                      bucketspec.nonspecialsinbucket - 1,
                       (Seqpos) prefixlength);
    }
  }
  wrapBentsedgresources(&bsr,
                        0, /* partwidth value unused because lcptab == NULL */
                        NULL,
                        NULL,
                        NULL,
                        verboseinfo);
}
