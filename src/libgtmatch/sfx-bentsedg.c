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
#include <assert.h>
#include "libgtcore/arraydef.h"
#include "libgtcore/chardef.h"
#include "libgtcore/minmax.h"
#include "divmodmul.h"
#include "sfx-codespec.h"
#include "intcode-def.h"
#include "sfx-lcpsub.h"
#include "spacedef.h"
#include "encseq-def.h"
#include "turnwheels.h"
#include "sfx-outlcp.h"

#include "sfx-cmpsuf.pr"

#define COMPAREOFFSET   (UCHAR_MAX + 1)
#define UNIQUEINT(P)    ((Seqpos) ((P) + COMPAREOFFSET))
#define ACCESSCHAR(POS) getencodedchar(encseq,POS,readmode) /* XXX */
#define ISNOTEND(POS)   ((POS) < totallength && ISNOTSPECIAL(ACCESSCHAR(POS)))

#define DECLARETMPC Uchar tmpsvar, tmptvar
#define DEREF(VAR,A,S)\
        (((A) < totallength && ISNOTSPECIAL(VAR = ACCESSCHAR(S))) ?\
        ((Seqpos) VAR) : UNIQUEINT(S))

#define PTR2INT(VAR,I) DEREF(VAR,cptr = *(I)+depth,cptr)

#define WITHLCPNEW

#ifdef WITHLCPNEW
#define LCPINDEX(I)        (Seqpos) ((I) - lcpsubtab->suftabbase)
#define SETLCP(I,V)        lcpsubtab->spaceSeqpos[I] = V
#define EVALLCPLEN(LL,T)   LL = (Seqpos) (tptr - (T))
#else
#define SETLCP(I,V)        /* Nothing */
#define EVALLCPLEN(LL,T)   /* Nothing */
#endif

#define UNDEFLCP(TLEN) ((TLEN)+1)

#define STRINGCOMPARE(S,T,OFFSET,LL)\
        for (sptr = (S)+(OFFSET), tptr = (T)+(OFFSET); /* Nothing */;\
             sptr++, tptr++)\
        {\
          ccs = DEREF(tmpsvar,sptr,sptr);\
          cct = DEREF(tmptvar,tptr,tptr);\
          if (ccs != cct)\
          {\
            EVALLCPLEN(LL,T);\
            break;\
          }\
        }

