/*
  Copyright (c) 2009 Gordon Gremme <gremme@zbh.uni-hamburg.de>

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

#ifndef DUP_FEATURE_STREAM_H
#define DUP_FEATURE_STREAM_H

#include <stdio.h>
#include "extended/node_stream_api.h"

/* implements the ``genome_stream'' interface */
typedef struct GtDupFeatureStream GtDupFeatureStream;

const GtNodeStreamClass* gt_dup_feature_stream_class(void);
/* Duplicate internal feature nodes of type <source_type> as features with type
   <dest_type>. The duplicated features does not inherit the children. */
GtNodeStream*            gt_dup_feature_stream_new(GtNodeStream*,
                                                   const char *dest_type,
                                                   const char *source_type);

#endif
