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

from gt.dlload import gtlib


class GTError(Exception):

    pass


class Error:

    def __init__(self, ptr=None):
        if ptr:
            self.error = ptr
            self.own = False
        else:
            self.error = gtlib.gt_error_new()
            self.own = True
        self._as_parameter_ = self.error

    def __del__(self):
        if self.own:
            try:
                gtlib.gt_error_delete(self.error)
            except AttributeError:
                pass

    def from_param(cls, obj):
        if not isinstance(obj, Error):
            raise TypeError, "argument must be an Error"
        return obj._as_parameter_

    from_param = classmethod(from_param)

    def get(self):
        return gtlib.gt_error_get(self.error)

    def set(self, errmsg):
        return gtlib.gt_error_set(self.error, errmsg)

    def is_set(self):
        return gtlib.gt_error_is_set(self.error) == 1

    def unset(self):
        gtlib.gt_error_unset(self.error)

    def register(cls, gtlib):
        from ctypes import c_void_p, c_char_p, c_int
        gtlib.gt_error_new.restype = c_void_p
        gtlib.gt_error_get.restype = c_char_p
        gtlib.gt_error_is_set.restype = c_int
        gtlib.gt_error_get.argtypes = [c_void_p]
        gtlib.gt_error_set.argtypes = [c_void_p, c_char_p]
        gtlib.gt_error_is_set.argtypes = [c_void_p]
        gtlib.gt_error_unset.argtypes = [c_void_p]

    register = classmethod(register)


def gterror(err):
    if isinstance(err, Error):
        raise GTError, "GenomeTools error: " + err.get()
    else:
        raise GTError, "GenomeTools error: " + err


