/*
  Copyright (c) 2007 Stefan Kurtz <kurtz@zbh.uni-hamburg.de>
  Copyright (c) 2007 Center for Bioinformatics, University of Hamburg

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
#include "core/versionfunc.h"
#include "match/test-mtrieins.pr"
#include "tools/gt_trieins.h"

static OPrval parse_options(bool *onlyins,int *parsed_args,
                            int argc, const char **argv, GtError *err)
{
  GtOptionParser *op;
  GtOption *option;
  OPrval oprval;

  gt_error_check(err);
  op = gt_option_parser_new("[options] indexname",
                         "Perform trie insertions and check consistency.");
  gt_option_parser_set_mailaddress(op,"<kurtz@zbh.uni-hamburg.de>");
  option= gt_option_new_bool("ins","perform only insertions",onlyins,false);
  gt_option_parser_add_option(op, option);
  gt_option_parser_set_min_max_args(op, 1U, 1U);
  oprval = gt_option_parser_parse(op, parsed_args, argc, argv, gt_versionfunc,
                                  err);
  gt_option_parser_delete(op);
  return oprval;
}

int gt_trieins(int argc, const char **argv, GtError *err)
{
  GtStr *indexname;
  bool haserr = false;
  int parsed_args;
  bool onlyins = false;

  gt_error_check(err);

  switch (parse_options(&onlyins,&parsed_args, argc, argv, err)) {
    case OPTIONPARSER_OK: break;
    case OPTIONPARSER_ERROR: return -1;
    case OPTIONPARSER_REQUESTS_EXIT: return 0;
  }
  gt_assert(parsed_args == 1);

  indexname = gt_str_new_cstr(argv[parsed_args]);
  if (test_trieins(onlyins,indexname,err) != 0)
  {
    haserr = true;
  }
  gt_str_delete(indexname);
  return haserr ? -1 : 0;
}
