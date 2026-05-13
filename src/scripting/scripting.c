/*
 * scripting.c — Lua scripting via embedded Lua 5.4
 *
 * Compile with: -DENABLE_LUA -llua5.4 -lm
 * If Lua is not available, a stub implementation is provided.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scripting.h"
#include "../engine/engine.h"

#ifdef ENABLE_LUA

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static lua_State *g_lua = NULL;
static Store     *g_script_store = NULL;

/* redis.call(cmd, arg1, arg2, ...) — bridge into the command engine */
static int lua_redis_call(lua_State *L) {
    int nargs = lua_gettop(L);
    if (nargs < 1) {
        lua_pushstring(L, "ERR wrong number of arguments for redis.call");
        return 1;
    }

    /* Build the command string */
    char cmd_buf[2048];
    int pos = 0;
    for (int i = 1; i <= nargs && pos < (int)sizeof(cmd_buf) - 64; i++) {
        const char *arg = luaL_checkstring(L, i);
        int has_space = (strchr(arg, ' ') != NULL);
        if (i > 1) cmd_buf[pos++] = ' ';
        if (has_space) {
            pos += snprintf(cmd_buf + pos, sizeof(cmd_buf) - pos, "\"%s\"", arg);
        } else {
            pos += snprintf(cmd_buf + pos, sizeof(cmd_buf) - pos, "%s", arg);
        }
    }
    cmd_buf[pos] = '\0';

    CommandResult res = run_command(g_script_store, cmd_buf);
    lua_pushstring(L, res.message ? res.message : "");
    freeResult(res);
    return 1;
}

/* redis.log(level, message) */
static int lua_redis_log(lua_State *L) {
    const char *msg = luaL_checkstring(L, lua_gettop(L));
    printf("[lua] %s\n", msg);
    return 0;
}

int scripting_init(void) {
    g_lua = luaL_newstate();
    if (!g_lua) return -1;
    luaL_openlibs(g_lua);

    /* Create 'redis' table */
    lua_newtable(g_lua);
    lua_pushcfunction(g_lua, lua_redis_call);
    lua_setfield(g_lua, -2, "call");
    lua_pushcfunction(g_lua, lua_redis_log);
    lua_setfield(g_lua, -2, "log");
    lua_setglobal(g_lua, "redis");

    printf("[scripting] Lua %s interpreter initialised\n", LUA_VERSION);
    return 0;
}

CommandResult scripting_eval(Store *store, const char *script,
                             int numkeys, char **keys, char **args, int numargs) {
    if (!g_lua) return error("ERR Lua scripting not initialised");

    g_script_store = store;

    /* Set KEYS table */
    lua_newtable(g_lua);
    for (int i = 0; i < numkeys; i++) {
        lua_pushstring(g_lua, keys[i]);
        lua_rawseti(g_lua, -2, i + 1);
    }
    lua_setglobal(g_lua, "KEYS");

    /* Set ARGV table */
    lua_newtable(g_lua);
    for (int i = 0; i < numargs; i++) {
        lua_pushstring(g_lua, args[i]);
        lua_rawseti(g_lua, -2, i + 1);
    }
    lua_setglobal(g_lua, "ARGV");

    /* Execute the script */
    if (luaL_dostring(g_lua, script) != LUA_OK) {
        const char *err = lua_tostring(g_lua, -1);
        char buf[1024];
        snprintf(buf, sizeof(buf), "ERR %s", err ? err : "unknown lua error");
        lua_pop(g_lua, 1);
        g_script_store = NULL;
        return error(buf);
    }

    /* Get return value */
    const char *result = lua_tostring(g_lua, -1);
    CommandResult res = ok(result ? result : "(nil)");
    lua_settop(g_lua, 0);  /* clear stack */
    g_script_store = NULL;
    return res;
}

void scripting_close(void) {
    if (g_lua) {
        lua_close(g_lua);
        g_lua = NULL;
    }
}

#else  /* !ENABLE_LUA — stub implementation */

int scripting_init(void) {
    printf("[scripting] Lua scripting disabled (compile with -DENABLE_LUA)\n");
    return 0;
}

CommandResult scripting_eval(Store *store, const char *script,
                             int numkeys, char **keys, char **args, int numargs) {
    (void)store; (void)script; (void)numkeys; (void)keys; (void)args; (void)numargs;
    return error("ERR Lua scripting not enabled. Recompile with -DENABLE_LUA and link -llua5.4 -lm");
}

void scripting_close(void) { }

#endif
