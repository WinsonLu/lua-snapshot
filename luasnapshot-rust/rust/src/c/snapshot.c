#ifdef __cplusplus
extern "C" {
#endif
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include "lua_gc_node.h"
#include "cJSON.h"
#define SNAPSHOT_METATABLE "_snapshot_metatable_"

static void traverse_object(lua_State* L, lua_State* dL, struct lua_gc_node* parent, const char* link);
static void traverse_table(lua_State* L, lua_State* dL, struct lua_gc_node* parent, const char* link);
static void traverse_function(lua_State* L, lua_State* dL, struct lua_gc_node* parent, const char* link);
static void traverse_userdata(lua_State* L, lua_State* dL, struct lua_gc_node* parent, const char* link);
static void traverse_thread(lua_State* L, lua_State* dL, struct lua_gc_node* parent, const char* link);

static __thread char  buff[128]; 

#if LUA_VERSION_NUM == 501
static void
luaL_checkversion(lua_State *L) {
	if (lua_pushthread(L) == 0) {
		luaL_error(L, "Must require in main thread");
	}
	lua_setfield(L, LUA_REGISTRYINDEX, "mainthread");
}

static void
lua_rawsetp(lua_State *L, int idx, const void *p) {
	if (idx < 0) {
		idx += lua_gettop(L) + 1;
	}
	lua_pushlightuserdata(L, (void *)p);
	lua_insert(L, -2);
	lua_rawset(L, idx);
}

static void
lua_rawgetp(lua_State *L, int idx, const void *p) {
	if (idx < 0) {
		idx += lua_gettop(L) + 1;
	}
	lua_pushlightuserdata(L, (void *)p);
	lua_rawget(L, idx);
}

static void
lua_getuservalue(lua_State *L, int idx) {
	lua_getfenv(L, idx);
}

static void
mark_function_env(lua_State *L, lua_State *dL, struct lua_gc_node* parent) {
	lua_getfenv(L,-1);
	if (lua_istable(L,-1)) {
		traverse_object(L, dL, parent, "[environment]");
	} else {
		lua_pop(L,1);
	}
}

#define is_lightcfunction(L, idx) (0)

#else
#define mark_function_env(L, dL, t)

static int 
is_lightcfunction(lua_State *L, int idx) {
	if (lua_iscfunction(L, idx)) {
		if (lua_getupvalue(L, idx, 1) == NULL) {
			return 1;
		}
		lua_pop(L, 1);
	}
	return 0;
}
#endif

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define TABLE 1
#define FUNCTION 2
#define SOURCE 3
#define THREAD 4
#define USERDATA 5
#define GC_NODE 6

// 根据TValue的tt字段，返回对应的类型字符串
static const char* lua_type_to_string[] = {
    NULL,
    "boolean",
    "lightuserdata",
    "number",
    "string",
    "table",
    "function",
    "userdata",
    "thread",
    "numtags"
};

static struct lua_gc_node* gen_node(lua_State* L, lua_State* dL, struct lua_gc_node* parent, const char* link) {
    int type = lua_type(L, -1);
    const void *p = lua_topointer(L, -1);
    // 创建新的节点
    struct lua_gc_node* new_node = lua_gc_node_new(type, lua_typename(L, type), p);
    // 初始化引用量为1
	lua_gc_node_set_refs(new_node, 1);
	// 复制链接
	lua_gc_node_set_link(new_node, link);
     // 添加到父节点的子节点列表
    lua_gc_node_add_child(parent, new_node);
	//printf("%d: %d\n", __LINE__, lua_gc_node_count(parent));
    /* TODO 添加对size字段的计算 */

    // 添加节点到GC_NODE表
    lua_pushlightuserdata(dL, (void*)new_node);
    lua_rawsetp(dL, GC_NODE, (const void*)p);
    return new_node;
}

static const char* 
keystring(lua_State* L, int index, char* buffer, size_t size) {
    int type = lua_type(L, index);
    switch (type) {
        case LUA_TSTRING:
            return lua_tostring(L, index);
        case LUA_TNUMBER:
            snprintf(buff, size, "[%lg]", lua_tonumber(L, index));
            break;
        case LUA_TBOOLEAN:
            snprintf(buffer, size, "[%s]", lua_toboolean(L, index) ? "true" : "false");
            break;
        case LUA_TNIL:
            snprintf(buffer, size, "[nil]");
            break;
        default:
            snprintf(buffer, size, "[%s:%p]", lua_typename(L, type), lua_topointer(L, index));
            break;
    }
    return buffer;
}

static bool is_marked(lua_State* dL, const void* p) {
    lua_rawgetp(dL, GC_NODE, p);
    if (lua_isnil(dL, -1)) {
        lua_pop(dL, 1);
        return false;
    }
    struct lua_gc_node* node = (struct lua_gc_node*)lua_topointer(dL, -1);
    // 怎加引用计数
    //node->refs += 1;
	lua_gc_node_set_refs(node, lua_gc_node_get_refs(node) + 1);
    lua_pop(dL, 1);
    return true;
}

static void traverse_object(lua_State* L, lua_State* dL, struct lua_gc_node* parent, const char* link) {
	// 判断该对象是否是一个snapshot对象	
	if (lua_getmetatable(L, -1)) {
		luaL_getmetatable(L, SNAPSHOT_METATABLE);
		// 如果是，则跳过
		if (lua_rawequal(L, -1, -2)) {
			lua_pop(L, 2);
			return;
		}
		lua_pop(L, 1);
	}

    int type = lua_type(L, -1);
    switch (type)  {
        case LUA_TTABLE:
            traverse_table(L, dL, parent, link);
            break;
        case LUA_TUSERDATA:
            traverse_userdata(L, dL, parent, link);
            break;
        case LUA_TFUNCTION:
            traverse_function(L, dL, parent, link);
            break;
        case LUA_TTHREAD:
            traverse_thread(L, dL, parent, link);
            break;
		default:
            lua_pop(L, 1);
            break;
    }
}

static void traverse_table(lua_State* L, lua_State* dL, struct lua_gc_node* parent, const char* link) {
    const void *p = lua_topointer(L, -1);
    if (p == NULL)
        return;

    if (is_marked(dL, p)) {
        lua_pop(L, 1);
        return;
    }


    struct lua_gc_node* curr_node = gen_node(L, dL, parent, link);

    bool weakk = false;
    bool weakv = false;
    // 根据metatable判断其k、v的引用是否是弱引用
    if (lua_getmetatable(L, -1)) {
        lua_pushliteral(L, "__mode");
        lua_rawget(L, -2);
        if (lua_isstring(L, -1)) {
            const char* mode = lua_tostring(L, -1);
            if (strchr(mode, 'k')) { weakk = true; }
            if (strchr(mode, 'v')) { weakv = true; }
        }
        lua_pop(L, 1);

        luaL_checkstack(L, LUA_MINSTACK, NULL);
        traverse_object(L, dL, curr_node, "[metatable]");
    }

    // 遍历table
    lua_pushnil(L);
	size_t tbl_size = 0;
    while (lua_next(L, -2) != 0) {
        // 跳过弱引用的value
        if (weakv) { lua_pop(L, 1); }
        else {
            const char* keystr = keystring(L, -2, buff, sizeof(buff));
            traverse_object(L, dL, curr_node, keystr);
        }
        if (!weakk) { 
            lua_pushvalue(L, -1); 
            traverse_object(L, dL, curr_node, "[key]");
        }
		tbl_size++;
    }
	// 设置table的描述，主要是大小
	snprintf(buff, sizeof(buff), "(size: %lu)", tbl_size);
	//strncat(curr_node->desc, buff, LUA_GC_NODE_DESC_SIZE);
	lua_gc_node_set_desc(curr_node, buff);
    lua_pop(L, 1);
}

static void traverse_function(lua_State* L, lua_State* dL, struct lua_gc_node* parent, const char* link) {
    const void* p = lua_topointer(L, -1);
    if (p == NULL)
        return;
    
    if (is_marked(dL, p)) {
        lua_pop(L, 1);
        return;
    }

    struct lua_gc_node* curr_node = gen_node(L, dL, parent, link);
    // 遍历upvalue
    int i;
    for (i = 1; ; i++) {
        const char* name = lua_getupvalue(L, -1, i);
        if (name == NULL)
            break;
        traverse_object(L, dL, curr_node, name[0] ? name : "[upvalue]");
    }
    if (lua_iscfunction(L, -1)) {
        lua_pop(L, 1);
    } else {
        lua_Debug ar;
        lua_getinfo(L, ">S", &ar);
        snprintf(buff, sizeof(buff), "(func: %s:%d)", ar.short_src, ar.linedefined);
        lua_pushstring(dL, buff);
        lua_rawsetp(dL, SOURCE, curr_node);
		// 设置function节点的desc,主要包括定义的源文件名和行数
		lua_gc_node_set_desc(curr_node, buff);
    }
}

static void traverse_thread(lua_State* L, lua_State* dL, struct lua_gc_node* parent, const char* link) {
    const void* p = lua_topointer(L, -1);
    if (p == NULL)
        return;
    if (is_marked(dL, p)) {
        lua_pop(L, 1);
        return;
    }

    struct lua_gc_node* curr_node = gen_node(L, dL, parent, link);
    
    int level = 0;
    lua_State* cL = lua_tothread(L, -1);
    if (cL == L) {
        level = 1;
    } else { /* TODO 多线程支持 */ }

    lua_Debug ar;
    luaL_Buffer b;
    luaL_buffinit(dL, &b);
    // 遍历函数局部变量
    while (lua_getstack(cL, level, &ar)) {
        lua_getinfo(cL, "Sl", &ar);
		/*
        snprintf(buff, sizeof(buff), "%s", ar.short_src);
        if (ar.currentline >= 0) {
            char tmp[16];
            sprintf(tmp, ":%d", ar.currentline);
            strncat(buff, tmp, sizeof(buff) - 1);
        }
        */

        int i, j;
        for (j = 1; j > -1; j -= 2) {
            for (i = j; ; i += j) {
                const char* name = lua_getlocal(cL, &ar, i);
                if (name == NULL)
                    break;
                snprintf(buff, sizeof(buff), "[%s:%s]", name, ar.short_src);
                traverse_object(cL, dL, curr_node, buff);
            }
        }
        ++level;
    }
	snprintf(buff, sizeof(buff), "(vars: %d)", level);
	lua_gc_node_set_desc(curr_node, buff);

    lua_pop(L, 1);
}

static void traverse_userdata(lua_State* L, lua_State* dL, struct lua_gc_node* parent, const char* link) {
    const void* p = lua_topointer(L, -1);
    if (p == NULL)
        return;

    if (is_marked(dL, p)) {
        lua_pop(L, 1);
        return;
    }

    struct lua_gc_node* curr_node = gen_node(L, dL, parent, link);

    if (lua_getmetatable(L, -1)) {
        traverse_object(L, dL, curr_node, "[metatable]");
    }

    lua_getuservalue(L, -1);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 2);
    } else {
        traverse_object(L, dL, curr_node, "[userdata]");
        lua_pop(L, 1);
    }
}

