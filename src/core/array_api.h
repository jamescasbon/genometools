/*
  Copyright (c) 2005-2008 Gordon Gremme <gremme@zbh.uni-hamburg.de>
  Copyright (c) 2005-2008 Center for Bioinformatics, University of Hamburg

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

#ifndef ARRAY_API_H
#define ARRAY_API_H

#include <stdlib.h>
#include "core/fptr_api.h"

typedef struct GT_Array GT_Array;

GT_Array*     gt_array_new(size_t);
GT_Array*     gt_array_clone(const GT_Array*);
void*         gt_array_get(const GT_Array*, unsigned long);
void*         gt_array_get_first(const GT_Array*);
void*         gt_array_get_last(const GT_Array*);
void*         gt_array_pop(GT_Array*);
void*         gt_array_get_space(const GT_Array*);
#define       gt_array_add(a, elem)\
              gt_array_add_elem(a, &(elem), sizeof (elem))
void          gt_array_add_elem(GT_Array*, void*, size_t);
void          gt_array_add_array(GT_Array*, const GT_Array*);
void          gt_array_rem(GT_Array*, unsigned long); /* O(n) */
void          gt_array_reverse(GT_Array*);
void          gt_array_set_size(GT_Array*, unsigned long);
void          gt_array_reset(GT_Array*);
size_t        gt_array_elem_size(const GT_Array*);
unsigned long gt_array_size(const GT_Array*);
void          gt_array_sort(GT_Array*, GT_Compare compar);
/* GT_Compare the content of <array_a> with the content of <array_b>.
   <array_a> and <array_b> must have the same gt_array_size() and
   gt_array_elem_size(). */
int           gt_array_cmp(const GT_Array *array_a, const GT_Array *array_b);
void          gt_array_delete(GT_Array*);

#endif