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

#ifndef GFF3_VISITOR_H
#define GFF3_VISITOR_H

/* implements the ``genome visitor'' interface */
typedef struct GtGFF3Visitor GtGFF3Visitor;

#include "extended/node_visitor.h"

const GtNodeVisitorClass* gt_gff3_visitor_class(void);
GtNodeVisitor*            gt_gff3_visitor_new(GtFile*);
void                      gt_gff3_visitor_set_fasta_width(GtNodeVisitor*,
                                                          unsigned long);
void                      gt_gff3_visitor_retain_id_attributes(GtNodeVisitor *);

#endif
