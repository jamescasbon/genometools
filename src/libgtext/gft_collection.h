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

#ifndef GFT_COLLECTION_H
#define GFT_COLLECTION_H

#include "libgtcore/strarray.h"
#include "libgtext/genome_feature_type.h"

/* A genome feature type collection. */
typedef struct GFTCollection GFTCollection;

GFTCollection*     gft_collection_new(void);
void               gft_collection_delete(GFTCollection*);
/* Takes ownership of <gft>. */
void               gft_collection_add(GFTCollection*, const char *type,
                                      GenomeFeatureType *gft);
GenomeFeatureType* gft_collection_get(GFTCollection*, const char *type);
/* Returns a StrArray which contains all type names in alphabetical order which
   are stored in this colleciton. The caller is responsible to free it! */
StrArray*          gft_collection_get_types(const GFTCollection
                                            *gft_collection);

#endif