/*
  Copyright (c) 2006-2009 Gordon Gremme <gremme@zbh.uni-hamburg.de>
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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "core/assert_api.h"
#include "core/fasta.h"
#include "core/hashmap.h"
#include "core/ma.h"
#include "core/unused_api.h"
#include "core/string_distri.h"
#include "core/cstr_table.h"
#include "core/str_api.h"
#include "core/warning_api.h"
#include "extended/genome_node.h"
#include "extended/gff3_output.h"
#include "extended/gff3_parser.h"
#include "extended/gff3_visitor.h"
#include "extended/node_visitor_rep.h"

struct GtGFF3Visitor {
  const GtNodeVisitor parent_instance;
  bool version_string_shown,
       retain_ids,
       fasta_directive_shown;
  GtStringDistri *id_counter;
  GtHashmap *feature_node_to_id_array,
            *feature_node_to_unique_id_str;
  unsigned long fasta_width;
  GtFile *outfp;
  GtCstrTable *used_ids;
};

typedef struct {
  GtHashmap *gt_feature_node_to_id_array;
  const char *id;
} AddIDInfo;

typedef struct {
  bool *attribute_shown;
  GtFile *outfp;
} ShowAttributeInfo;

#define gff3_visitor_cast(GV)\
        gt_node_visitor_cast(gt_gff3_visitor_class(), GV)

static void gff3_version_string(GtNodeVisitor *gv)
{
  GtGFF3Visitor *gff3_visitor = gff3_visitor_cast(gv);
  gt_assert(gff3_visitor);
  if (!gff3_visitor->version_string_shown) {
    gt_file_xprintf(gff3_visitor->outfp, "%s   %u\n", GFF_VERSION_PREFIX,
                       GFF_VERSION);
    gff3_visitor->version_string_shown = true;
  }
}

static void gff3_visitor_free(GtNodeVisitor *gv)
{
  GtGFF3Visitor *gff3_visitor = gff3_visitor_cast(gv);
  gt_assert(gff3_visitor);
  gt_string_distri_delete(gff3_visitor->id_counter);
  gt_hashmap_delete(gff3_visitor->feature_node_to_id_array);
  gt_hashmap_delete(gff3_visitor->feature_node_to_unique_id_str);
  gt_cstr_table_delete(gff3_visitor->used_ids);
}

static int gff3_visitor_comment_node(GtNodeVisitor *gv, GtCommentNode *cn,
                                     GT_UNUSED GtError *err)
{
  GtGFF3Visitor *gff3_visitor;
  gt_error_check(err);
  gff3_visitor = gff3_visitor_cast(gv);
  gt_assert(gv && cn);
  gff3_version_string(gv);
  gt_file_xprintf(gff3_visitor->outfp, "#%s\n",
                     gt_comment_node_get_comment(cn));
  return 0;
}

static int add_id(GtGenomeNode *gn, void *data, GT_UNUSED GtError *err)
{
  AddIDInfo *info = (AddIDInfo*) data;
  GtArray *parent_features = NULL;
  gt_error_check(err);
  gt_assert(gn && info && info->gt_feature_node_to_id_array && info->id);
  parent_features = gt_hashmap_get(info->gt_feature_node_to_id_array, gn);
  if (!parent_features) {
    parent_features = gt_array_new(sizeof (char*));
    gt_hashmap_add(info->gt_feature_node_to_id_array, gn, parent_features);
  }
  gt_array_add(parent_features, info->id);
  return 0;
}

static void show_attribute(const char *attr_name, const char *attr_value,
                           void *data)
{
  ShowAttributeInfo *info = (ShowAttributeInfo*) data;
  gt_assert(attr_name && attr_value && info);
  if (strcmp(attr_name, ID_STRING) && strcmp(attr_name, PARENT_STRING)) {
    if (*info->attribute_shown)
      gt_file_xfputc(';', info->outfp);
    else
      *info->attribute_shown = true;
    gt_file_xprintf(info->outfp, "%s=%s", attr_name, attr_value);
  }
}

static int gff3_show_feature_node(GtGenomeNode *gn, void *data,
                                  GT_UNUSED GtError *err)
{
  bool part_shown = false;
  GtGFF3Visitor *gff3_visitor = (GtGFF3Visitor*) data;
  GtFeatureNode *fn = (GtFeatureNode*) gn;
  GtArray *parent_features = NULL;
  ShowAttributeInfo info;
  unsigned long i;
  GtStr *id;

  gt_error_check(err);
  gt_assert(gn && fn && gff3_visitor);

  /* output leading part */
  gt_gff3_output_leading(fn, gff3_visitor->outfp);

  /* show unique id part of attributes */
  if ((id = gt_hashmap_get(gff3_visitor->feature_node_to_unique_id_str, gn))) {
    gt_file_xprintf(gff3_visitor->outfp, "%s=%s", ID_STRING, gt_str_get(id));
    part_shown = true;
  }

  /* show parent part of attributes */
  parent_features = gt_hashmap_get(gff3_visitor->feature_node_to_id_array, gn);
  if (gt_array_size(parent_features)) {
    if (part_shown)
      gt_file_xfputc(';', gff3_visitor->outfp);
    gt_file_xprintf(gff3_visitor->outfp, "%s=", PARENT_STRING);
    for (i = 0; i < gt_array_size(parent_features); i++) {
      if (i)
        gt_file_xfputc(',', gff3_visitor->outfp);
      gt_file_xprintf(gff3_visitor->outfp, "%s",
                         *(char**) gt_array_get(parent_features, i));
    }
    part_shown = true;
  }

  /* show missing part of attributes */
  info.attribute_shown = &part_shown;
  info.outfp = gff3_visitor->outfp;
  gt_feature_node_foreach_attribute(fn, show_attribute, &info);

  /* show dot if no attributes have been shown */
  if (!part_shown)
    gt_file_xfputc('.', gff3_visitor->outfp);

  /* show terminal newline */
  gt_file_xfputc('\n', gff3_visitor->outfp);

  return 0;
}