static int 
lua_gc_node_gc(lua_State* L) {
	struct lua_gc_node* node = *(struct lua_gc_node**)lua_touserdata(L, -1);
	if (node != NULL)
		lua_gc_node_free(node);
	return 0;
}

static int 
snapshot(lua_State *L) {
    int nargs = lua_gettop(L);
    if (nargs != 0 && nargs != 2) {
        luaL_error(L, "Number of arguments should be 0 or 2.");
        return 0;
    }
    int i;
    lua_State* dL = luaL_newstate();
    for (i = 0; i < GC_NODE; ++i) {
        lua_newtable(dL);
    }
    struct lua_gc_node* father = lua_gc_node_new(0, "", NULL);
    if (nargs == 0) {
        lua_pushvalue(L, LUA_REGISTRYINDEX);
        traverse_object(L, dL, father, "[REGISTRY]");
        //traverse_node(father.first_child, 0);
        //char* jsonstr = lua_gc_node_to_jsonstr(father.first_child);
        //printf("%s\n", jsonstr);
        //free(jsonstr);
        lua_pop(L, 1);
    }
    else {
        const char* link = lua_tostring(L, -1);
        lua_pushvalue(L, -2);
        traverse_object(L, dL, father, link);
        //traverse_node(father.first_child, 0);
        //char* jsonstr = lua_gc_node_to_jsonstr(father.first_child);
        //printf("%s\n", jsonstr);
        //free(jsonstr);
        lua_pop(L, 1);
    }
	struct lua_gc_node** ptr = (struct lua_gc_node**)lua_newuserdata(L, sizeof(struct lua_gc_node*));
	*ptr = lua_gc_node_detach_first_child(father);
	luaL_getmetatable(L, SNAPSHOT_METATABLE);
	lua_setmetatable(L, -2);
    lua_close(dL);
    return 1;
}

