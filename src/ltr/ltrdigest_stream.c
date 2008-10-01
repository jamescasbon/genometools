/*
  Copyright (c) 2008 Sascha Steinbiss <steinbiss@zbh.uni-hamburg.de>
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

#include <string.h>
#include "core/log.h"
#include "core/ma.h"
#include "core/mathsupport.h"
#include "core/range.h"
#include "core/str.h"
#include "core/unused_api.h"
#include "extended/node_stream_rep.h"
#include "extended/feature_node.h"
#include "extended/genome_node_iterator.h"
#include "extended/reverse.h"
#include "ltr/ltrdigest_def.h"
#include "ltr/ltrdigest_stream.h"
#include "ltr/ltr_visitor.h"

struct GtLTRdigestStream {
  const GtNodeStream parent_instance;
  GtNodeStream *in_stream;
  GtBioseq *bioseq;
  GtPBSOptions *pbs_opts;
  GtPPTOptions *ppt_opts;
#ifdef HAVE_HMMER
  GtPdomOptions *pdom_opts;
#endif
  GtLTRVisitor *lv;
  GtStr *ltrdigest_tag;
  int tests_to_run;
  GtLTRElement element;
};

#define gt_ltrdigest_stream_cast(GS)\
        gt_node_stream_cast(gt_ltrdigest_stream_class(), GS)

#ifdef HAVE_HMMER
static int pdom_hit_attach_gff3(void *key, void *value, void *data,
                                GT_UNUSED GtError *err)
{
  unsigned long i;
  GtRange rng;
  struct plan7_s *model = (struct plan7_s *) key;
  GtLTRdigestStream  *ls = (GtLTRdigestStream *) data;
  GtPdomHit *hit = (GtPdomHit*) value;
  GtStrand strand = hit->strand;

  if (strand != gt_feature_node_get_strand(ls->element.mainnode))
    return 0;

  for (i=0;i<gt_array_size(hit->best_chain);i++)
  {
    GtGenomeNode *gf;
    struct hit_s *singlehit = *(struct hit_s **) gt_array_get(hit->best_chain,
                                                              i);
    Phase frame = gt_phase_get(singlehit->name[0]);
    rng.start = singlehit->sqfrom; rng.end = singlehit->sqto;
    gt_pdom_convert_frame_position(&rng, frame);
    gt_ltrelement_offset2pos(&ls->element, &rng, 0,
                             GT_OFFSET_BEGIN_LEFT_LTR,
                             strand);
    gf = gt_feature_node_new(gt_genome_node_get_seqid((GtGenomeNode*)
                                                      ls->element.mainnode),
                             "protein_match",
                             rng.start,
                             rng.end,
                             strand);
    gt_feature_node_set_source((GtFeatureNode*) gf, ls->ltrdigest_tag);
    gt_feature_node_set_phase(gf, frame);
    gt_feature_node_add_attribute((GtFeatureNode*) gf,"pfamname", model->name);
    gt_feature_node_add_attribute((GtFeatureNode*) gf,"pfamid", model->acc);
    gt_feature_node_add_child(ls->element.mainnode, (GtFeatureNode*) gf);
  }
  return 0;
}
#endif

static void pbs_attach_results_to_gff3(GtPBSResults *results, GtLTRElement *element,
                                       GtStr *tag, unsigned int radius)
{
  GtRange pbs_range;
  GtGenomeNode *gf;
  char buffer[BUFSIZ];
  pbs_range.start = results->best_hit->start;
  pbs_range.end   = results->best_hit->end;
  gt_ltrelement_offset2pos(element, &pbs_range, radius,
                           GT_OFFSET_END_LEFT_LTR,
                           results->best_hit->strand);
  results->best_hit->start = pbs_range.start;
  results->best_hit->end = pbs_range.end;
  gf = gt_feature_node_new(gt_genome_node_get_seqid((GtGenomeNode*)
                                                    element->mainnode),
                           "primer_binding_site",
                           pbs_range.start,
                           pbs_range.end,
                           results->best_hit->strand);
  gt_feature_node_set_source((GtFeatureNode*) gf, tag);
  gt_feature_node_set_score((GtFeatureNode*) gf, results->best_hit->score);
  gt_feature_node_add_attribute((GtFeatureNode*) gf,"trna",
                                 results->best_hit->trna);
  gt_feature_node_set_strand((GtGenomeNode*) element->mainnode,
                              results->best_hit->strand);
  (void) snprintf(buffer, BUFSIZ-1, "%lu", results->best_hit->tstart);
  gt_feature_node_add_attribute((GtFeatureNode*) gf,"trnaoffset", buffer);
  (void) snprintf(buffer, BUFSIZ-1, "%lu", results->best_hit->offset);
  gt_feature_node_add_attribute((GtFeatureNode*) gf,"pbsoffset", buffer);
  (void) snprintf(buffer, BUFSIZ-1, "%lu", results->best_hit->edist);
  gt_feature_node_add_attribute((GtFeatureNode*) gf,"edist", buffer);
  gt_feature_node_add_child(element->mainnode, (GtFeatureNode*) gf);
}

static void ppt_attach_results_to_gff3(GtPPTResults *results,
                                       GtLTRElement *element,
                                       GtStr *tag)
{
  GtRange ppt_range;
  GtGenomeNode *gf;
  GtPPTHit* hit = gt_ppt_results_get_ranked_hit(results, 0);
  ppt_range = gt_ppt_hit_get_coords(hit);

  gf = gt_feature_node_new(gt_genome_node_get_seqid((GtGenomeNode*)
                                                    element->mainnode),
                           "RR_tract",
                           ppt_range.start,
                           ppt_range.end,
                           gt_ppt_hit_get_strand(hit));
  gt_feature_node_set_source((GtFeatureNode*) gf, tag);
  gt_feature_node_set_strand((GtGenomeNode*) element->mainnode,
                            gt_ppt_hit_get_strand(hit));
  gt_feature_node_add_child(element->mainnode, (GtFeatureNode*) gf);
}

static void run_ltrdigest(GtLTRElement *element, GtSeq *seq,
                          GtLTRdigestStream *ls, GtError *err)
{
  GtPPTResults *ppt_results;
  GtPBSResults pbs_results;
#ifdef HAVE_HMMER
  GtPdomResults pdom_results;
#endif
  char *rev_seq;
  const char *base_seq = gt_seq_get_orig(seq)+element->leftLTR_5;
  unsigned long seqlen = gt_ltrelement_length(element);

  /* create reverse strand sequence */
  rev_seq = gt_calloc(seqlen+1, sizeof (char));
  memcpy(rev_seq, base_seq, sizeof (char) * seqlen);
  rev_seq[seqlen] = '\0';
  (void) gt_reverse_complement(rev_seq, seqlen, err);

  /* initialize results */
  memset(&pbs_results, 0, sizeof (GtPBSResults));
