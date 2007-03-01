/*
  Copyright (c) 2006-2007 Gordon Gremme <gremme@zbh.uni-hamburg.de>
  Copyright (c) 2006-2007 Center for Bioinformatics, University of Hamburg
  See LICENSE file or http://genometools.org/license.html for license details.
*/

#include "gt.h"

#define GT_CDS_SOURCE_TAG "gt cds"

typedef struct {
  bool verbose;
  Str *seqfile,
      *regionmapping;
} CDS_arguments;

static OPrval parse_options(int *parsed_args, CDS_arguments *arguments,
                            int argc, char **argv, Env *env)
{
  OptionParser *op;
  Option *option;
  OPrval oprval;
  env_error_check(env);
  op = option_parser_new("[option ...] GFF3_file", "Add CDS features to exon "
                         "features given in GFF3_file.", env);

  /* -seqfile and -regionmapping */
  seqid2file_options(op, arguments->seqfile, arguments->regionmapping, env);

  /* -v */
  option = option_new_verbose(&arguments->verbose, env);
  option_parser_add_option(op, option, env);

  /* parse */
  option_parser_set_comment_func(op, gtdata_show_help, NULL);
  oprval = option_parser_parse_min_max_args(op, parsed_args, argc, argv,
                                            versionfunc, 2, 2, env);
  option_parser_delete(op, env);
  return oprval;
}

int gt_cds(int argc, char *argv[], Env *env)
{
  GenomeStream *gff3_in_stream, *cds_stream = NULL, *gff3_out_stream = NULL;
  GenomeNode *gn;
  CDS_arguments arguments;
  RegionMapping *regionmapping;
  int parsed_args, has_err = 0;
  env_error_check(env);

  /* option parsing */
  arguments.seqfile = str_new(env);
  arguments.regionmapping = str_new(env);

  switch (parse_options(&parsed_args, &arguments, argc, argv, env)) {
    case OPTIONPARSER_OK: break;
    case OPTIONPARSER_ERROR:
      str_delete(arguments.regionmapping, env);
      str_delete(arguments.seqfile, env);
      return -1;
    case OPTIONPARSER_REQUESTS_EXIT:
      str_delete(arguments.regionmapping, env);
      str_delete(arguments.seqfile, env);
      return 0;
  }

  /* create gff3 input stream */
  assert(parsed_args < argc);
  gff3_in_stream = gff3_in_stream_new_sorted(argv[parsed_args],
                                             arguments.verbose, env);

  /* create region mapping */
  regionmapping = seqid2file_regionmapping_new(arguments.seqfile,
                                               arguments.regionmapping, env);
  if (!regionmapping)
    has_err = -1;

  /* create CDS stream */
  if (!has_err) {
    cds_stream = cds_stream_new(gff3_in_stream, regionmapping,
                                GT_CDS_SOURCE_TAG, env);
    if (!cds_stream)
      has_err = -1;
  }

  /* create gff3 output stream */
  if (!has_err)
    gff3_out_stream = gff3_out_stream_new(cds_stream, stdout, env);

  /* pull the features through the stream and free them afterwards */
  while (!(has_err = genome_stream_next_tree(gff3_out_stream, &gn, env)) &&
         gn) {
    genome_node_rec_delete(gn, env);
  }

  /* free */
  genome_stream_delete(gff3_out_stream, env);
  genome_stream_delete(cds_stream, env);
  genome_stream_delete(gff3_in_stream, env);
  str_delete(arguments.regionmapping, env);
  str_delete(arguments.seqfile, env);

  return has_err;
}
