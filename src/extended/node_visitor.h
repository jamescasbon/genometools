/*
  Copyright (c) 2006-2008 Gordon Gremme <gremme@zbh.uni-hamburg.de>
  Copyright (c) 2006-2008 Center for Bioinformatics, University of Hamburg

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

#ifndef NODE_VISITOR_H
#define NODE_VISITOR_H

/* the ``genome visitor'' interface, a visitor for genome nodes */
typedef struct GtNodeVisitorClass GtNodeVisitorClass;
typedef struct GtNodeVisitor GtNodeVisitor;

#include "extended/comment_node_api.h"
#include "extended/feature_node.h"
#include "extended/region_node.h"
#include "extended/sequence_node_api.h"

int  gt_node_visitor_visit_comment_node(GtNodeVisitor*, GtCommentNode*,
                                        GtError*);
int  gt_node_visitor_visit_feature_node(GtNodeVisitor*, GtFeatureNode*,
                                        GtError*);
int  gt_node_visitor_visit_region_node(GtNodeVisitor*, GtRegionNode*, GtError*);
int  gt_node_visitor_visit_sequence_node(GtNodeVisitor*, GtSequenceNode*,
                                         GtError*);
void gt_node_visitor_delete(GtNodeVisitor *gv);

#endif
