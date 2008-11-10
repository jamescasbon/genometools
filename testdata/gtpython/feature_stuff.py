#!/usr/bin/env python
#
# Copyright (c) 2007 Gordon Gremme <gremme@zbh.uni-hamburg.de>
# Copyright (c) 2007 Center for Bioinformatics, University of Hamburg
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

from gt.core import *
from gt.extended import *
from gt.annotationsketch import *
import sys
import re

if __name__ == "__main__":
  if len(sys.argv) != 2:
    sys.stderr.write("Usage: " + sys.argv[0] + " GFF3_file\n")
    sys.stderr.write("Test the FeatureIndex and FeatureStream bindings on" \
                     "GFF3 file.")
    sys.exit(1)

  genome_stream = GFF3InStream(sys.argv[1])

  # try to instantiate an interface -> should fail
  try:
    feature_index = FeatureIndex()
  except NotImplementedError:
    pass
  else:
    raise

  # instantiate index object
  feature_index = FeatureIndexMemory()
  genome_stream = FeatureStream(genome_stream, feature_index)

  feature = genome_stream.next_tree()
  while feature:
    feature = genome_stream.next_tree()

  seqid = feature_index.get_first_seqid()
  features = feature_index.get_features_for_seqid(seqid)
  if not features:
    raise

  gff3_visitor = GFF3Visitor()

  for feature in features:
    feature.accept(gff3_visitor)