static GtStr* create_unique_id(GtGFF3Visitor *gff3_visitor, GtFeatureNode *fn)
{
  const char *type;
  GtStr *id;
  gt_assert(gff3_visitor && fn);
  type = gt_feature_node_get_type(fn);

  /* increase id counter */
  gt_string_distri_add(gff3_visitor->id_counter, type);

  /* build id string */
  id = gt_str_new_cstr(type);
  gt_str_append_ulong(id, gt_string_distri_get(gff3_visitor->id_counter, type));

  /* store (unique) id */
  gt_hashmap_add(gff3_visitor->feature_node_to_unique_id_str, fn, id);

  return id;
}

static void make_unique_id_string(GtStr *current_id, unsigned long counter)
{
  /* name => name.1 */
  gt_str_append_char(current_id, '.');
  gt_str_append_ulong(current_id, counter);
}

static bool id_string_is_unique(GtStr *id, GtStr *buf, GtCstrTable *tab,
                                unsigned long i)
{
  gt_str_reset(buf);
  gt_str_append_str(buf, id);
  make_unique_id_string(buf, i);
  return !gt_cstr_table_get(tab, gt_str_get(buf));
}
static GtStr* make_id_unique(GtGFF3Visitor *gff3_visitor, GtFeatureNode *fn)
{
  unsigned long i = 1;
  GtStr *id = gt_str_new_cstr(gt_feature_node_get_attribute(fn, "ID"));

  if (gt_cstr_table_get(gff3_visitor->used_ids, gt_str_get(id))) {
    GtStr *buf = gt_str_new();
    while (!id_string_is_unique(id, buf, gff3_visitor->used_ids, i++));
    gt_warning("feature ID \"%s\" not unique: changing to %s", gt_str_get(id),
                                                               gt_str_get(buf));
    gt_str_set(id, gt_str_get(buf));
    gt_str_delete(buf);
  }
  /* update table with the new id */
  gt_cstr_table_add(gff3_visitor->used_ids, gt_str_get(id));
  /* store (unique) id */
  gt_hashmap_add(gff3_visitor->feature_node_to_unique_id_str, fn, id);

  return id;
}

static int store_ids(GtGenomeNode *gn, void *data, GtError *err)
{
  GtGFF3Visitor *gff3_visitor = (GtGFF3Visitor*) data;
  GtFeatureNode *fn = (GtFeatureNode*) gn;
  AddIDInfo add_id_info;
  int had_err = 0;
  GtStr *id;

  gt_error_check(err);
  gt_assert(gn && fn && gff3_visitor);

  if (gt_genome_node_has_children(gn) || gt_feature_node_is_multi(fn) ||
      (gff3_visitor->retain_ids && gt_feature_node_get_attribute(fn, "ID"))) {
    if (gt_feature_node_is_multi(fn)) {
      id = gt_hashmap_get(gff3_visitor->feature_node_to_unique_id_str,
                          gt_feature_node_get_multi_representative(fn));
      if (!id) { /* the representative does not have its own id */
        if (gff3_visitor->retain_ids)
          id = make_id_unique(gff3_visitor, fn);
        else {
          id = create_unique_id(gff3_visitor,
                                gt_feature_node_get_multi_representative(fn));
        }
      }
      if (gt_feature_node_get_multi_representative(fn) != fn) {
        gt_hashmap_add(gff3_visitor->feature_node_to_unique_id_str, fn,
                       gt_str_ref(id));
      }
    }
    else {
      if (gff3_visitor->retain_ids)
        id = make_id_unique(gff3_visitor, fn);
      else
        id = create_unique_id(gff3_visitor, fn);
    }
    /* for each child -> store the parent feature in the hash map */
    add_id_info.gt_feature_node_to_id_array =
      gff3_visitor->feature_node_to_id_array,
    add_id_info.id = gt_str_get(id);
    had_err = gt_genome_node_traverse_direct_children(gn, &add_id_info, add_id,
                                                      err);
  }
  return had_err;
}

