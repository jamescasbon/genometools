/*
  Copyright (c) 2007      Christin Schaerfer <cschaerfer@zbh.uni-hamburg.de>
  Copyright (c)      2008 Sascha Steinbiss <ssteinbiss@zbh.uni-hamburg.de>
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

#include <string.h>
#include "libgtcore/ensure.h"
#include "libgtcore/log.h"
#include "libgtcore/ma.h"
#include "libgtext/feature_type_factory_builtin.h"
#include "libannotationsketch/block.h"
#include "libannotationsketch/element.h"

struct Block {
  Dlist *elements;
  Range range;
  Str *caption;
  bool show_caption;
  Strand strand;
  GenomeFeatureType *type;
  GenomeNode *top_level_feature;
  unsigned long reference_count;
};

/* Compare function used to insert Elements into dlist, order by type */
static int elemcmp(const void *a, const void *b)
{
  Element *elem_a = (Element*) a;
  Element *elem_b = (Element*) b;

  GenomeFeatureType *ta = element_get_type(elem_a);
  GenomeFeatureType *tb = element_get_type(elem_b);

  if (ta == tb)
    return 0;
  else if (strcmp(genome_feature_type_get_cstr(ta),
                  genome_feature_type_get_cstr(tb)) < 0) {
    return 1;
  }
  return -1;
}

int block_compare(const Block *block1, const Block *block2)
{
  int ret;
  assert(block1 && block2);
  ret = range_compare(block_get_range(block1), block_get_range(block2));
  if (ret == 0 && block1 != block2)
    ret = (block1 < block2 ? -1 : 1);
  return ret;
}

Block* block_ref(Block *block)
{
  assert(block);
  block->reference_count++;
  return block;
}

Block* block_new(void)
{
  Block *block = ma_calloc(1, sizeof (Block));
  block->elements = dlist_new(elemcmp);
  block->caption = NULL;
  block->show_caption = true;
  block->strand = STRAND_UNKNOWN;
  block->top_level_feature = NULL;
  return block;
}

Block* block_new_from_node(GenomeNode *node)
{
  Block *block;
  assert(node);
  block = block_new();
  block->range = genome_node_get_range(node);
  block->strand = genome_feature_get_strand((GenomeFeature*) node);
  block->type = genome_feature_get_type((GenomeFeature*) node);
  block->top_level_feature = genome_node_ref(node);
  return block;
}

void block_insert_element(Block *block, GenomeNode *gn)
{
  Element *element;
  assert(block && gn);
  if (!block->top_level_feature)
    block->top_level_feature = genome_node_ref(gn);
  element = element_new(gn);
  dlist_add(block->elements, element);
}

GenomeNode* block_get_top_level_feature(const Block *block)
{
  assert(block);
  return block->top_level_feature;
}

Range block_get_range(const Block *block)
{
   assert(block);
   return block->range;
}

Range* block_get_range_ptr(const Block *block)
{
   assert(block);
   return (Range*) &(block->range);
}

void block_set_range(Block *block, Range r)
{
  assert(block && r.start <= r.end);
  block->range = r;
}

bool block_has_only_one_fullsize_element(const Block *block)
{
  bool ret = false;
  assert(block);
  if (dlist_size(block->elements) == 1UL) {
    Range elem_range, block_range;
    assert(dlist_first(block->elements) == dlist_last(block->elements));
    elem_range = element_get_range(
                   dlistelem_get_data(dlist_first(block->elements)));
    block_range = block_get_range(block);
    ret = (range_compare(block_range, elem_range) == 0);
  }
  return ret;
}

void block_set_caption_visibility(Block *block, bool val)
{
  assert(block);
  block->show_caption = val;
}

bool block_caption_is_visible(const Block *block)
{
  assert(block);
  return (block->caption && block->show_caption);
}

void block_set_caption(Block *block, Str *caption)
{
  assert(block);
  block->caption = caption;
}

Str* block_get_caption(const Block *block)
{
  assert(block);
  return block->caption;
}

void block_set_strand(Block *block, Strand strand)
{
  assert(block);
  block->strand = strand;
}

