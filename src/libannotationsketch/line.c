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

#include "libgtcore/ensure.h"
#include "libgtcore/interval_tree.h"
#include "libgtcore/ma.h"
#include "libgtcore/range.h"
#include "libgtext/feature_type_factory_builtin.h"
#include "libannotationsketch/block.h"
#include "libannotationsketch/line.h"
#include "libannotationsketch/style.h"

struct Line {
  bool has_captions;
  Array *blocks;
};

Line* line_new(void)
{
  Line *line;
  line = ma_malloc(sizeof (Line));
  line->blocks = array_new(sizeof (Block*));
  line->has_captions = false;
  return line;
}

void line_insert_block(Line *line, Block *block)
{
  assert(line && block);
  if (!line->has_captions && block_get_caption(block) != NULL)
    line->has_captions = true;
  array_add(line->blocks, block);
}

bool line_has_captions(const Line *line)
{
  assert(line);
  return line->has_captions;
}

Array* line_get_blocks(Line* line)
{
  assert(line);
  return line->blocks;
}

int line_render(Line *line, Canvas *canvas)
{
  int i = 0;
  assert(line && canvas);
  canvas_visit_line_pre(canvas, line);
  for (i = 0; i < array_size(line->blocks); i++) {
    Block *block;
    block = *(Block**) array_get(line->blocks, i);
    block_render(block, canvas);
  }
  canvas_visit_line_post(canvas, line);
  return 0;
}

int line_unit_test(Error *err)
{
  FeatureTypeFactory *feature_type_factory;
  GenomeFeatureType *type;
  Range r1, r2, r3, r4, r_parent;
  Array* blocks;
  Str *seqid1, *seqid2, *seqid3;
  int had_err = 0;
  GenomeNode *parent, *gn1, *gn2, *gn3, *gn4;
  Line *l1, *l2;
  Block *b1, *b2;
  error_check(err);

  const char* foo = "foo";
  const char* bar = "bar";
  const char* blub = "blub";

  feature_type_factory = feature_type_factory_builtin_new();

  r_parent.start = 10UL;
  r_parent.end = 80UL;

  r1.start = 10UL;
  r1.end = 50UL;

  r2.start = 51UL;
  r2.end = 80UL;

  r3.start = 70UL;
  r3.end = 100UL;

  r4.start = 10UL;
  r4.end = 20UL;

  seqid1 = str_new_cstr("test1");
  seqid2 = str_new_cstr("test2");
  seqid3 = str_new_cstr("foo");

  type = feature_type_factory_create_gft(feature_type_factory, gft_gene);
  parent = genome_feature_new(seqid1, type, r_parent, STRAND_FORWARD);
  type = feature_type_factory_create_gft(feature_type_factory, gft_exon);
  gn1 = genome_feature_new(seqid3, type, r1, STRAND_FORWARD);
  gn2 = genome_feature_new(seqid3, type, r2, STRAND_FORWARD);
  gn3 = genome_feature_new(seqid2, type, r3, STRAND_FORWARD);
  type = feature_type_factory_create_gft(feature_type_factory,
                                         gft_TF_binding_site);
  gn4 = genome_feature_new(seqid3, type, r4, STRAND_FORWARD);

  l1 = line_new();
  l2 = line_new();

  genome_feature_add_attribute((GenomeFeature*) parent, "Name", foo);
  genome_feature_add_attribute((GenomeFeature*) gn1, "Name", bar);
  genome_feature_add_attribute((GenomeFeature*) gn2, "Name", bar);
  genome_feature_add_attribute((GenomeFeature*) gn3, "Name", blub);
  genome_feature_add_attribute((GenomeFeature*) gn4, "Name", bar);

  b1 = block_new();
  b2 = block_new();

  block_insert_element(b1, gn1);
  block_insert_element(b2, gn2);
  block_set_range(b1, r1);
  block_set_range(b2, r2);

  /* test line_insert_block */
  ensure(had_err,  (0 == array_size(line_get_blocks(l1))));
  line_insert_block(l1, b1);
  ensure(had_err,  (1 == array_size(line_get_blocks(l1))));
  line_insert_block(l1, b2);
  ensure(had_err,  (2 == array_size(line_get_blocks(l1))));

  /* test line_get_blocks */
  blocks = line_get_blocks(l1);
  ensure(had_err, (2 == array_size(blocks)));

  str_delete(seqid1);
  str_delete(seqid2);
  str_delete(seqid3);
  line_delete(l1);
  line_delete(l2);
  genome_node_delete(parent);
  genome_node_delete(gn1);
  genome_node_delete(gn2);
  genome_node_delete(gn3);
  genome_node_delete(gn4);
  feature_type_factory_delete(feature_type_factory);

  return had_err;
}

void line_delete(Line *line)
{
  unsigned long i;
  if (!line) return;
  for (i = 0; i < array_size(line->blocks); i++)
    block_delete(*(Block**) array_get(line->blocks, i));
  array_delete(line->blocks);
  ma_free(line);
}