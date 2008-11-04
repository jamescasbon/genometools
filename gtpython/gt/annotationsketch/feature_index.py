#
# Copyright (c) 2008 Sascha Steinbiss <steinbiss@zbh.uni-hamburg.de>
# Copyright (c) 2008 Center for Bioinformatics, University of Hamburg
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

from gt.dlload import gtlib
from gt.core.array import Array
from gt.core.error import Error, gterror
from gt.core.range import Range
from gt.core.str_array import StrArray
from gt.extended.genome_node import GenomeNode

class FeatureIndex:
  def __init__(self, *args):
    raise NotImplementedError, "Please call the constructor of a " \
                               "FeatureIndex implementation."

  def from_param(self):
    return self._as_parameter_

  def get_features_for_seqid(self, seqid):
    rval = gtlib.gt_feature_index_get_features_for_seqid(self.fi, seqid)
    if rval:
      a = Array(rval)
      result = []
      for i in range(a.size()):
        result.append(GenomeNode(gtlib.gt_genome_node_rec_ref(a.get(i))))
      return result
    else:
      return None

  def add_gff3file(self, filename):
    err = Error()
    rval = gtlib.gt_feature_index_add_gff3file(self.fi, filename, err)
    if rval != 0:
      gterror(err)

  def get_first_seqid(self):
    return gtlib.gt_feature_index_get_first_seqid(self.fi)

  def get_seqids(self):
    result = []
    stra = StrArray(gtlib.gt_feature_index_get_seqids(self.fi))
    for i in range(stra.size()):
        result.append(stra.get(i))
    return result

  def get_range_for_seqid(self, seqid):
    from ctypes import byref
    if not gtlib.gt_feature_index_has_seqid(self.fi, seqid):
      gterror("feature_index does not contain seqid")
    range = Range()
    gtlib.gt_feature_index_get_range_for_seqid(self.fi, byref(range), seqid)
    return range

  def register(cls, gtlib):
    from ctypes import c_char_p, c_void_p, c_bool
    gtlib.gt_feature_index_get_features_for_seqid.restype = c_void_p
    gtlib.gt_feature_index_add_gff3file.argtypes = [FeatureIndex, c_char_p, \
                                                    Error]
    gtlib.gt_feature_index_get_first_seqid.restype = c_char_p
    gtlib.gt_feature_index_get_seqids.restype = c_void_p
    gtlib.gt_feature_index_has_seqid.argtypes = [c_void_p, c_char_p]
    gtlib.gt_feature_index_has_seqid.restype = c_bool
    gtlib.gt_feature_index_get_range_for_seqid.argtypes = [c_void_p, \
                                                           c_void_p, \
                                                           c_char_p]
  register = classmethod(register)


class FeatureIndexMemory(FeatureIndex):
  def __init__(self):
    self.fi = gtlib.gt_feature_index_memory_new()
    self._as_parameter_ = self.fi

  def __del__(self):
    gtlib.gt_feature_index_delete(self.fi)