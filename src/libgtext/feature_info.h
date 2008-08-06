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

#ifndef FEATURE_INFO_H
#define FEATURE_INFO_H

#include "libgtext/genome_node.h"

typedef struct FeatureInfo FeatureInfo;

FeatureInfo* feature_info_new();
void         feature_info_delete(FeatureInfo*);
void         feature_info_reset(FeatureInfo*);
GenomeNode*  feature_info_get(const FeatureInfo*, const char *id);
void         feature_info_add(FeatureInfo*, const char *id, GenomeNode*);
GenomeNode*  feature_info_get_pseudo_parent(const FeatureInfo*, const char *id);
void         feature_info_add_pseudo_parent(FeatureInfo*, const char *id,
                                            GenomeNode *pseudo_parent);
GenomeNode*  feature_info_find_root(const FeatureInfo*, const char *id);

#endif