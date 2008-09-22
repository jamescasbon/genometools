/*
  Copyright (c) 2006-2007 Gordon Gremme <gremme@zbh.uni-hamburg.de>
  Copyright (c) 2006-2007 Center for Bioinformatics, University of Hamburg

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

#ifndef NODE_STREAM_REP_H
#define NODE_STREAM_REP_H

#include <stdio.h>
#include "extended/node_stream.h"

typedef void (*GtNodeStreamFreeFunc)(GtNodeStream*);
typedef int  (*GtNodeStreamNextFunc)(GtNodeStream*, GtGenomeNode**, GtError*);

typedef struct GtNodeStreamMembers GtNodeStreamMembers;

struct GtNodeStream
{
  const GtNodeStreamClass *c_class;
  GtNodeStreamMembers *members;
};

const
GtNodeStreamClass* gt_node_stream_class_new(size_t size,
                                            GtNodeStreamFreeFunc,
                                            GtNodeStreamNextFunc);
GtNodeStream*      gt_node_stream_create(const GtNodeStreamClass*,
                                         bool ensure_sorting);
void*              gt_node_stream_cast(const GtNodeStreamClass*, GtNodeStream*);

#endif