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
from gt.core.error import gterror
from gt.core.str_array import StrArray
from gt.extended.genome_stream import GenomeStream

class GFF3InStream(GenomeStream):
  def __init__(self, filename):
    from ctypes import c_bool
    try:
      p = open(filename)
      tmp = p.readline()
      p.close()
    except:
      gterror("File " + filename + " not readable!")
    self.gs = gtlib.gt_gff3_in_stream_new_sorted(filename, c_bool(False))
    self._as_parameter_ = self.gs

  def get_used_types(self):
    str_array_ptr = gtlib.gt_gff3_in_stream_get_used_types(self.gs)
    used_types = StrArray(str_array_ptr)
    return used_types.to_list()

  def register(cls, gtlib):
    from ctypes import c_char_p, c_bool, c_void_p
    gtlib.gt_gff3_in_stream_new_sorted.argtypes = [c_char_p, c_bool]
    gtlib.gt_gff3_in_stream_get_used_types.restype = c_void_p
    gtlib.gt_gff3_in_stream_new_sorted.restype = c_void_p
  register = classmethod(register)