static int gff3_visitor_feature_node(GtNodeVisitor *gv, GtFeatureNode *fn,
                                     GtError *err)
{
  GtGFF3Visitor *gff3_visitor;
  int had_err;
  gt_error_check(err);
  gff3_visitor = gff3_visitor_cast(gv);

  gff3_version_string(gv);

  had_err = gt_genome_node_traverse_children((GtGenomeNode*) fn, gff3_visitor,
                                             store_ids, true, err);
  if (!had_err) {
    if (gt_genome_node_is_tree((GtGenomeNode*) fn)) {
      had_err = gt_genome_node_traverse_children((GtGenomeNode*) fn,
                                                 gff3_visitor,
                                                 gff3_show_feature_node, true,
                                                 err);
    }
    else {
      /* got a DAG -> traverse bin breadth first fashion to make sure that the
         'Parent' attributes are shown in correct order */
      had_err = gt_genome_node_traverse_children_breadth((GtGenomeNode*) fn,
                                                         gff3_visitor,
                                                         gff3_show_feature_node,
                                                         true, err);
    }
  }

  /* reset hashmaps */
  gt_hashmap_reset(gff3_visitor->feature_node_to_id_array);
  gt_hashmap_reset(gff3_visitor->feature_node_to_unique_id_str);

  /* show terminator, if the feature has children (otherwise it is clear that
     the feature is complete, because no ID attribute has been shown) */
  if (gt_genome_node_has_children((GtGenomeNode*) fn) ||
      (gff3_visitor->retain_ids && gt_feature_node_get_attribute(fn, "ID"))) {
    gt_file_xprintf(gff3_visitor->outfp, "%s\n", GFF_TERMINATOR);
  }

  return had_err;
}

static int gff3_visitor_region_node(GtNodeVisitor *gv, GtRegionNode *rn,
                                    GT_UNUSED GtError *err)
{
  GtGFF3Visitor *gff3_visitor;
  gt_error_check(err);
  gff3_visitor = gff3_visitor_cast(gv);
  gt_assert(gv && rn);
  gff3_version_string(gv);
  gt_file_xprintf(gff3_visitor->outfp, "%s   %s %lu %lu\n",
                     GFF_SEQUENCE_REGION,
                     gt_str_get(gt_genome_node_get_seqid((GtGenomeNode*) rn)),
                     gt_genome_node_get_start((GtGenomeNode*) rn),
                     gt_genome_node_get_end((GtGenomeNode*) rn));
  return 0;
}

static int gff3_visitor_sequence_node(GtNodeVisitor *gv, GtSequenceNode *sn,
                                      GT_UNUSED GtError *err)
{
  GtGFF3Visitor *gff3_visitor;
  gt_error_check(err);
  gff3_visitor = gff3_visitor_cast(gv);
  gt_assert(gv && sn);
  if (!gff3_visitor->fasta_directive_shown) {
    gt_file_xprintf(gff3_visitor->outfp, "%s\n", GFF_FASTA_DIRECTIVE);
    gff3_visitor->fasta_directive_shown = true;
  }
  gt_fasta_show_entry_generic(gt_sequence_node_get_description(sn),
                              gt_sequence_node_get_sequence(sn),
                              gt_sequence_node_get_sequence_length(sn),
                              gff3_visitor->fasta_width, gff3_visitor->outfp);
  return 0;
}

const GtNodeVisitorClass* gt_gff3_visitor_class()
{
  static const GtNodeVisitorClass *gvc = NULL;
  if (!gvc) {
    gvc = gt_node_visitor_class_new(sizeof (GtGFF3Visitor),
                                    gff3_visitor_free,
                                    gff3_visitor_comment_node,
                                    gff3_visitor_feature_node,
                                    gff3_visitor_region_node,
                                    gff3_visitor_sequence_node);
  }
  return gvc;
}

GtNodeVisitor* gt_gff3_visitor_new(GtFile *outfp)
{
  GtNodeVisitor *gv = gt_node_visitor_create(gt_gff3_visitor_class());
  GtGFF3Visitor *gff3_visitor = gff3_visitor_cast(gv);
  gff3_visitor->version_string_shown = false;
  gff3_visitor->fasta_directive_shown = false;
  gff3_visitor->id_counter = gt_string_distri_new();
  gff3_visitor->feature_node_to_id_array =
    gt_hashmap_new(HASH_DIRECT, NULL, (GtFree) gt_array_delete);
  gff3_visitor->feature_node_to_unique_id_str =
    gt_hashmap_new(HASH_DIRECT, NULL, (GtFree) gt_str_delete);
  gff3_visitor->fasta_width = 0;
  gff3_visitor->outfp = outfp;
  gff3_visitor->used_ids = gt_cstr_table_new();
  gff3_visitor->retain_ids = false;
  return gv;
}

void gt_gff3_visitor_retain_id_attributes(GtNodeVisitor *gv)
{
    GtGFF3Visitor *gff3_visitor = gff3_visitor_cast(gv);
    gff3_visitor->retain_ids = true;
}

void gt_gff3_visitor_set_fasta_width(GtNodeVisitor *gv,
                                     unsigned long fasta_width)
{
  GtGFF3Visitor *gff3_visitor = gff3_visitor_cast(gv);
  gt_assert(gv);
  gff3_visitor->fasta_width = fasta_width;
}
