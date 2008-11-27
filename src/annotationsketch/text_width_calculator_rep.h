/*
  Copyright (c) 2008 Sascha Steinbiss <steinbiss@zbh.uni-hamburg.de>
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

#ifndef TEXT_WIDTH_CALCULATOR_REP_H
#define TEXT_WIDTH_CALCULATOR_REP_H

#include <stdio.h>
#include "annotationsketch/text_width_calculator.h"

typedef double (*TextWidthCalculatorGetTextWidth)(GtTextWidthCalculator*,
                                                  const char*);
typedef void (*TextWidthCalculatorFreeFunc)(GtTextWidthCalculator*);

typedef struct GtTextWidthCalculatorMembers GtTextWidthCalculatorMembers;

struct GtTextWidthCalculator {
  const GtTextWidthCalculatorClass *c_class;
  GtTextWidthCalculatorMembers *pvt;
};

const GtTextWidthCalculatorClass*
gt_text_width_calculator_class_new(size_t size,
                                   TextWidthCalculatorGetTextWidth,
                                   TextWidthCalculatorFreeFunc);

GtTextWidthCalculator*
gt_text_width_calculator_create(const GtTextWidthCalculatorClass*);

void*
gt_line_breaker_cast(const GtTextWidthCalculatorClass*, GtTextWidthCalculator*);

#endif