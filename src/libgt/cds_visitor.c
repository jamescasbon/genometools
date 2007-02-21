/*
  Copyright (c) 2006-2007 Gordon Gremme <gremme@zbh.uni-hamburg.de>
  Copyright (c) 2006-2007 Center for Bioinformatics, University of Hamburg
  See LICENSE file or http://genometools.org/license.html for license details.
*/

#include <assert.h>
#include "bioseq.h"
#include "cds_visitor.h"
#include "error.h"
#include "fasta.h"
#include "genome_visitor_rep.h"
#include "orf.h"
#include "splicedseq.h"
#include "translate.h"
#include "undef.h"

struct CDSVisitor {
  const GenomeVisitor parent_instance;
  Str *sequence_file,
      *source;
  Splicedseq *splicedseq; /* the (spliced) sequence of the currently considered
                             gene */
  Bioseq *bioseq;
};

#define cds_visitor_cast(GV)\
        genome_visitor_cast(cds_visitor_class(), GV)

static void cds_visitor_free(GenomeVisitor *gv)
{
  CDSVisitor *cds_visitor = cds_visitor_cast(gv);
  assert(cds_visitor);
  str_delete(cds_visitor->sequence_file);
  str_delete(cds_visitor->source);
  splicedseq_delete(cds_visitor->splicedseq);
  bioseq_delete(cds_visitor->bioseq);
}

static int extract_cds_if_necessary(GenomeNode *gn, void *data, Env *env)
{
  CDSVisitor *v = (CDSVisitor*) data;
  const char *sequence;
  unsigned long seqnum;
  GenomeFeature *gf;
  Range range;

  env_error_check(env);
  gf = genome_node_cast(genome_feature_class(), gn);
  assert(gf);

  if (genome_feature_get_type(gf) == gft_exon &&
      (genome_feature_get_strand(gf) == STRAND_FORWARD ||
       genome_feature_get_strand(gf) == STRAND_REVERSE)) {
    sequence = bioseq_get_sequence_with_desc(v->bioseq,
                                             str_get(genome_node_get_seqid(gn)),
                                             &seqnum);
    range = genome_node_get_range(gn);
    assert(range.start && range.end); /* 1-based coordinates */
    assert(range.end <= bioseq_get_sequence_length(v->bioseq, seqnum));
    splicedseq_add(v->splicedseq, range.start - 1, range.end - 1, sequence);
  }
  return 0;
}

