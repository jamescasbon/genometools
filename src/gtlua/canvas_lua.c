/*
  Copyright (c) 2008 Sascha Steinbiss <ssteinbiss@zbh.uni-hamburg.de>
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

#ifndef WITHOUT_CAIRO

#include "lauxlib.h"
#include "annotationsketch/canvas.h"
#include "annotationsketch/luastyle.h"
#include "core/error.h"
#include "extended/luahelper.h"
#include "gtlua/canvas_lua.h"
#include "gtlua/image_info_lua.h"

static int canvas_lua_new_generic(lua_State *L, GraphicsOutType t)
{
  GT_Canvas **canvas;
  ImageInfo **ii;
  unsigned int width;
  GT_Style *style;
  width = luaL_checkint(L, 1);
  /* create canvas */
  style = lua_get_style_from_registry(L);
  canvas = lua_newuserdata(L, sizeof (GT_Canvas*));
  assert(canvas);
  /* if a imageinfo object is passed, it must be correct type */
  if (lua_isnil(L, 2))
    *canvas = gt_canvas_new(style, t, width, NULL);
  else
  {
    ii = check_imageinfo(L, 2);
   *canvas = gt_canvas_new(style, t, width, *ii);
  }
  luaL_getmetatable(L, CANVAS_METATABLE);
  lua_setmetatable(L, -2);
  return 1;
}

static int canvas_lua_new_pdf(lua_State *L)
{
  return canvas_lua_new_generic(L, GRAPHICS_PDF);
}

static int canvas_lua_new_png(lua_State *L)
{
  return canvas_lua_new_generic(L, GRAPHICS_PNG);
}

static int canvas_lua_new_svg(lua_State *L)
{
  return canvas_lua_new_generic(L, GRAPHICS_SVG);
}

static int canvas_lua_new_ps(lua_State *L)
{
  return canvas_lua_new_generic(L, GRAPHICS_PS);
}

static int canvas_lua_to_file(lua_State *L)
{
  GT_Canvas **canvas;
  Error *err;
  const char *fn;
  int had_err = 0;
  err = error_new();
  canvas = check_canvas(L, 1);
  fn = luaL_checkstring(L, 2);
  assert(canvas);
  had_err = gt_canvas_to_file(*canvas, fn, err);
  if (had_err)
    return lua_gt_error(L, err);
  error_delete(err);
  return 0;
}

static int canvas_lua_delete(lua_State *L)
{
  GT_Canvas **canvas;
  canvas = check_canvas(L, 1);
  gt_canvas_delete(*canvas);
  return 0;
}

static const struct luaL_Reg canvas_lib_f [] = {
  { "canvas_new_png", canvas_lua_new_png },
  { "canvas_new_pdf", canvas_lua_new_pdf },
  { "canvas_new_ps", canvas_lua_new_ps },
  { "canvas_new_svg", canvas_lua_new_svg },
  { NULL, NULL }
};

static const struct luaL_Reg canvas_lib_m [] = {
  { "to_file", canvas_lua_to_file },
  { NULL, NULL }
};

int luaopen_canvas(lua_State *L)
{
  assert(L);
  luaL_newmetatable(L, CANVAS_METATABLE);
  lua_pushvalue(L, -1); /* duplicate the metatable */
  lua_setfield(L, -2, "__index");
  /* set its _gc field */
  lua_pushstring(L, "__gc");
  lua_pushcfunction(L, canvas_lua_delete);
  lua_settable(L, -3);
  /* register functions */
  luaL_register(L, NULL, canvas_lib_m);
  luaL_register(L, "gt", canvas_lib_f);
  return 1;
}

#endif