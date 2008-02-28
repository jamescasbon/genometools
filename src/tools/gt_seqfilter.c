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

#include <string.h>
#include "libgtcore/bioseq.h"
#include "libgtcore/fasta.h"
#include "libgtcore/ma.h"
#include "libgtcore/option.h"
#include "libgtcore/undef.h"
#include "tools/gt_seqfilter.h"

typedef struct {
  unsigned long minlength,
                maxlength;
} SeqFilterArguments;

static void* gt_seqfilter_arguments_new(void)
{
  SeqFilterArguments *arguments = ma_calloc(1, sizeof *arguments);
  return arguments;
}

static void gt_seqfilter_arguments_delete(void *tool_arguments)
{
  SeqFilterArguments *arguments = tool_arguments;
  if (!tool_arguments) return;
  ma_free(arguments);
}

static OptionParser* gt_seqfilter_option_parser_new(void *tool_arguments)
{
  SeqFilterArguments *arguments = tool_arguments;
  Option *option;
  OptionParser *op;
  assert(arguments);

  op = option_parser_new("[option ...] sequence_file [...]",
                         "Filter the given sequence_file(s) and show the "
                         "results on stdout.");

  /* -minlength */
  option = option_new_ulong("minlength", "set minimum length a sequence must "
                            "have to pass the filter", &arguments->minlength,
                            UNDEF_ULONG);
  option_parser_add_option(op, option);

  /* -maxlength */
  option = option_new_ulong("maxlength", "set maximum length a sequence can "
                            "have to pass the filter", &arguments->maxlength,
                            UNDEF_ULONG);
  option_parser_add_option(op, option);

  /* set minimal arugments */
  option_parser_set_min_args(op, 1);

  return op;
}

static int gt_seqfilter_runner(int argc, const char **argv,
                               void *tool_arguments, Error *err)
{
  SeqFilterArguments *arguments = tool_arguments;
  Bioseq *bioseq;
  unsigned long i;
  int arg = 0, had_err = 0;

  error_check(err);
  assert(tool_arguments);

  while (!had_err && arg < argc) {
    bioseq = bioseq_new(argv[arg], err);
    if (!bioseq)
      had_err = -1;

    for (i = 0; i < bioseq_number_of_sequences(bioseq); i++) {
      if ((arguments->minlength == UNDEF_ULONG ||
           bioseq_get_sequence_length(bioseq, i) >= arguments->minlength) &&
          (arguments->maxlength == UNDEF_ULONG ||
           bioseq_get_sequence_length(bioseq, i) <= arguments->maxlength)) {
        fasta_show_entry(bioseq_get_description(bioseq, i),
                         bioseq_get_sequence(bioseq, i),
                         bioseq_get_sequence_length(bioseq, i), 0);
      }
    }

    bioseq_delete(bioseq);

    arg++;
  }

  return had_err;
}

Tool* gt_seqfilter(void)
{
  return tool_new(gt_seqfilter_arguments_new,
                  gt_seqfilter_arguments_delete,
                  gt_seqfilter_option_parser_new,
                  NULL,
                  gt_seqfilter_runner);
}