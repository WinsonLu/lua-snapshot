mod cJSON {
    include!("cJSON.rs");
}

mod LuaGcNode {
    include!("LuaGcNode.rs");
}

use std::os::raw::*;
#[no_mangle]
pub extern "C" fn lua_gc_node_get_refs(node: *mut LuaGcNode::LuaGcNode) -> u32 {
    if node as u8 == 0 {
        return 0;
    } else {
        unsafe {
            let n = &*node;
            return n.refs;
        }
    }
}

#[no_mangle]
pub extern "C" fn lua_gc_node_set_refs(node: *mut LuaGcNode::LuaGcNode, refs: u32) {
    if node as u8 == 0 {
        return;
    } else {
        unsafe {
            let n = &mut *node;
            n.refs = refs;
        }
    }
}

#[no_mangle]
pub extern "C" fn lua_gc_node_detach_first_child(node: *mut LuaGcNode::LuaGcNode) -> *mut LuaGcNode::LuaGcNode {
    if node as u8 == 0 {
        return 0 as *mut LuaGcNode::LuaGcNode;
    } else {
        unsafe {
            let mut n = Box::from_raw(node);
            let child = n.first_child.take();
            if child.is_none() {
                return 0 as *mut LuaGcNode::LuaGcNode;
            } else {
                return Box::into_raw(child.unwrap());
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn lua_gc_node_to_jsonstr(node: *mut LuaGcNode::LuaGcNode) -> *const i8 {
    if node as u8 == 0 {
        return 0 as *const i8;
    } else {
        unsafe {
            let n = &*node;
            return n.to_json();
        }
    }
}

#[no_mangle]
pub extern "C" fn lua_gc_node_to_jsonstrfmt(node: *mut LuaGcNode::LuaGcNode) -> *const i8 {
    if node as u8 == 0 {
        return 0 as *const i8;
    } else {
        unsafe {
            let n = &*node;
            return n.to_jsonfmt();
        }
    }
}

struct lua_State;
extern "C" {
    #[link(name="snapshot1")]
    fn luaopen_snapshot(L: *mut lua_State) -> c_int;
}
