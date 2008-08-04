/*
  Copyright (c) 2008 Sascha Steinbiss <ssteinbiss@stud.zbh.uni-hamburg.de>
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

#ifndef CANVAS_H
#define CANVAS_H

typedef struct Canvas Canvas;

#include "libgtview/block.h"
#include "libgtview/config.h"
#include "libgtview/diagram.h"
#include "libgtview/element.h"
#include "libgtview/graphics.h"
#include "libgtview/line.h"
#include "libgtview/imageinfo.h"
#include "libgtview/track.h"

/* Create a new Canvas object with given <output_type> and <width> using the
   configuration given in <config>. The optional <image_info> is filled when
   the created Config object is used to render a Diagram object. */
Canvas*       canvas_new(Config *config, GraphicsOutType output_type,
                         unsigned long width, ImageInfo *image_info);
/* Returns the height of the given <canvas>. */
unsigned long canvas_get_height(Canvas *canvas);
/* Callback function for Diagram rendering. */
int           canvas_visit_diagram(Canvas*, Diagram*);
/* Callback function for Diagram rendering. */
int           canvas_visit_track_pre(Canvas*, Track*);
/* Callback function for Diagram rendering. */
int           canvas_visit_track_post(Canvas*, Track*);
/* Callback function for Diagram rendering. */
int           canvas_visit_line_pre(Canvas*, Line*);
/* Callback function for Diagram rendering. */
int           canvas_visit_line_post(Canvas*, Line*);
/* Callback function for Diagram rendering. */
int           canvas_visit_block(Canvas*, Block*);
/* Callback function for Diagram rendering. */
int           canvas_visit_element(Canvas*, Element*);
/* Write rendered <canvas> to file with name <filename>. */
int           canvas_to_file(Canvas *canvas, const char *filename, Error*);
/* Append rendered <canvas> to given <stream>. */
int           canvas_to_stream(Canvas *canvas, Str *stream);
/* Delete the given <canvas>. */
void          canvas_delete(Canvas *canvas);

#endif