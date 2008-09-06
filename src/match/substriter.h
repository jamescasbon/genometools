/*
  Copyright (c) 2007 Stefan Kurtz <kurtz@zbh.uni-hamburg.de>
  Copyright (c) 2007 Center for Bioinformatics, University of Hamburg

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

#ifndef SUBSTRITER_H
#define SUBSTRITER_H

#include <inttypes.h>
#include <stdbool.h>
#include "core/strarray.h"
#include "core/symboldef.h"
#include "intcode-def.h"
#include "alphadef.h"

typedef struct Substriter Substriter;

Substriter *substriter_new(const Alphabet *alphabet,unsigned int qvalue);

void substriter_init(Substriter *substriter,const Uchar *start,
                     unsigned long len);

int substriter_next(Substriter *substriter);

void substriter_delete(Substriter **substriter);

#endif