#ifdef HAVE_HMMER
  memset(&pdom_results, 0, sizeof (GtPdomResults));
#endif

  /* PPT finding
   * -----------*/
  if (ls->tests_to_run & LTRDIGEST_RUN_PPT)
  {
    ppt_results = gt_ppt_find((const char*) base_seq, (const char*) rev_seq,
                            element, ls->ppt_opts);
    if (gt_ppt_results_get_number_of_hits(ppt_results) > 0)
    {
      ppt_attach_results_to_gff3(ppt_results, element,
                                 ls->ltrdigest_tag);
    }
    gt_ppt_results_delete(ppt_results);
  }

  /* PBS finding
   * ----------- */
  if (ls->tests_to_run & LTRDIGEST_RUN_PBS)
  {
    gt_pbs_find((const char*) base_seq, (const char*) rev_seq,
             element, &pbs_results, ls->pbs_opts, err);
     if (pbs_results.best_hit)
     {
      pbs_attach_results_to_gff3(&pbs_results, element,
                                 ls->ltrdigest_tag, ls->pbs_opts->radius);
      gt_feature_node_set_strand((GtGenomeNode *) ls->element.mainnode,
                                  pbs_results.best_hit->strand);
     }
  }

#ifdef HAVE_HMMER
  /* Protein domain finding
   * ----------------------*/
  if (ls->tests_to_run & LTRDIGEST_RUN_PDOM)
  {
    pdom_results.domains = gt_hashmap_new(HASH_DIRECT,
                                          NULL,
                                          gt_pdom_clear_domain_hit);
    gt_pdom_find((const char*) base_seq, (const char*) rev_seq,
                  element, &pdom_results, ls->pdom_opts);
    if (!pdom_results.empty)
    {
      /* determine most likely strand from protein domain results */
      if (gt_double_compare(pdom_results.combined_e_value_fwd,
           pdom_results.combined_e_value_rev) < 0)
      {
        gt_feature_node_set_strand((GtGenomeNode *) ls->element.mainnode,
                                    GT_STRAND_FORWARD);
      }
      else
      {
        gt_feature_node_set_strand((GtGenomeNode *) ls->element.mainnode,
                                    GT_STRAND_REVERSE);
      }

      (void) gt_hashmap_foreach(pdom_results.domains,
                                pdom_hit_attach_gff3,
                                ls,
                                err);
    }
    gt_hashmap_delete(pdom_results.domains);
  }
