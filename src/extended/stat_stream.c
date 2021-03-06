/*
  Copyright (c) 2007-2008 Gordon Gremme <gremme@zbh.uni-hamburg.de>
  Copyright (c) 2007-2008 Center for Bioinformatics, University of Hamburg

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

#include "core/assert_api.h"
#include "extended/stat_stream.h"
#include "extended/stat_visitor.h"
#include "extended/node_stream_api.h"

struct GtStatStream
{
  const GtNodeStream parent_instance;
  GtNodeStream *in_stream;
  GtNodeVisitor *stat_visitor;
  unsigned long number_of_DAGs;
};

#define stat_stream_cast(GS)\
        gt_node_stream_cast(gt_stat_stream_class(), GS)

static int stat_stream_next(GtNodeStream *gs, GtGenomeNode **gn, GtError *err)
{
  GtStatStream *stat_stream;
  int had_err;
  gt_error_check(err);
  stat_stream = stat_stream_cast(gs);
  had_err = gt_node_stream_next(stat_stream->in_stream, gn, err);
  if (!had_err) {
    gt_assert(stat_stream->stat_visitor);
    if (*gn) {
      stat_stream->number_of_DAGs++;
      had_err = gt_genome_node_accept(*gn, stat_stream->stat_visitor, err);
      gt_assert(!had_err); /* the status visitor is sane */
    }
  }
  return had_err;
}

static void stat_stream_free(GtNodeStream *gs)
{
  GtStatStream *stat_stream = stat_stream_cast(gs);
  gt_node_visitor_delete(stat_stream->stat_visitor);
  gt_node_stream_delete(stat_stream->in_stream);
}

const GtNodeStreamClass* gt_stat_stream_class(void)
{
  static const GtNodeStreamClass *nsc = NULL;
  if (!nsc) {
    nsc = gt_node_stream_class_new(sizeof (GtStatStream),
                                   stat_stream_free,
                                   stat_stream_next);
  }
  return nsc;
}

GtNodeStream* gt_stat_stream_new(GtNodeStream *in_stream,
                                 bool gene_length_distri,
                                 bool gene_score_distri,
                                 bool exon_length_distri,
                                 bool exon_number_distri,
                                 bool intron_length_distri)
{
  GtNodeStream *gs = gt_node_stream_create(gt_stat_stream_class(), false);
  GtStatStream *ss = stat_stream_cast(gs);
  ss->in_stream = gt_node_stream_ref(in_stream);
  ss->stat_visitor = gt_stat_visitor_new(gene_length_distri, gene_score_distri,
                                         exon_length_distri, exon_number_distri,
                                         intron_length_distri);
  return gs;
}

void gt_stat_stream_show_stats(GtNodeStream *gs)
{
  GtStatStream *ss = stat_stream_cast(gs);
  printf("parsed feature trees: %lu\n", ss->number_of_DAGs);
  gt_stat_visitor_show_stats(ss->stat_visitor);
}
