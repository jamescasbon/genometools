/*
  Copyright (c) 2007 Gordon Gremme <gremme@zbh.uni-hamburg.de>
  Copyright (c) 2007 Center for Bioinformatics, University of Hamburg
  See LICENSE file or http://genometools.org/license.html for license details.
*/

#include "lauxlib.h"
#include "gtlua.h"
#include "libgtview/diagram_lua.h"
#include "libgtview/render.h"
#include "libgtview/render_lua.h"

#define RENDER_METATABLE  "GenomeTools.render"
#define check_render(L) \
        (Render**) luaL_checkudata(L, 1, RENDER_METATABLE);

static int render_lua_new(lua_State *L)
{
  Render **render;
  Config *config;
  Env *env = get_env_from_registry(L);
  render = lua_newuserdata(L, sizeof (Render**));
  assert(render);
  config = get_config_from_registry(L);
  *render = render_new(config, env);
  luaL_getmetatable(L, RENDER_METATABLE);
  lua_setmetatable(L, -2);
  return 1;
}

static int render_lua_to_png(lua_State *L)
{
  Render **render;
  Diagram **diagram;
  const char *filename;
  unsigned int width;
  Env *env = get_env_from_registry(L);
  render = check_render(L);
  diagram = check_diagram(L, 2);
  filename = luaL_checkstring(L, 3);
  if (lua_isnil(L, 4))
    width = DEFAULT_RENDER_WIDTH;
  else
    width = luaL_checkint(L, 4);
  render_to_png(*render, *diagram, filename, width, env);
  return 0;
}

static int render_lua_delete(lua_State *L)
{
  Render **render = check_render(L);
  Env *env;
  env = get_env_from_registry(L);
  render_delete(*render, env);
  return 0;
}

static const struct luaL_Reg render_lib_f [] = {
  { "render_new", render_lua_new },
  { NULL, NULL }
};

static const struct luaL_Reg render_lib_m [] = {
  { "to_png", render_lua_to_png },
  { NULL, NULL }
};

int luaopen_render(lua_State *L)
{
  assert(L);
  luaL_newmetatable(L, RENDER_METATABLE);
  /* metatable.__index = metatable */
  lua_pushvalue(L, -1); /* duplicate the metatable */
  lua_setfield(L, -2, "__index");
  /* set its _gc field */
  lua_pushstring(L, "__gc");
  lua_pushcfunction(L, render_lua_delete);
  lua_settable(L, -3);
  /* register functions */
  luaL_register(L, NULL, render_lib_m);
  luaL_register(L, "gt", render_lib_f);
  return 1;
}
