#ifndef _XLUA_SNAPSHOT_SNAPSHOT_H_
#define _XLUA_SNAPSHOT_SNAPSHOT_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "lauxlib.h"
#include "lua.h"
int luaopen_snapshot(lua_State* L);
#ifdef __cplusplus
}
#endif

#endif /* _XLUA_SNAPSHOT_SNAPSHOT_H_ */
