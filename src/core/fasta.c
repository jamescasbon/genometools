/*
  Copyright (c) 2006-2008 Gordon Gremme <gremme@zbh.uni-hamburg.de>
  Copyright (c) 2006-2008 Center for Bioinformatics, University of Hamburg

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

#include "core/fasta.h"
#include "core/fasta_separator.h"
#include "core/xansi.h"

void gt_fasta_show_entry(const char *description, const char *sequence,
                         unsigned long sequence_length, unsigned long width)
{
  gt_assert(sequence);
  gt_fasta_show_entry_generic(description, sequence, sequence_length, width,
                              NULL);
}

void gt_fasta_show_entry_generic(const char *description, const char *sequence,
                                 unsigned long sequence_length,
                                 unsigned long width, GtFile *outfp)
{
  unsigned long i, current_length;
  gt_assert(sequence);
  gt_file_xfputc(FASTA_SEPARATOR, outfp);
  if (description)
    gt_file_xfputs(description, outfp);
  gt_file_xfputc('\n', outfp);
  for (i = 0, current_length = 0; i < sequence_length; i++, current_length++) {
    if (width && current_length == width) {
      gt_file_xfputc('\n', outfp);
      current_length = 0;
    }
    gt_file_xfputc(sequence[i], outfp);
  }
  gt_file_xfputc('\n', outfp);
}
