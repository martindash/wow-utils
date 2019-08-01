#include <lua.h>
#include <lauxlib.h>

#include <string.h>

#include "dbc.h"

#if LUA_VERSION_NUM < 502
  #define luaL_newlib(L, m) \
    (lua_newtable(L), luaL_register(L, NULL, m))

// adapted from lua-users.org
static void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup) {
  luaL_checkstack(L, nup+1, "too many upvalues");
  for(; l->name != NULL; l++) {
    int i;
    lua_pushstring(L, l->name);
    for(i = 0; i < nup; i++)
      lua_pushvalue(L, -(nup+1));
    lua_pushcclosure(L, l->func, nup);
    lua_settable(L, -(nup + 3));
  }
  lua_pop(L, nup);
}
#endif

typedef struct {
  dbc_file_t dbc;
  const unsigned char *dbcfile;
} ldbc_userdata_t;

typedef struct {
  dbc_file_t *dbc;
  unsigned char record[];
} ldbc_record_userdata_t;

static int ldbc_get_int(lua_State *L);
static int ldbc_get_uint(lua_State *L);

static int ldbc_close(lua_State *L) {
  ldbc_userdata_t *udata = (ldbc_userdata_t *) luaL_checkudata(L, 1, "dbc");
  dbc_close(&udata->dbc);
  return 0;
}

static int ldbc_get_header(lua_State *L) {
  ldbc_userdata_t *udata = (ldbc_userdata_t *) luaL_checkudata(L, 1, "dbc");
  lua_pushstring(L, udata->dbc.header.signature);
  lua_pushnumber(L, udata->dbc.header.rcount);
  lua_pushnumber(L, udata->dbc.header.fcount);
  lua_pushnumber(L, udata->dbc.header.rsize);
  lua_pushnumber(L, udata->dbc.header.ssize);
  return 5;
}

static int ldbc_get_int(lua_State *L) {
  ldbc_record_userdata_t *udata =
    (ldbc_record_userdata_t *) luaL_checkudata(L, 1, "dbcrecord");
  int32_t field = 0;
  if(dbc_read_int(udata->dbc, udata->record, &field) != 0) {
    lua_pushnil(L);
    lua_pushstring(L, "cannot read int from record");
    return 2;
  }

  lua_pushnumber(L, field);
  return 1;
}

static int ldbc_get_record(lua_State *L) {
  ldbc_userdata_t *udata = (ldbc_userdata_t *) luaL_checkudata(L, 1, "dbc");
  ldbc_record_userdata_t *udata_r =
    (ldbc_record_userdata_t *) lua_newuserdata(L, sizeof(ldbc_record_userdata_t) + udata->dbc.header.rsize);

  switch(dbc_read_record(&udata->dbc, udata_r->record)) {
    case -2:
      lua_pushnil(L);
      lua_pushstring(L, "cannot read record from dbc");
      return 2;
    case -1:
      lua_pushnil(L);
      lua_pushstring(L, "no records left");
      return 2;
  }

  udata_r->dbc = &udata->dbc;

  luaL_getmetatable(L, "dbcrecord");
  lua_setmetatable(L, -2);

  return 1;
}

static int ldbc_get_uint(lua_State *L) {
  ldbc_record_userdata_t *udata =
    (ldbc_record_userdata_t *) luaL_checkudata(L, 1, "dbcrecord");
  uint32_t field = 0;
  if(dbc_read_uint(udata->dbc, udata->record, &field) != 0) {
    lua_pushnil(L);
    lua_pushstring(L, "cannot read uint from record");
    return 2;
  }

  lua_pushnumber(L, field);
  return 1;
}

static int ldbc_open(lua_State *L) {
  ldbc_userdata_t *udata = (ldbc_userdata_t *) lua_newuserdata(L, sizeof(ldbc_userdata_t));
  memset(&udata->dbc, 0, sizeof(dbc_file_t));
  udata->dbcfile = luaL_checkstring(L, 1);

  if(dbc_open(&udata->dbc, udata->dbcfile) != 0) {
    lua_pushnil(L);
    lua_pushstring(L, "no such file or directory");
    return 2;
  }

  luaL_getmetatable(L, "dbc");
  lua_setmetatable(L, -2);

  return 1;
}

static const struct luaL_Reg ldbc[] = {
  { "open", ldbc_open },
  { NULL, NULL }
};

int luaopen_dbc(lua_State *L) {
  if(luaL_newmetatable(L, "dbc")) {
    static struct luaL_Reg dbc_methods[] = {
      { "__gc", ldbc_close },
      { "close", ldbc_close },
      { "get_header", ldbc_get_header },
      { "get_record", ldbc_get_record },
      { NULL, NULL }
    };
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, dbc_methods, 0);
  }

  if(luaL_newmetatable(L, "dbcrecord")) {
    static struct luaL_Reg dbcrecord_methods[] = {
      { "get_int", ldbc_get_int },
      { "get_uint", ldbc_get_uint },
      { NULL, NULL }
    };
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, dbcrecord_methods, 0);
  }

  luaL_newlib(L, ldbc);
  return 1;
}