static int
snapshot_printjson(lua_State* L, bool is_formatted) {
	if (lua_gettop(L) != 1) {
		luaL_error(L, "Number of arguments should be 1.");
		return 0;
	}
	void* ptr = lua_touserdata(L, -1);
	if (!lua_getmetatable(L, -1)) {
		luaL_error(L, "Argument is not a valid snapshot.");
		return 0;
	}
	luaL_getmetatable(L, SNAPSHOT_METATABLE);
	if (!lua_rawequal(L, -1, -2)) {
		luaL_error(L, "Argument is not a valid snapshot.");
		return 0;
	}
	struct lua_gc_node* node = *(struct lua_gc_node**)(ptr);
	if (node == NULL)
		return 0;
	char* jsonstr = is_formatted ? lua_gc_node_to_jsonstrfmt(node) : lua_gc_node_to_jsonstr(node);
	printf("%s\n", jsonstr);
	cJSON_free(jsonstr);
	return 0;
}

static int
snapshot_print_jsonfmt(lua_State* L) {
	return snapshot_printjson(L, true);
}

static int
snapshot_print_json_nofmt(lua_State* L) {
	return snapshot_printjson(L, false);
}

static int
snapshot_tojsonfile(lua_State* L, bool is_formatted) {
	if (lua_gettop(L) != 2) {
		luaL_error(L, "Number of arguments should be 2.");
		return 0;
	}
	// 检查是否是snapshot类型
	void* ptr = lua_touserdata(L, -2);
	if (ptr != NULL) {
		if (!lua_getmetatable(L, -2)) {
			luaL_error(L, "Argument 1 should be a snapshot.");
			return 0;
		}
		luaL_getmetatable(L, SNAPSHOT_METATABLE);
		if (!lua_rawequal(L, -1, -2)) {
			luaL_error(L, "Argument 1 should be a snapshot.");
			return 0;
		}
		lua_pop(L, 1);
	} else {
		luaL_error(L, "Argument 1 should be a snapshot.");
		return 0;
	}
	const char* filename = lua_tostring(L, -1);
	if (filename == NULL) {
		luaL_error(L, "Argument 2 should be string.");
		return 0;
	}
	struct lua_gc_node* node = *(struct lua_gc_node**)(ptr);
	FILE* f = fopen(filename, "wb+");
	if (f == NULL) {
		luaL_error(L, "Failed to open file: %s to write.", filename);
		return 0;
	}
	char* jsonstr = NULL;
	if (node == NULL) {
		// 截断文件
		fclose(f);
		return 0;
	} 
	if (is_formatted)
		jsonstr = lua_gc_node_to_jsonstrfmt(node);
	else
		jsonstr = lua_gc_node_to_jsonstr(node);
	const char* p = jsonstr;
	while (*p != 0)
		fwrite(p++, 1, 1, f);
	fclose(f);
	cJSON_free(jsonstr);

	return 0;
}

