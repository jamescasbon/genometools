/*
  Copyright (c) 2008 Gordon Gremme <gremme@zbh.uni-hamburg.de>
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
#include "core/mathsupport.h"
#include "extended/shredder.h"

struct GtShredder {
  GtBioseq *bioseq;
  unsigned long minlength,
                maxlength,
                overlap,
                seqnum,
                pos;
  double sample_probability;
};

GtShredder* gt_shredder_new(GtBioseq *bioseq, unsigned long minlength,
                                       unsigned long maxlength)
{
  GtShredder *shredder = gt_calloc(1, sizeof *shredder);
  gt_assert(bioseq && minlength && minlength <= maxlength);
  shredder->bioseq = bioseq;
  shredder->minlength = minlength;
  shredder->maxlength = maxlength;
  shredder->sample_probability = 1.0;
  return shredder;
}

void gt_shredder_delete(GtShredder *shredder)
{
  if (!shredder) return;
  gt_free(shredder);
}

void gt_shredder_set_overlap(GtShredder *shredder, unsigned long overlap)
{
  gt_assert(shredder);
  shredder->overlap = overlap;
}

void gt_shredder_set_sample_probability(GtShredder *shredder,
                                        double probability)
{
  gt_assert(shredder);
  gt_assert(probability >= 0.0 && probability <= 1.0);
  shredder->sample_probability = probability;
}

static const char* generate_fragment(GtShredder *shredder,
                                     unsigned long *fragment_length,
                                     GtStr *desc)
{
  gt_assert(shredder && fragment_length);
  if (shredder->seqnum < gt_bioseq_number_of_sequences(shredder->bioseq)) {
    unsigned long seqlen, fraglen;
    const char *frag;
    seqlen = gt_bioseq_get_sequence_length(shredder->bioseq, shredder->seqnum);
    fraglen = (shredder->maxlength == shredder->minlength
               ? 0 : gt_rand_max(shredder->maxlength - shredder->minlength))
              + shredder->minlength;
    gt_assert(fraglen >= shredder->minlength);
    frag = gt_bioseq_get_sequence(shredder->bioseq, shredder->seqnum)
           + shredder->pos;
    if (shredder->pos + fraglen > seqlen)
      fraglen = seqlen - shredder->pos;
    *fragment_length = fraglen;
    gt_str_reset(desc);
    gt_str_append_cstr(desc, gt_bioseq_get_description(shredder->bioseq,
                                                 shredder->seqnum));
    gt_assert(shredder->pos + fraglen <= seqlen);
    if (shredder->pos + fraglen == seqlen) { /* last fragment */
      shredder->seqnum++;
      shredder->pos = 0;
    }
    else {
      if (fraglen > shredder->overlap)
        shredder->pos += fraglen - shredder->overlap;
      else
        shredder->pos++; /* go at least one base further each step */
    }
    return frag;
  }
  return NULL;
}

const char* gt_shredder_shred(GtShredder *shredder,
                              unsigned long *fragment_length,
                              GtStr *desc)
{
  const char *frag;
  gt_assert(shredder && fragment_length);
  while ((frag = generate_fragment(shredder, fragment_length, desc))) {
    if (shredder->sample_probability == 1.0 ||
        gt_rand_0_to_1() <= shredder->sample_probability) {
      return frag;
    }
  }
  return NULL;
}
