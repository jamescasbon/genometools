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

#include "core/bioseq_iterator.h"
#include "core/fasta.h"
#include "core/option.h"
#include "core/ma.h"
#include "core/unused_api.h"
#include "extended/gtdatahelp.h"
#include "extended/shredder.h"
#include "tools/gt_shredder.h"

typedef struct {
  unsigned long coverage,
                minlength,
                maxlength,
                overlap;
  double sample_probability;
} GtShredderArguments;

static void* gt_shredder_arguments_new(void)
{
  return gt_calloc(1, sizeof (GtShredderArguments));
}

static void gt_shredder_arguments_delete(void *tool_arguments)
{
  GtShredderArguments *arguments = tool_arguments;
  if (!arguments) return;
  gt_free(arguments);
}

static GtOptionParser* gt_shredder_option_parser_new(void *tool_arguments)
{
  GtShredderArguments *arguments = tool_arguments;
  GtOptionParser *op;
  GtOption *o;
  gt_assert(arguments);
  op = gt_option_parser_new("[option ...] [sequence_file ...]",
                         "GtShredder sequence_file into consecutive pieces of "
                         "random length.");
  o = gt_option_new_ulong_min("coverage", "set the number of times the "
                           "sequence_file is shreddered", &arguments->coverage,
                           1, 1);
  gt_option_parser_add_option(op, o);
  o = gt_option_new_ulong("minlength",
                       "set the minimum length of the shreddered "
                       "fragments", &arguments->minlength, 300);
  gt_option_parser_add_option(op, o);
  o = gt_option_new_ulong("maxlength",
                       "set the maximum length of the shreddered "
                       "fragments", &arguments->maxlength, 700);
  gt_option_parser_add_option(op, o);
  o = gt_option_new_ulong("overlap", "set the overlap between consecutive "
                       "pieces", &arguments->overlap, 0);
  gt_option_parser_add_option(op, o);
  o = gt_option_new_probability("sample", "take samples of the generated "
                             "sequences pieces with the given probability",
                             &arguments->sample_probability, 1.0);
  gt_option_parser_add_option(op, o);
  gt_option_parser_set_comment_func(op, gt_gtdata_show_help, NULL);
  return op;
}

static int gt_shredder_arguments_check(GT_UNUSED int rest_argc,
                                       void *tool_arguments, GtError *err)
{
  GtShredderArguments *arguments = tool_arguments;
  gt_error_check(err);
  gt_assert(arguments);
  if (arguments->minlength > arguments->maxlength) {
    gt_error_set(err, "-minlength must be <= than -maxlength");
    return -1;
  }
  return 0;
}

static int gt_shredder_runner(GT_UNUSED int argc, const char **argv,
                              int parsed_args, void *tool_arguments,
                              GtError *err)
{
  GtShredderArguments *arguments = tool_arguments;
  GtBioseqIterator *bsi;
  unsigned long i;
  GtBioseq *bioseq;
  int had_err;
  GtStr *desc;

  gt_error_check(err);
  gt_assert(arguments);

  /* init */
  desc = gt_str_new();
  bsi = gt_bioseq_iterator_new(argc - parsed_args, argv + parsed_args);

  /* shredder */
  while (!(had_err = gt_bioseq_iterator_next(bsi, &bioseq, err)) && bioseq) {
    for (i = 0; i < arguments->coverage; i++) {
      GtShredder *shredder;
      unsigned long fragment_length;
      const char *fragment;
      shredder = gt_shredder_new(bioseq, arguments->minlength,
                              arguments->maxlength);
      gt_shredder_set_overlap(shredder, arguments->overlap);
      gt_shredder_set_sample_probability(shredder,
                                         arguments->sample_probability);
      while ((fragment = gt_shredder_shred(shredder, &fragment_length, desc))) {
        gt_str_append_cstr(desc, " [shreddered fragment]");
        gt_fasta_show_entry(gt_str_get(desc), fragment, fragment_length, 0);
      }
      gt_shredder_delete(shredder);
    }
    gt_bioseq_delete(bioseq);
  }

  /* free */
  gt_bioseq_iterator_delete(bsi);
  gt_str_delete(desc);

  return had_err;
}

GtTool* gt_shredder(void)
{
  return gt_tool_new(gt_shredder_arguments_new,
                     gt_shredder_arguments_delete,
                     gt_shredder_option_parser_new,
                     gt_shredder_arguments_check,
                     gt_shredder_runner);
}
