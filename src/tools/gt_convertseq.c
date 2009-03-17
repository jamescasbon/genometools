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

#include "core/fileutils.h"
#include "core/filelengthvalues.h"
#include "core/ma.h"
#include "core/option.h"
#include "core/sequence_buffer_embl.h"
#include "core/seqiterator.h"
#include "core/unused_api.h"
#include "core/versionfunc.h"
#include "core/xposix.h"
#include "core/progressbar.h"
#include "core/disc_distri.h"
#include "match/format64.h"
#include "match/stamp.h"
#include "tools/gt_convertseq.h"

typedef struct
{
  bool verbose,
       showflv;
  unsigned long fastawidth;
} ConvertseqOptions;

static OPrval parse_options(ConvertseqOptions *opts,
                            int *parsed_args,int argc,
                            const char **argv, GtError *err)
{
  GtOptionParser *op;
  GtOption *optionverbose, *o;
  OPrval oprval;

  gt_error_check(err);

  op = gt_option_parser_new("[options] file [...]",
                         "Parse and convert sequence file formats.");
  gt_option_parser_set_mailaddress(op,"<steinbiss@zbh.uni-hamburg.de>");

  optionverbose = gt_option_new_bool("v","be verbose",
                                     &opts->verbose, false);
  gt_option_parser_add_option(op, optionverbose);

  o = gt_option_new_bool("showfilelengthvalues","show filelengths",
                         &opts->showflv, false);
  gt_option_parser_add_option(op, o);

  o = gt_option_new_ulong("fastawidth","FASTA output line width",
                         &opts->fastawidth, 60);
  gt_option_parser_add_option(op, o);

  gt_option_parser_set_min_args(op, 1U);
  oprval = gt_option_parser_parse(op, parsed_args, argc, argv, gt_versionfunc,
                                  err);
  gt_option_parser_delete(op);
  return oprval;
}

int gt_convertseq(int argc, const char **argv, GtError *err)
{
  GtStrArray *files;
  const Uchar *sequence;
  char *desc;
  unsigned long len, j;
  int i, parsed_args, had_err;
  off_t totalsize;
  ConvertseqOptions opts;
  Filelengthvalues *flv;

  gt_error_check(err);

  /* option parsing */
  switch (parse_options(&opts,&parsed_args, argc, argv, err)) {
    case OPTIONPARSER_OK: break;
    case OPTIONPARSER_ERROR:
        return -1;
    case OPTIONPARSER_REQUESTS_EXIT:
        return 0;
  }

  files = gt_str_array_new();
  for (i = parsed_args; i < argc; i++)
  {
    gt_str_array_add_cstr(files, argv[i]);
  }
  totalsize = gt_files_estimate_total_size(files);
  
  flv = gt_calloc(gt_str_array_size(files), sizeof (Filelengthvalues));

  GtSeqIterator *seqit;
  GtSequenceBuffer *eb = gt_sequence_buffer_embl_new(files);
  /* read input using seqiterator */
  seqit = gt_seqiterator_new_with_buffer(eb, NULL, true);
  if (opts.verbose)
  {
    gt_progressbar_start(gt_seqiterator_getcurrentcounter(seqit,
                                                         (unsigned long long)
                                                         totalsize),
                         (unsigned long long) totalsize);
  }
  while (true)
  {
    desc = NULL;
    i = 0;
    j = 1;
    had_err = gt_seqiterator_next(seqit, &sequence, &len, &desc, err);
    if (had_err != 1)
      break;
    printf(">%s\n", desc);
    for(i=0;i<len;i++) {
      putc(sequence[i], stdout);
      if ((j % opts.fastawidth) == 0) {
        j=0;
        printf("\n");
      }
      j++;
    }
    printf("\n");
    gt_free(desc);
  }
  if (opts.showflv) {
    unsigned long j;
    for (j=0;j<gt_str_array_size(files);j++) {
      printf("file %lu (%s): %lu/%lu\n", j, gt_str_array_get(files, j),
                                      flv[j].length, flv[j].effectivelength);
    }
  }
  gt_sequence_buffer_delete(eb);
  gt_seqiterator_delete(seqit);
  if (opts.verbose)
  {
    gt_progressbar_stop();
  }
  gt_str_array_delete(files);
  gt_free(flv);
  return had_err;
}