/*
  Copyright (c) 2007-2009 Gordon Gremme <gremme@zbh.uni-hamburg.de>
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

#ifndef INTER_FEATURE_STREAM_H
#define INTER_FEATURE_STREAM_H

#include <stdio.h>
#include "extended/node_stream_api.h"

/* implements the ``genome_stream'' interface */
typedef struct GtInterFeatureStream GtInterFeatureStream;

const GtNodeStreamClass* gt_inter_feature_stream_class(void);
/* Adds features of type <inter_type> between features of type
   <outside_type>. */
GtNodeStream*            gt_inter_feature_stream_new(GtNodeStream*,
                                                     const char *outside_type,
                                                     const char *inter_type);

#endif
