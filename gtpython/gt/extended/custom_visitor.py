#!/usr/bin/python
# -*- coding: utf-8 -*-
#
# Copyright (c) 2009 Sascha Steinbiss <steinbiss@zbh.uni-hamburg.de>
# Copyright (c) 2009 Center for Bioinformatics, University of Hamburg
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
from gt.extended.node_visitor import NodeVisitor
from ctypes import CFUNCTYPE, c_void_p, c_int, POINTER
from gt.extended.feature_node import FeatureNode
from gt.extended.region_node import RegionNode
from gt.extended.sequence_node import SequenceNode
from gt.extended.comment_node import CommentNode
from gt.core.error import Error, GTError

CommentNodeFunc = CFUNCTYPE(c_int, c_void_p, c_void_p)
FeatureNodeFunc = CFUNCTYPE(c_int, c_void_p, c_void_p)
RegionNodeFunc = CFUNCTYPE(c_int, c_void_p, c_void_p)
SequenceNodeFunc = CFUNCTYPE(c_int, c_void_p, c_void_p)
FreeFunc = CFUNCTYPE(c_void_p, c_void_p)

class CustomVisitor(NodeVisitor):

    def __init__(self):
        def feature_node_w(fn_p, err_p):
            fn = FeatureNode.create_from_ptr(fn_p)
            err = Error(err_p)
            try:
                try:
                    self.visit_feature_node(fn)
                except AttributeError:
                    pass
                return 0
            except GTError as errmsg:
                err.set(str(errmsg))
                return -1

        self.feature_node_cb = FeatureNodeFunc(feature_node_w)

        def sequence_node_w(sn_p, err_p):
            sn = SequenceNode.create_from_ptr(sn_p)
            err = Error(err_p)
            try:
                try:
                    self.visit_sequence_node(sn)
                except AttributeError:
                    pass
                return 0
            except GTError as errmsg:
                err.set(str(errmsg))
                return -1

        self.sequence_node_cb = SequenceNodeFunc(sequence_node_w)

        def region_node_w(rn_p, err_p):
            rn = RegionNode.create_from_ptr(rn_p)
            err = Error(err_p)
            try:
                try:
                    self.visit_region_node(rn)
                except AttributeError:
                    pass
                return 0
            except GTError as errmsg:
                err.set(str(errmsg))
                return -1

        self.region_node_cb = RegionNodeFunc(region_node_w)

        def comment_node_w(cn_p, err_p):
            cn = CommentNode.create_from_ptr(cn_p)
            err = Error(err_p)
            try:
                try:
                    self.visit_comment_node(cn)
                except AttributeError:
                    pass
                return 0
            except GTError as errmsg:
                err.set(str(errmsg))
                return -1

        self.comment_node_cb = CommentNodeFunc(comment_node_w)

        self.gv = gtlib.gt_script_wrapper_visitor_new(self.comment_node_cb, \
                                                      self.feature_node_cb, \
                                                      self.region_node_cb, \
                                                      self.sequence_node_cb, \
                                                      None)
        self._as_parameter_ = self.gv