#endif
  gt_free(rev_seq);
  gt_pbs_clear_results(&pbs_results);
}

int gt_ltrdigest_stream_next(GtNodeStream *gs, GtGenomeNode **gn,
                             GtError *e)
{
  GtLTRdigestStream *ls;
  int had_err;

  gt_error_check(e);
  ls = gt_ltrdigest_stream_cast(gs);

  /* initialize this element */
  memset(&ls->element, 0, sizeof (GtLTRElement));

  /* get annotations from parser */
  had_err = gt_node_stream_next(ls->in_stream, gn, e);
  if (!had_err && *gn)
  {
    GtGenomeNodeIterator *gni;
    GtGenomeNode *mygn;

   if (!gt_feature_node_try_cast(*gn))
     return 0;

    /* fill LTRElement structure from GFF3 subgraph */
    gni = gt_genome_node_iterator_new(*gn);
    for (mygn = *gn; mygn; mygn = gt_genome_node_iterator_next(gni))
      (void) gt_genome_node_accept(mygn, (GtNodeVisitor*) ls->lv, e);
    gt_genome_node_iterator_delete(gni);
  }

  if (ls->element.mainnode)
  {
    unsigned long seqid;
    const char *sreg;
    GtSeq *seq;
    GtRange elemrng;

    sreg = gt_str_get(gt_genome_node_get_seqid((GtGenomeNode*)
                                               ls->element.mainnode));
    (void) sscanf(sreg,"seq%lu", &seqid);
    seq = gt_bioseq_get_seq(ls->bioseq, seqid);

    elemrng = gt_genome_node_get_range((GtGenomeNode*) ls->element.mainnode);
    if (elemrng.end <= gt_seq_length(seq))
      /* run LTRdigest core routine */
      run_ltrdigest(&ls->element, seq, ls, e);
    else
    {
      /* do not process elements whose positions exceed sequence boundaries
       (obviously annotation and sequence do not match!) */
      gt_error_set(e, "Element '%s' exceeds sequence boundaries! (%lu > %lu)",
        gt_feature_node_get_attribute(ls->element.mainnode, "ID"),
        elemrng.end, gt_seq_length(seq));
      had_err = -1;
    }
  }
  return had_err;
}

void gt_ltrdigest_stream_free(GtNodeStream *gs)
{
  GtLTRdigestStream *ls = gt_ltrdigest_stream_cast(gs);
  gt_node_visitor_delete((GtNodeVisitor*) ls->lv);
  gt_str_delete(ls->ltrdigest_tag);
  gt_node_stream_delete(ls->in_stream);
}

const GtNodeStreamClass* gt_ltrdigest_stream_class(void)
{
  static const GtNodeStreamClass *gsc;
  if (!gsc)
    gsc = gt_node_stream_class_new(sizeof (GtLTRdigestStream),
                                   gt_ltrdigest_stream_free,
                                   gt_ltrdigest_stream_next );
  return gsc;
}

GtNodeStream* gt_ltrdigest_stream_new(GtNodeStream *in_stream,
                                      int tests_to_run,
                                      GtBioseq *bioseq,
                                      GtPBSOptions *pbs_opts,
                                      GtPPTOptions *ppt_opts
#ifdef HAVE_HMMER
           ,GtPdomOptions *pdom_opts
#endif
           )
{
  GtNodeStream *gs;
  GtLTRdigestStream *ls;
  gs = gt_node_stream_create(gt_ltrdigest_stream_class(), true);
  ls = gt_ltrdigest_stream_cast(gs);
  ls->in_stream = gt_node_stream_ref(in_stream);
  ls->ppt_opts = ppt_opts;
  ls->pbs_opts = pbs_opts;
#ifdef HAVE_HMMER
  ls->pdom_opts = pdom_opts;
#endif
  ls->tests_to_run = tests_to_run;
  ls->bioseq = bioseq;
  ls->ltrdigest_tag = gt_str_new_cstr("LTRdigest");
  ls->lv = (GtLTRVisitor*) gt_ltr_visitor_new(&ls->element);
  return gs;
}