static int
snapshot_tojsonfile_nofmt(lua_State* L) {
	return snapshot_tojsonfile(L, false);
}

static int
snapshot_tojsonfile_fmt(lua_State* L) {
	return snapshot_tojsonfile(L, true);
}

static int
snapshot_tofile(lua_State* L) {
	if (lua_gettop(L) != 2) {
		luaL_error(L, "Number of arguments should be 2.");
		return 0;
	}
	// 检查是否是snapshot类型
	void* ptr = lua_touserdata(L, -2);
	if (ptr != NULL) {
		if (!lua_getmetatable(L, -2)) {
			luaL_error(L, "Argument 1 should be a snapshot.");
			return 0;
		}
		luaL_getmetatable(L, SNAPSHOT_METATABLE);
		if (!lua_rawequal(L, -1, -2)) {
			luaL_error(L, "Argument 1 should be a snapshot.");
			return 0;
		}
		lua_pop(L, 1);
	} else {
		luaL_error(L, "Argument 1 should be a snapshot.");
		return 0;
	}
	const char* filename = lua_tostring(L, -1);
	if (filename == NULL) {
		luaL_error(L, "Argument 2 should be string.");
		return 0;
	}
	struct lua_gc_node* node = *(struct lua_gc_node**)ptr;
	FILE* f = fopen(filename, "wb+");
	if (f == NULL) {
		luaL_error(L, "Failed to open file: %s to write.", filename);
		return 0;
	}
	const char* str = NULL;
	if (node == NULL) {
		// 截断文件
		fclose(f);
		return 0;
	} 
	str = lua_gc_node_to_str(node);
	const char* p = str;
	while (*p != 0) {
		fwrite(p++, 1, 1, f);
	}
	fclose(f);

	return 0;
}

