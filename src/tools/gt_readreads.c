/*
  Copyright (c) 2009 Sascha Steinbiss <steinbiss@zbh.uni-hamburg.de>
  Copyright (c) 2009 Center for Bioinformatics, University of Hamburg

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
#include "core/fileutils_api.h"
#include "core/ma.h"
#include "core/progressbar.h"
#include "core/quality.h"
#include "core/seqiterator_qual_fastq.h"
#include "core/str_array_api.h"
#include "core/unused_api.h"
#include "tools/gt_readreads.h"

#define SEQUENCE_CHAR_SEPARATOR '|'

typedef struct {
  bool verbose,
       showseq;
  GtStr *qualformat;
} GtReadreads;

static void* gt_readreads_arguments_new(void)
{
  GtReadreads *arguments = gt_calloc(1, sizeof *arguments);
  arguments->qualformat = gt_str_new();
  return arguments;
}

static void gt_readreads_arguments_delete(void *tool_arguments)
{
  GtReadreads *arguments = tool_arguments;
  if (!arguments) return;
  gt_str_delete(arguments->qualformat);
  gt_free(arguments);
}

static GtOptionParser* gt_readreads_option_parser_new(void *tool_arguments)
{
  GtReadreads *opts = tool_arguments;
  GtOptionParser *op;
  GtOption *option;
  gt_assert(opts);

  static const char *qualformats[] = {
    "phred",
    "solexa",
    NULL
  };

  /* init */
  op = gt_option_parser_new("[option ...] [file]",
                            "Read in FASTQ reads with PHRED or Solexa "
                            "qualities and print them.");

  option = gt_option_new_bool("v","be verbose",
                              &opts->verbose, false);
  gt_option_parser_add_option(op, option);

  option = gt_option_new_bool("showseq","show sequences",
                              &opts->showseq, false);
  gt_option_parser_add_option(op, option);

  option = gt_option_new_choice("format", "quality score scale\n"
                                          "can be 'phred' or 'solexa'",
                                opts->qualformat,
                                qualformats[0],
                                qualformats);
  gt_option_parser_add_option(op, option);

  return op;
}

static int gt_readreads_arguments_check(GT_UNUSED int rest_argc,
                                        GT_UNUSED void *tool_arguments,
                                        GT_UNUSED GtError *err)
{
  GT_UNUSED GtReadreads *arguments = tool_arguments;
  int had_err = 0;
  gt_error_check(err);
  gt_assert(arguments);

  return had_err;
}

static int gt_readreads_runner(int argc, const char **argv, int parsed_args,
                               void *tool_arguments, GT_UNUSED GtError *err)
{
  GtReadreads *opts = tool_arguments;
  GtStrArray *files;
  GtSeqIteratorQual *siq;
  int had_err = 0;
  unsigned long i,
                totalsize,
                len;
  const GtUchar *seq,
                *qual;
  char *desc;
  GtStr *scores = gt_str_new();

  gt_error_check(err);
  gt_assert(opts);

  files = gt_str_array_new();
  for (i = parsed_args; i < argc; i++)
  {
    gt_str_array_add_cstr(files, argv[i]);
  }
  totalsize = gt_files_estimate_total_size(files);

  siq = gt_seqiterator_qual_fastq_new(files, err);

  if (opts->verbose)
  {
    gt_progressbar_start(gt_seqiterator_qual_getcurrentcounter(siq,
                                                         (unsigned long long)
                                                         totalsize),
                         (unsigned long long) totalsize);
  }

  while (true)
  {
    had_err = gt_seqiterator_qual_next(siq, &seq, &qual, &len, &desc, err);
    if (had_err != 1)
      break;
    if (opts->showseq) {
      unsigned long *lens = gt_malloc(sizeof (unsigned long)*len);
      gt_str_reset(scores);
      for (i=0;i<len;i++) {
        unsigned long l;
        if (strcmp(gt_str_get(opts->qualformat), "phred") == 0) {
          l = gt_str_length(scores);
          gt_str_append_uint(scores,
                             gt_quality_fastq_to_phred(qual[i]));
          lens[i] = gt_str_length(scores) - l;
        } else if (strcmp(gt_str_get(opts->qualformat), "solexa") == 0) {
          l = gt_str_length(scores);
          gt_str_append_int(scores,
                            gt_quality_fastq_to_solexa(qual[i]));
          lens[i] = gt_str_length(scores) - l;
        }
        if (i != len-1)
          gt_str_append_char(scores, SEQUENCE_CHAR_SEPARATOR);
      }
      for (i=0;i<len;i++) {
        printf("%*c", (int) lens[i], seq[i]);
        if (i != len-1)
          printf("%c", SEQUENCE_CHAR_SEPARATOR);
      }
      printf("\n%s\n\n", gt_str_get(scores));
      gt_free(lens);
    }
    gt_free(desc);
  }
  if (opts->verbose)
    gt_progressbar_stop();
  gt_str_array_delete(files);
  gt_str_delete(scores);
  gt_seqiterator_qual_delete(siq);
  return had_err;
}

GtTool* gt_readreads(void)
{
  return gt_tool_new(gt_readreads_arguments_new,
                     gt_readreads_arguments_delete,
                     gt_readreads_option_parser_new,
                     gt_readreads_arguments_check,
                     gt_readreads_runner);
}
