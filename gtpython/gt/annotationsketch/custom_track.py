#!/usr/bin/python
# -*- coding: utf-8 -*-
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

from ctypes import CFUNCTYPE, c_char_p, c_void_p, c_int, c_ulong
from gt.dlload import gtlib
from gt.annotationsketch.color import Color
from gt.annotationsketch.style import Style
from gt.annotationsketch.graphics import Graphics
from gt.core.error import Error, gterror
from gt.core.gtrange import Range

RenderFunc = CFUNCTYPE(c_int, c_void_p, c_ulong, Range, c_void_p,
                       c_void_p)
TitleFunc = CFUNCTYPE(c_char_p, c_void_p)
HeightFunc = CFUNCTYPE(c_ulong, c_void_p)
FreeFunc = CFUNCTYPE(c_void_p, c_void_p)


class CustomTrack(object):

    def __init__(self):

    # create callbacks to C script wrapper

        def get_title_w(ptr):
            return self.get_title()

        self.get_title_cb = TitleFunc(get_title_w)

        def get_height_w(ptr):
            return self.get_height()

        self.get_height_cb = HeightFunc(get_height_w)

        def render_w(graphics, ypos, rng, sty, err):
            g = Graphics(graphics)
            s = Style(sty)
            e = Error(err)
            return self.render(g, ypos, rng, s, e)

        self.render_cb = RenderFunc(render_w)

        def free_w(ptr):
            self.free()

        self.free_cb = FreeFunc(free_w)
        self.ctt = gtlib.gt_custom_track_script_wrapper_new(self.render_cb,
                self.get_height_cb, self.get_title_cb, self.free_cb)
        self._as_parameter_ = self.ctt

    def __del__(self):
        try:
            gtlib.gt_custom_track_delete(self.ctt)
        except AttributeError:
            pass

    def from_param(cls, obj):
        if not isinstance(obj, CustomTrack):
            raise TypeError, "argument must be a CustomTrack"
        return obj._as_parameter_

    from_param = classmethod(from_param)

    def register(cls, gtlib):
        from ctypes import c_char_p, c_void_p, c_int, POINTER
        gtlib.gt_custom_track_script_wrapper_new.restype = c_void_p
        gtlib.gt_custom_track_script_wrapper_new.argtypes = [RenderFunc,
                HeightFunc, TitleFunc, FreeFunc]

    register = classmethod(register)