static int
snapshot_print(lua_State* L) {
	if (lua_gettop(L) != 1) {
		luaL_error(L, "Number of arguments should be 1.");
		return 0;
	}
	void* ptr = lua_touserdata(L, -1);
	if (!lua_getmetatable(L, -1)) {
		luaL_error(L, "Argument is not a valid snapshot.");
		return 0;
	}
	luaL_getmetatable(L, SNAPSHOT_METATABLE);
	if (!lua_rawequal(L, -1, -2)) {
		luaL_error(L, "Argument is not a valid snapshot.");
		return 0;
	}
	struct lua_gc_node* node = *(struct lua_gc_node**)(ptr);
	const char* str = lua_gc_node_to_str(node);
	printf("%s", str);
	return 0;
}

static int
snapshot_free(lua_State* L) {
	if (lua_gettop(L) != 1) {
		luaL_error(L, "Number of arguments should be 1.");
		return 0;
	}
	void* ptr = lua_touserdata(L, -1);
	if (!lua_getmetatable(L, -1)) {
		luaL_error(L, "Argument is not a snapshot.");
		return 0;
	}
	luaL_getmetatable(L, SNAPSHOT_METATABLE);
	if (!lua_rawequal(L, -1, -2)) {
		return 0;
	}
	struct lua_gc_node* node = *(struct lua_gc_node**)ptr;
	lua_gc_node_free(node);
	*(struct lua_gc_node**)ptr = NULL;
	lua_pop(L, 3);
	return 0;
}