Strand block_get_strand(const Block *block)
{
  assert(block);
  return block->strand;
}

void block_set_type(Block *block, GenomeFeatureType *type)
{
  assert(block);
  block->type = type;
}

GenomeFeatureType* block_get_type(const Block *block)
{
  assert(block);
  return block->type;
}

unsigned long block_get_size(const Block *block)
{
  assert(block && block->elements);
  return dlist_size(block->elements);
}

int block_render(Block *block, Canvas *canvas)
{
 int had_err = 0;
 Dlistelem *delem;
 assert(block && canvas);
 /* if resulting block was too short,
    do not traverse this feature tree further */
 if (-1 == canvas_visit_block(canvas, block))
   return had_err;
 for (delem = dlist_first(block->elements); delem;
      delem = dlistelem_next(delem)) {
    Element* elem = (Element*) dlistelem_get_data(delem);
    element_render(elem, canvas);
  }
  return had_err;
}

int block_unit_test(Error *err)
{
  FeatureTypeFactory *feature_type_factory;
  GenomeFeatureType *gft;
  Range r1, r2, r_temp, b_range;
  int had_err = 0;
  Strand s;
  GenomeNode *gn1, *gn2;
  Element *e1, *e2;
  Block * b;
  Str *seqid, *caption1, *caption2;
  error_check(err);

  feature_type_factory = feature_type_factory_builtin_new();
  seqid = str_new_cstr("seqid");
  caption1 = str_new_cstr("foo");
  caption2 = str_new_cstr("bar");

  r1.start = 10UL;
  r1.end = 50UL;

  r2.start = 40UL;
  r2.end = 50UL;

  gft = feature_type_factory_create_gft(feature_type_factory, gft_gene);
  gn1 = genome_feature_new(seqid, gft, r1, STRAND_FORWARD);
  gft = feature_type_factory_create_gft(feature_type_factory, gft_exon);
  gn2 = genome_feature_new(seqid, gft, r2, STRAND_FORWARD);

  e1 = element_new(gn1);
  e2 = element_new(gn2);

  b = block_new();

  /* test block_insert_elements */
  ensure(had_err, (0UL == block_get_size(b)));
  block_insert_element(b, gn1);
  ensure(had_err, (1UL == block_get_size(b)));
  block_insert_element(b, gn2);
  ensure(had_err, (2UL == block_get_size(b)));

  /* test block_set_range & block_get_range */
  r_temp = range_join(r1, r2);
  block_set_range(b, r_temp);
  b_range = block_get_range(b);
  ensure(had_err, (0 == range_compare(b_range, r_temp)));
  ensure(had_err, (1 == range_compare(r2, r_temp)));

  /* tests block_set_caption & block_get_caption */
  block_set_caption(b, caption1);
  ensure(had_err, (0 == str_cmp(block_get_caption(b), caption1)));
  ensure(had_err, (0 != str_cmp(block_get_caption(b), caption2)));

  /* tests block_set_strand & block_get_range */
  s = block_get_strand(b);
  ensure(had_err, (STRAND_UNKNOWN == s));
  block_set_strand(b, STRAND_FORWARD);
  s = block_get_strand(b);
  ensure(had_err, (STRAND_FORWARD == s));

  str_delete(caption2);
  str_delete(seqid);
  element_delete(e1);
  element_delete(e2);
  block_delete(b);
  genome_node_delete(gn1);
  genome_node_delete(gn2);
  feature_type_factory_delete(feature_type_factory);

  return had_err;
}

void block_delete(Block *block)
{
  Dlistelem *delem;
  if (!block) return;
  if (block->reference_count) {
    block->reference_count--;
    return;
  }
  for (delem = dlist_first(block->elements); delem;
       delem = dlistelem_next(delem)) {
    Element* elem = (Element*) dlistelem_get_data(delem);
    element_delete(elem);
  }
  if (block->caption)
    str_delete(block->caption);
  dlist_delete(block->elements);
  if (block->top_level_feature)
    genome_node_delete(block->top_level_feature);
  ma_free(block);
}