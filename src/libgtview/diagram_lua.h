/*
  Copyright (c) 2007 Gordon Gremme <gremme@zbh.uni-hamburg.de>
  Copyright (c) 2007 Center for Bioinformatics, University of Hamburg
  See LICENSE file or http://genometools.org/license.html for license details.
*/

#ifndef DIAGRAM_LUA_H
#define DIAGRAM_LUA_H

#include "lua.h"

/* exports the Diagram class to Lua:

   -- create a diagram which contains the genome nodes given in <feature_index>
   -- in the range from <startpos> to <endend> in sequence region <seqid>
   diagram = gt.diagram_new(feature_index, startpos, endpos, seqid)
*/
int luaopen_diagram(lua_State*);

#define DIAGRAM_METATABLE  "GenomeTools.diagram"
#define check_diagram(L, POS) \
              (Diagram**) luaL_checkudata(L, POS, DIAGRAM_METATABLE);

#endif