static int
snapshot_copy(lua_State* L) {
	if (lua_gettop(L) != 1) {
		luaL_error(L, "Number of arguments should be 1.");
		return 0;
	}
	void* ptr = lua_touserdata(L, -1);
	if (!lua_getmetatable(L, -1)) {
		luaL_error(L, "Argument is not a snapshot.");
		return 0;
	}
	luaL_getmetatable(L, SNAPSHOT_METATABLE);
	if (!lua_rawequal(L, -1, -2)) {
		luaL_error(L, "Argument is not a snapshot.");
		lua_pushnil(L);
		return 1;
	}
	struct lua_gc_node* node = *(struct lua_gc_node**)ptr;
	struct lua_gc_node* copy = lua_gc_node_copyall(node);
	ptr = lua_newuserdata(L, sizeof(copy));
	*(struct lua_gc_node**)ptr = copy;
	lua_pushvalue(L, -3);
	lua_setmetatable(L, -2);

	return 1;
}

static int
snapshot_diff(lua_State* L, bool isAdded) {
	if (lua_gettop(L) != 2) {
		luaL_error(L, "Number of arguments should be 2.");
		return 0;
	}
	void* ptr1 = lua_touserdata(L, 1);
	void* ptr2 = lua_touserdata(L, 2);
	luaL_getmetatable(L, SNAPSHOT_METATABLE);
	int i;
	for (i = 1; i <= 2; ++i) {
		lua_getmetatable(L, i);
		if (!lua_rawequal(L, -1, -2)) {
			luaL_error(L, "Argument %d is not a snapshot.", i);
			return 0;
		}
		lua_pop(L, 1);
	}
	struct lua_gc_node* node1 = *(struct lua_gc_node**)ptr1;
	struct lua_gc_node* node2 = *(struct lua_gc_node**)ptr2;
	struct lua_gc_node* res = NULL;
	if (isAdded)
		lua_gc_node_diff(node1, node2, &res, NULL);
	else
		lua_gc_node_diff(node1, node2, NULL, &res);
	void* ptr = lua_newuserdata(L, sizeof(res));
	*(struct lua_gc_node**)ptr = res;
	luaL_getmetatable(L, SNAPSHOT_METATABLE);
	lua_setmetatable(L, -2);

	return 1;
}

static int
snapshot_increased(lua_State* L) {
	return snapshot_diff(L, true);
}

static int
snapshot_decreased(lua_State* L) {
	return snapshot_diff(L, false);
}

static struct luaL_Reg
snapshot_lib[] = {
	{"snapshot", snapshot}, 						// 保存当前时刻的某一table或registry的快照，并返回一个snapshot(userdata)对象
	{"print", snapshot_print}, 						// 转换为字符串，并打印
	{"print_jsonfmt", snapshot_print_jsonfmt}, 		// 转换为格式化过的json字符串，并打印
	{"print_json", snapshot_print_json_nofmt}, 		// 转换为未格式化过的json字符串, 并打印
	{"to_file", snapshot_tofile}, 					// 转换为字符串，并输出到指定文件
	{"to_jsonfile", snapshot_tojsonfile_nofmt}, 	// 转换为未格式化过的json字符串，并输出到指定文件
	{"to_jsonfilefmt", snapshot_tojsonfile_fmt}, 	// 转换为格式化过的json字符串，并输出到指定文件
	{"free", snapshot_free}, 						// 手动释放snapshot所占用的内存
	{"copy", snapshot_copy}, 						// 复制snapshot	
	{"incr", snapshot_increased}, 					// 求出snapshot1 到 snapshot2 的增量，返回类型仍为snapshot
	{"decr", snapshot_decreased}, 					// 求出snapshot1 到 snapshot2 的减量，返回类型仍为snapshot
	{NULL, NULL}
};

int
luaopen_snapshot(lua_State* L) {
    luaL_checkversion(L);
	luaL_newmetatable(L, SNAPSHOT_METATABLE);
	lua_pushstring(L, "__gc");
	lua_pushcfunction(L, lua_gc_node_gc);
	lua_rawset(L, -3);
	lua_pop(L, 1);
	luaL_newlib(L, snapshot_lib);
	lua_pushvalue(L, -1);
	lua_setglobal(L, "snapshot");
    return 1;
}

#ifdef __cplusplus
}
#endif