static int add_cds_if_necessary(GenomeNode *gn, void *data, Env *env)
{
  CDSVisitor *v = (CDSVisitor*) data;
  GenomeNode *cds_feature;
  Str *pr_0, *pr_1, *pr_2;
  GenomeFeature *gf;
  unsigned long i;
  Array *orfs;
  Range orf, cds;
  Strand strand;
  int has_err;

  env_error_check(env);
  gf = genome_node_cast(genome_feature_class(), gn);
  assert(gf);

  /* traverse the direct children */
  splicedseq_reset(v->splicedseq);
  has_err = genome_node_traverse_direct_children(gn, v,
                                                 extract_cds_if_necessary, env);
  if (!has_err && splicedseq_length(v->splicedseq) > 2) {
    strand = genome_feature_get_strand(gf);
    if (strand == STRAND_REVERSE) {
      if (splicedseq_reverse(v->splicedseq, env))
        return -1;
    }
    /* determine ORFs for all three frames */
    pr_0 = str_new();
    pr_1 = str_new();
    pr_2 = str_new();
    /* printf("pr_0=%s\n", str_get(pr_0)); */
    orfs = array_new(sizeof (Range));
    translate_dna(pr_0, splicedseq_get(v->splicedseq),
                  splicedseq_length(v->splicedseq), 0);
    translate_dna(pr_1, splicedseq_get(v->splicedseq),
                  splicedseq_length(v->splicedseq), 1);
    translate_dna(pr_2, splicedseq_get(v->splicedseq),
                  splicedseq_length(v->splicedseq), 2);
    determine_ORFs(orfs, 0, str_get(pr_0), str_length(pr_0));
    determine_ORFs(orfs, 1, str_get(pr_1), str_length(pr_1));
    determine_ORFs(orfs, 2, str_get(pr_2), str_length(pr_2));

    if (array_size(orfs)) {
      /* sort ORFs according to length */
      ranges_sort_by_length_stable(orfs);

      /* create CDS features from the longest ORF */
      orf = *(Range*) array_get(orfs, 0);
      assert(range_length(orf) >= 3);
      /* the first CDS feature */
      /*printf("%lu, %lu\n", orf.start, orf.end); */
      cds.start = splicedseq_map(v->splicedseq, strand == STRAND_FORWARD
                                 ? orf.start : orf.end) + 1;
      cds.end = splicedseq_map(v->splicedseq, strand == STRAND_FORWARD
                               ? orf.end : orf.start) + 1;
      /*printf("%lu, %lu\n", cds.start, cds.end);*/
      cds_feature = genome_feature_new(gft_CDS, cds,
                                       genome_feature_get_strand(gf), NULL,
                                       UNDEFULONG);
      genome_node_set_source(cds_feature, v->source);
      genome_node_set_seqid(cds_feature, genome_node_get_seqid(gn));
      genome_node_set_phase(cds_feature, 0);
      /* all CDS features in between */
      for (i = strand == STRAND_FORWARD ? orf.start + 1 : orf.end - 1;
           strand == STRAND_FORWARD ? i < orf.end : i > orf.start;
           strand == STRAND_FORWARD ? i++ : i--) {
        if (splicedseq_pos_is_border(v->splicedseq, i)) {
          /*printf("i=%lu\n", i);*/
          genome_feature_set_end((GenomeFeature*) cds_feature,
                                 splicedseq_map(v->splicedseq, i) + 1);
          genome_node_is_part_of_genome_node(gn, cds_feature);
          if (strand == STRAND_FORWARD)
            orf.start = i + 1;
          else
            orf.end = i - 1;
          cds.start = splicedseq_map(v->splicedseq, strand == STRAND_FORWARD
                                     ? orf.start : orf.end) + 1;
          cds.end = splicedseq_map(v->splicedseq, strand == STRAND_FORWARD
                                   ? orf.end : orf.start) + 1;
          cds_feature = genome_feature_new(gft_CDS, cds,
                                           genome_feature_get_strand(gf), NULL,
                                           UNDEFULONG);
          genome_node_set_source(cds_feature, v->source);
          genome_node_set_seqid(cds_feature, genome_node_get_seqid(gn));
          /* XXX correct this */
          genome_node_set_phase(cds_feature,
                                splicedseq_map(v->splicedseq, orf.start)  % 3);
        }
      }
      /* set the end of the last CDS feature and store it */
      genome_feature_set_end((GenomeFeature*) cds_feature,
                             splicedseq_map(v->splicedseq,
                                            strand == STRAND_FORWARD
                                            ? orf.end : orf.start) + 1);
      genome_node_is_part_of_genome_node(gn, cds_feature);
    }

    /* free */
    array_delete(orfs);
    str_delete(pr_2);
    str_delete(pr_1);
    str_delete(pr_0);
  }
  return has_err;
}

static int cds_visitor_genome_feature(GenomeVisitor *gv, GenomeFeature *gf,
                                      /*@unused@*/ Log *l, Env *env)
{
  CDSVisitor *v = cds_visitor_cast(gv);
  env_error_check(env);
  return genome_node_traverse_children((GenomeNode*) gf, v,
                                       add_cds_if_necessary, false, env);

}

static int cds_visitor_sequence_region(GenomeVisitor *gv, SequenceRegion *sr,
                                       /*@unused@*/ Log *l, Env *env)
{
  CDSVisitor *cds_visitor;
  env_error_check(env);
  cds_visitor = cds_visitor_cast(gv);
  /* check if the given sequence file contains this sequence (region) */
  if (!bioseq_contains_sequence(cds_visitor->bioseq,
                                str_get(genome_node_get_seqid((GenomeNode*)
                                                              sr)))) {
    env_error_set(env, "sequence \"%s\" not contained in sequence file \"%s\"",
              str_get(genome_node_get_seqid((GenomeNode*) sr)),
              str_get(cds_visitor->sequence_file));
    return -1;
  }
  return 0;
}

const GenomeVisitorClass* cds_visitor_class()
{
  static const GenomeVisitorClass gvc = { sizeof (CDSVisitor),
                                          cds_visitor_free,
                                          NULL,
                                          cds_visitor_genome_feature,
                                          cds_visitor_sequence_region,
                                          NULL };
  return &gvc;
}

GenomeVisitor* cds_visitor_new(Str *sequence_file, Str *source, Env *env)
{
  GenomeVisitor *gv;
  CDSVisitor *cds_visitor;
  env_error_check(env);
  gv = genome_visitor_create(cds_visitor_class());
  cds_visitor = cds_visitor_cast(gv);
  cds_visitor->sequence_file = str_ref(sequence_file);
  cds_visitor->source = str_ref(source);
  cds_visitor->splicedseq = splicedseq_new();
  cds_visitor->bioseq = bioseq_new_str(sequence_file, env);
  if (!cds_visitor->bioseq) {
    cds_visitor_free(gv);
    return NULL;
  }
  return gv;
}
