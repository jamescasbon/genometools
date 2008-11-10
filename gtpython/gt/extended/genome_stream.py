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
from gt.core.error import Error, gterror
from gt.extended.genome_node import GenomeNode


class GenomeStream:
  def __init__(self, *args):
    raise NotImplementedError, "Please call the constructor of a " \
                               "GenomeStream implementation."

  def __del__(self):
    try:
      gtlib.gt_node_stream_delete(self.gs)
    except AttributeError:
      pass

  def next_tree(self):
    from ctypes import byref, c_void_p
    err = Error()
    genome_node = c_void_p()
    rval = gtlib.gt_node_stream_next(self.gs, byref(genome_node), err)
    if rval != 0:
      gterror(err)
    if genome_node.value == None:
      return None
    else:
      return GenomeNode(genome_node.value)