#define SWAP(A,B)\
        if ((A) != (B))\
        {\
          temp = *(A);\
          *(A) = *(B);\
          *(B) = temp;\
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

#define SMALLSIZE (Seqpos) 6

#define SUBSORT(WIDTH,BORDER,LEFT,RIGHT,DEPTH)\
        if ((WIDTH) <= (BORDER))\
        {\
          insertionsort(encseq,lcpsubtab,readmode,totallength,\
                        DEPTH,LEFT,RIGHT);\
        } else\
        {\
          PUSHMKVSTACK(LEFT,RIGHT,DEPTH);\
        }

#define PUSHMKVSTACK(L,R,D)\
        CHECKARRAYSPACE(mkvauxstack,MKVstack,1024);\
        mkvauxstack->spaceMKVstack[mkvauxstack->nextfreeMKVstack].left = L;\
        mkvauxstack->spaceMKVstack[mkvauxstack->nextfreeMKVstack].right = R;\
        mkvauxstack->spaceMKVstack[mkvauxstack->nextfreeMKVstack++].depth = D

#define POPMKVstack(L,R,D)\
        L = mkvauxstack->spaceMKVstack[--mkvauxstack->nextfreeMKVstack].left;\
        R = mkvauxstack->spaceMKVstack[mkvauxstack->nextfreeMKVstack].right;\
        D = mkvauxstack->spaceMKVstack[mkvauxstack->nextfreeMKVstack].depth;\
        width = (Seqpos) ((R) - (L) + 1)

typedef Seqpos Suffixptr;

 struct Lcpsubtab
{
  Seqpos *spaceSeqpos;
  unsigned long nextfreeSeqpos, allocatedSeqpos;
  const Seqpos *suftabbase;
  Turningwheel *tw;
};

static Suffixptr *medianof3(const Encodedsequence *encseq,
                            Readmode readmode,
                            Seqpos totallength,
                            Seqpos depth,
                            Suffixptr *a,
                            Suffixptr *b,
                            Suffixptr *c)
{
  Suffixptr cptr;
  Seqpos vala, valb, valc;
  Uchar tmpsvar, tmptvar;

  vala = PTR2INT(tmpsvar,a);
  valb = PTR2INT(tmptvar,b);
  if (vala == valb)
  {
    return a;
  }
  if ((valc = PTR2INT(tmpsvar,c)) == vala || valc == valb)
  {
    return c;
  }
  return vala < valb ?
        (valb < valc ? b : (vala < valc ? c : a))
      : (valb > valc ? b : (vala < valc ? a : c));
}

static void insertionsort(const Encodedsequence *encseq,
                          Lcpsubtab *lcpsubtab,
                          Readmode readmode,
                          Seqpos totallength,
                          Seqpos depth,
                          Suffixptr *leftptr,
                          Suffixptr *rightptr)
{
  Suffixptr sptr, tptr, *pi, *pj, temp;
  Seqpos ccs, cct;
  Uchar tmpsvar, tmptvar;
#ifdef WITHLCPNEW
  Seqpos lcpindex, lcplen;
#endif

  for (pi = leftptr + 1; pi <= rightptr; pi++)
  {
    for (pj = pi; pj > leftptr; pj--)
    {
      STRINGCOMPARE(*(pj-1),*pj,depth,lcplen);
#ifdef WITHLCPNEW
      lcpindex = LCPINDEX(pj);
      if (ccs > cct && pj < pi)
      {
        SETLCP(lcpindex+1,lcpsubtab->spaceSeqpos[lcpindex]);
      }
      SETLCP(lcpindex,lcplen);
#endif
      if (ccs < cct)
      {
        break;
      }
      SWAP(pj,pj-1);
    }
  }
}

typedef struct
{
  Suffixptr *left,
            *right;
  Seqpos depth;
} MKVstack;

DECLAREARRAYSTRUCT(MKVstack);

static void bentleysedgewick(const Encodedsequence *encseq,
                             Readmode readmode,
                             Seqpos totallength,
                             ArrayMKVstack *mkvauxstack,
                             Suffixptr *l,
                             Suffixptr *r,
                             Seqpos d,
                             Lcpsubtab *lcpsubtab)
{
  Suffixptr *left, *right, *leftplusw;
  Seqpos w, val, partval, depth, offset, doubleoffset, width;
  Suffixptr *pa, *pb, *pc, *pd, *pl, *pm, *pr, *aptr, *bptr, cptr, temp;
  Uchar tmpsvar;

  width = (Seqpos) (r - l + 1);
  if (width <= SMALLSIZE)
  {
    insertionsort(encseq,lcpsubtab,readmode,totallength,d,l,r);
    return;
  }
  left = l;
  right = r;
  depth = d;
  mkvauxstack->nextfreeMKVstack = 0;

  for (;;)
  {
    pl = left;
    pm = left + DIV2(width);
    pr = right;
    if (width > (Seqpos) 30)
    { /* On big arrays, pseudomedian of 9 */
      offset = DIV8(width);
      doubleoffset = MULT2(offset);
      pl = medianof3(encseq,readmode,totallength,depth,
                     pl,pl+offset,pl+doubleoffset);
      pm = medianof3(encseq,readmode,totallength,depth,
                     pm-offset,pm,pm+offset);
      pr = medianof3(encseq,readmode,totallength,depth,
                     pr-doubleoffset,pr-offset,pr);
    }
    pm = medianof3(encseq,readmode,totallength,depth,pl,pm,pr);
    SWAP(left, pm);
    partval = PTR2INT(tmpsvar,left);
    pa = pb = left + 1;
    pc = pd = right;
    for (;;)
    {
      while (pb <= pc)
      {
        if ((val = PTR2INT(tmpsvar,pb)) > partval)
        {
          break;
        }
        if (val == partval)
        {
          SWAP(pa, pb);
          pa++;
        }
        pb++;
      }
      while (pb <= pc)
      {
        if ((val = PTR2INT(tmpsvar,pc)) < partval)
        {
          break;
        }
        if (val == partval)
        {
          SWAP(pc, pd);
          pd--;
        }
        pc--;
      }
      if (pb > pc)
      {
        break;
      }
      SWAP(pb, pc);
      pb++;
      pc--;
    }

    assert(pa >= left);
    assert(pb >= pa);
    w = MIN((Seqpos) (pa-left),(Seqpos) (pb-pa));
    VECSWAP(left,  pb-w, w);
    pr = right + 1;
    assert(pd >= pc);
    assert(pr > pd);
    w = MIN((Seqpos) (pd-pc), (Seqpos) (pr-pd-1));
    VECSWAP(pb, pr-w, w);

    assert(pd >= pc);
    if ((w = (Seqpos) (pd-pc)) > 0)
    {
      SETLCP(LCPINDEX(right-w+1),depth);
      SUBSORT(w,SMALLSIZE,right-w+1,right,depth);
    }
    assert(pb >= pa);
    w = (Seqpos) (pb-pa);
    leftplusw = left + w;
    cptr = *leftplusw + depth;
    if (ISNOTEND(cptr))
    {
      right -= (pd-pb);
      width = (Seqpos) (right-leftplusw);
      SUBSORT(width,SMALLSIZE,leftplusw,right-1,depth+1);
    }
    if (w > 0)
    {
      SETLCP(LCPINDEX(leftplusw),depth);
      SUBSORT(w,SMALLSIZE,left,leftplusw-1,depth);
    }
    if (mkvauxstack->nextfreeMKVstack == 0)
    {
      break;
    }
    POPMKVstack(left,right,depth);
  }
}

typedef struct
{
  Seqpos left;
  unsigned long nonspecialsinbucket,
                specialsinbucket;
} Bucketboundaries;

static unsigned int calcbucketboundaries(Bucketboundaries *bbound,
                                         const Seqpos *leftborder,
                                         const Seqpos *countspecialcodes,
                                         Codetype code,
                                         Codetype maxcode,
                                         Seqpos totalwidth,
                                         unsigned int rightchar,
                                         unsigned int numofchars)
{
  bbound->left = leftborder[code];
  if (code == maxcode)
  {
    assert(totalwidth >= bbound->left);
    bbound->nonspecialsinbucket = (unsigned long) (totalwidth - bbound->left);
  } else
  {
    if (leftborder[code+1] > 0)
    {
      bbound->nonspecialsinbucket
        = (unsigned long) (leftborder[code+1] - bbound->left);
    } else
    {
      bbound->nonspecialsinbucket = 0;
    }
  }
  assert(rightchar == code % numofchars);
  if (rightchar == numofchars - 1)
  {
    bbound->specialsinbucket
      = (unsigned long)
        countspecialcodes[FROMCODE2SPECIALCODE(code,numofchars)];
    if (bbound->nonspecialsinbucket >= bbound->specialsinbucket)
    {
      bbound->nonspecialsinbucket -= bbound->specialsinbucket;
    } else
    {
      bbound->nonspecialsinbucket = 0;
    }
    rightchar = 0;
  } else
  {
    bbound->specialsinbucket = 0;
    rightchar++;
  }
  return rightchar;
}

static unsigned long determinemaxbucketsize(const Seqpos *leftborder,
                                            const Seqpos *countspecialcodes,
                                            const Codetype mincode,
                                            const Codetype maxcode,
                                            Seqpos totalwidth,
                                            unsigned int numofchars)
{
  unsigned long maxbucketsize = 1UL;
  unsigned int rightchar = mincode % numofchars;
  Bucketboundaries bbound;
  Codetype code;

  for (code = mincode; code <= maxcode; code++)
  {
    rightchar = calcbucketboundaries(&bbound,
                                     leftborder,
                                     countspecialcodes,
                                     code,
                                     maxcode,
                                     totalwidth,
                                     rightchar,
                                     numofchars);
    if (bbound.nonspecialsinbucket > maxbucketsize)
    {
      maxbucketsize = bbound.nonspecialsinbucket;
    }
  }
  return maxbucketsize;
}

void freelcpsubtab(Lcpsubtab **lcpsubtab)
{
  FREEARRAY(*lcpsubtab,Seqpos);
  freeTurningwheel(&(*lcpsubtab)->tw);
  FREESPACE(*lcpsubtab);
  return;
}

Lcpsubtab *newlcpsubtab(unsigned int prefixlength,unsigned int numofchars)
{
  Lcpsubtab *lcpsubtab;

  ALLOCASSIGNSPACE(lcpsubtab,NULL,Lcpsubtab,1);
  INITARRAY(lcpsubtab,Seqpos);
  lcpsubtab->tw = newTurningwheel(prefixlength,numofchars);
  return lcpsubtab;
}

static void setlcpundef(Lcpsubtab *lcpsubtab,unsigned long maxbucketsize,
                        Seqpos totallength)
{
  unsigned long i;

  for (i=0; i<maxbucketsize; i++)
  {
    lcpsubtab->spaceSeqpos[i] = UNDEFLCP(totallength);
  }
}

static void multilcpvalue(Lcpsubtab *lcpsubtab,
                          Outlcpinfo *outlcpinfo,
                          unsigned long bucketsize,
                          Seqpos posoffset)
{
  unsigned long i;

  for (i=0; i<bucketsize; i++)
  {
    outlcpvalue(lcpsubtab->spaceSeqpos[i],posoffset+i,outlcpinfo);
  }
}

static Seqpos computelocallcpvalue(const const Encodedsequence *encseq,
                                   Readmode readmode,
                                   Seqpos suffixpos1,
                                   Seqpos suffixpos2,
                                   Encodedsequencescanstate *esr1,
                                   Encodedsequencescanstate *esr2)
{
  Seqpos lcpvalue;
  int cmp;

  cmp = comparetwosuffixes(encseq,
                           readmode,
                           &lcpvalue,
                           false,
                           false,
                           0,
                           suffixpos1,
                           suffixpos2,
                           esr1,
                           esr2);
  if (cmp > 0)
  {
    fprintf(stderr,"cmp " FormatSeqpos
            " " FormatSeqpos " = %d, lcpval=" FormatSeqpos "\n",
            PRINTSeqposcast(suffixpos1),
            PRINTSeqposcast(suffixpos2),
            cmp,
            PRINTSeqposcast(lcpvalue));
  }
  return lcpvalue;
}

static void bucketends(const Encodedsequence *encseq,
                       Readmode readmode,
                       Encodedsequencescanstate *esr1,
                       Encodedsequencescanstate *esr2,
                       Outlcpinfo *outlcpinfo,
                       Seqpos previoussuffix,
                       const Seqpos *specialsection,
                       unsigned long specialsinbucket)
{
  unsigned long i;
  Seqpos lcpvalue;

  for (i=0; i<specialsinbucket; i++)
  {
    lcpvalue = computelocallcpvalue(encseq,
                                    readmode,
                                    i == 0 ? previoussuffix
                                           : specialsection[i-1],
                                    specialsection[i],
                                    esr1,
                                    esr2);
    assert(lcpvalue < (Seqpos) UCHAR_MAX);
    assert(lcpvalue > 0);
    outlcpvalue(lcpvalue,0,outlcpinfo);
  }
}

void sortallbuckets(Seqpos *suftabptr,
                    const Encodedsequence *encseq,
                    Readmode readmode,
                    const Seqpos *leftborder,
                    const Seqpos *countspecialcodes,
                    unsigned int numofchars,
                    unsigned int prefixlength,
                    Codetype mincode,
                    Codetype maxcode,
                    Seqpos totalwidth,
                    Seqpos previoussuffix,
                    Lcpsubtab *lcpsubtab,
                    Outlcpinfo *outlcpinfo)
{
  Codetype code;
  unsigned int rightchar = mincode % numofchars;
  Seqpos totallength = getencseqtotallength(encseq);
  ArrayMKVstack mkvauxstack;
  Bucketboundaries bbound;
  unsigned long maxbucketsize;
  Seqpos lcpvalue;
  Encodedsequencescanstate *esr1, *esr2;

  esr1 = newEncodedsequencescanstate();
  esr2 = newEncodedsequencescanstate();
  maxbucketsize = determinemaxbucketsize(leftborder,
                                         countspecialcodes,
                                         mincode,
                                         maxcode,
                                         totalwidth,
                                         numofchars);
  if (maxbucketsize > lcpsubtab->allocatedSeqpos)
  {
    lcpsubtab->allocatedSeqpos = maxbucketsize;
    ALLOCASSIGNSPACE(lcpsubtab->spaceSeqpos,lcpsubtab->spaceSeqpos,Seqpos,
                     lcpsubtab->allocatedSeqpos);
  }
  setlcpundef(lcpsubtab,maxbucketsize,totallength);
  INITARRAY(&mkvauxstack,MKVstack);
  for (code = mincode; code <= maxcode; code++)
  {
    rightchar = calcbucketboundaries(&bbound,
                                     leftborder,
                                     countspecialcodes,
                                     code,
                                     maxcode,
                                     totalwidth,
                                     rightchar,
                                     numofchars);
    if (code > 0)
    {
      (void) nextTurningwheel(lcpsubtab->tw);
    }
    if (bbound.nonspecialsinbucket > 0)
    {
      if (bbound.nonspecialsinbucket > 1UL)
      {
        lcpsubtab->suftabbase = suftabptr + bbound.left;
        bentleysedgewick(encseq,
                         readmode,
                         totallength,
                         &mkvauxstack,
                         suftabptr + bbound.left,
                         suftabptr + bbound.left +
                                     bbound.nonspecialsinbucket - 1,
                         (Seqpos) prefixlength,
                         lcpsubtab);
      }
      if (outlcpinfo != NULL)
      {
        if (code == 0)
        {
          lcpvalue = 0;
        } else
        {
          lcpvalue = computelocallcpvalue(encseq,
                                          readmode,
                                          previoussuffix,
                                          suftabptr[bbound.left],
                                          esr1,
                                          esr2);
        }
        SETLCP(0,lcpvalue);
        multilcpvalue(lcpsubtab,
                      outlcpinfo,
                      bbound.nonspecialsinbucket,
                      bbound.left);
        previoussuffix = suftabptr[bbound.left+bbound.nonspecialsinbucket-1];
      }
    }
    if (outlcpinfo != NULL)
    {
      if (bbound.specialsinbucket > 0)
      {
        bucketends(encseq,
                   readmode,
                   esr1,
                   esr2,
                   outlcpinfo,
                   previoussuffix,
                   suftabptr + bbound.left + bbound.nonspecialsinbucket,
                   bbound.specialsinbucket);
      }
      if (bbound.nonspecialsinbucket + bbound.specialsinbucket > 0)
      {
        previoussuffix = suftabptr[bbound.left + bbound.nonspecialsinbucket +
                                   bbound.specialsinbucket - 1];
      }
    }
  }
  freeEncodedsequencescanstate(&esr1);
  freeEncodedsequencescanstate(&esr2);
  FREEARRAY(&mkvauxstack,MKVstack);
}
