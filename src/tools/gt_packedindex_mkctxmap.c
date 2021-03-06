/*
  Copyright (c) 2008 Thomas Jahns <Thomas.Jahns@gmx.net>

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

#include "core/error.h"
#include "core/option.h"
#include "core/str.h"
#include "core/versionfunc.h"

#include "match/eis-bwtseq.h"
#include "match/eis-bwtseq-construct.h"
#include "match/eis-bwtseq-context-param.h"
#include "match/eis-bwtseq-sass.h"
#include "match/seqpos-def.h"
#include "match/sarr-def.h"
#include "match/esa-map.h"
#include "tools/gt_packedindex_mkctxmap.h"

struct mkCtxMapOptions
{
  int mapIntervalLog2;
  bool verboseOutput;
};

static OPrval
parseMkCtxMapOptions(int *parsed_args, int argc, const char **argv,
                     struct mkCtxMapOptions *params, GtError *err);

extern int
gt_packedindex_mkctxmap(int argc, const char *argv[], GtError *err)
{
  struct mkCtxMapOptions params;
  GtStr *projectName = NULL;
  Verboseinfo *verbosity = NULL;
  BWTSeq *bwtSeq = NULL;
  SASeqSrc *src;
  int parsedArgs;
  bool had_err = false;
  bool saInitialized = false, saiInitialized = false;
  Suffixarray sa;
  SuffixarrayFileInterface sai;
  projectName = gt_str_new();

  do {
    gt_error_check(err);
    {
      bool exitNow = false;
      switch (parseMkCtxMapOptions(&parsedArgs, argc, argv, &params, err))
      {
      case OPTIONPARSER_OK:
        break;
      case OPTIONPARSER_ERROR:
        had_err = true;
        exitNow = true;
        break;
      case OPTIONPARSER_REQUESTS_EXIT:
        exitNow = true;
        break;
      }
      if (exitNow)
        break;
    }
    gt_str_set(projectName, argv[parsedArgs]);
    verbosity = newverboseinfo(params.verboseOutput);
    /* try to find appropriate suffix source */
    {
      Seqpos len;
      if (streamsuffixarray(&sa, SARR_SUFTAB, projectName, verbosity, err))
      {
        gt_error_unset(err);
        if (streamsuffixarray(&sa, 0, projectName, verbosity, err))
        {
          had_err = true;
          break;
        }
        len = getencseqtotallength(sa.encseq) + 1;
        saInitialized = true;
        bwtSeq = loadBWTSeqForSA(projectName, BWT_ON_BLOCK_ENC,
                                 BWTDEFOPT_MULTI_QUERY, &sa, len, err);
        if (!(src = BWTSeqNewSASeqSrc(bwtSeq, NULL)))
        {
          gt_error_set(err, "The project %s does not contain sufficient"
                    " information to regenerate the suffix array.",
                    gt_str_get(projectName));
          had_err = true;
          break;
        }
      }
      else
      {
        len = getencseqtotallength(sa.encseq) + 1;
        saInitialized = true;
        initSuffixarrayFileInterface(&sai, len, &sa);
        src = SAI2SASS(&sai);
        saiInitialized = true;
      }
      {
        SeqDataReader readSfxIdx = SASSCreateReader(src, SFX_REQUEST_SUFTAB);
        BWTSeqContextRetriever *bwtSeqCR;
        BWTSeqContextRetrieverFactory *bwtSeqCRF
          = newBWTSeqContextRetrieverFactory(len, params.mapIntervalLog2);
        if (BWTSCRFReadAdvance(bwtSeqCRF, len, readSfxIdx)
            != len)
        {
          gt_error_set(err, "Creation of context map unsuccessful: %s",
                    gt_error_get(err));
          had_err = true;
          deleteBWTSeqContextRetrieverFactory(bwtSeqCRF);
          break;
        }
        bwtSeqCR = BWTSCRFGet(bwtSeqCRF, bwtSeq, projectName);
        deleteBWTSeqCR(bwtSeqCR);
        deleteBWTSeqContextRetrieverFactory(bwtSeqCRF);
      }
    }
  } while (0);
  if (bwtSeq)
  {
    SASSDelete(src);
    deleteBWTSeq(bwtSeq);
  }
  if (saiInitialized) destructSuffixarrayFileInterface(&sai);;
  if (saInitialized) freesuffixarray(&sa);
  if (verbosity) freeverboseinfo(&verbosity);
  if (projectName) gt_str_delete(projectName);
  return had_err?-1:0;
}

static OPrval
parseMkCtxMapOptions(int *parsed_args, int argc, const char **argv,
                     struct mkCtxMapOptions *params, GtError *err)
{
  GtOptionParser *op;
  OPrval oprval;
  GtOption *option;

  gt_error_check(err);
  op = gt_option_parser_new("indexname",
                         "Build BWT packedindex for project <indexname>.");
  registerCtxMapOptions(op, &params->mapIntervalLog2);

  option = gt_option_new_bool("v",
                           "print verbose progress information",
                           &params->verboseOutput,
                           false);
  gt_option_parser_add_option(op, option);

  gt_option_parser_set_min_max_args(op, 1, 1);
  oprval = gt_option_parser_parse(op, parsed_args, argc, argv, gt_versionfunc,
                                  err);

  gt_option_parser_delete(op);

  return oprval;
}
