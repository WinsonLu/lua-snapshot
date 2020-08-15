mod cJSON {
    include!("cJSON.rs");
}

mod LuaGcNode {
    include!("LuaGcNode.rs");
}

use std::os::raw::*;
#[no_mangle]
pub extern "C" fn lua_gc_node_new(
    node_type: c_int,
    type_str: *const c_char,
    pointer: *const c_void,
) -> *mut LuaGcNode::LuaGcNode {
    unsafe {
        let ptr = Box::new(LuaGcNode::LuaGcNode::new(
            LuaGcNode::LuaGcNodeType::from(node_type),
            std::ffi::CStr::from_ptr(type_str).to_str().unwrap(),
            pointer,
        ));
        return Box::into_raw(ptr);
    }
}

#[no_mangle]
pub extern "C" fn lua_gc_node_free(node: *mut LuaGcNode::LuaGcNode) {
    if node as u8 != 0 {
        unsafe {
            let mut n = Box::from_raw(node);
            n.first_child.take();
        }
    }
}

#[no_mangle]
pub extern "C" fn lua_gc_node_count(node: *mut LuaGcNode::LuaGcNode) -> u32 {
    if node as u8 != 0 {
        unsafe {
            let n = &mut *node;
            return n.count();
        }
    } else {
        return 0;
    }
}

#[no_mangle]
pub extern "C" fn lua_gc_node_set_desc(node: *mut LuaGcNode::LuaGcNode, desc: *const c_char) -> c_int {
    if node as u8 != 0 {
        unsafe {
            let n = &mut *node;
            n.set_desc(std::ffi::CStr::from_ptr(desc).to_str().ok().unwrap());
            return 0;
        }
    } else {
        return 1;
    }
}

#[no_mangle]
pub extern "C" fn lua_gc_node_set_link(node: *mut LuaGcNode::LuaGcNode, link: *const c_char) -> c_int {
    if node as u8 != 0 {
        unsafe {
            let n = &mut *node;
            n.set_link(std::ffi::CStr::from_ptr(link).to_str().ok().unwrap());
            return 0;
        }
    } else {
        return 1;
    }
}

#[no_mangle]
pub extern "C" fn lua_gc_node_copy(node: *mut LuaGcNode::LuaGcNode) -> *mut LuaGcNode::LuaGcNode {
    if node as u8 != 0 {
        unsafe {
            let n = &mut *node;
            let ptr = Box::new(n.copy());

            return Box::into_raw(ptr);
        }
    } else {
        return 0 as *mut LuaGcNode::LuaGcNode;
    }
}

#[no_mangle]
pub extern "C" fn lua_gc_node_copyall(node: *mut LuaGcNode::LuaGcNode) -> *mut LuaGcNode::LuaGcNode {
    if node as u8 != 0 {
        unsafe {
            let n = &mut *node;
            let ptr = Box::new(n.copyall());

            return Box::into_raw(ptr);
        }
    } else {
        return 0 as *mut LuaGcNode::LuaGcNode;
    }
}

#[no_mangle]
pub extern "C" fn lua_gc_node_print(node: *mut LuaGcNode::LuaGcNode) {
    if node as u8 != 0 {
        unsafe {
            let n = &mut *node;
            println!("{}", n.to_str());
        }
    }
}

#[no_mangle]
pub extern "C" fn lua_gc_node_add_child(father: *mut LuaGcNode::LuaGcNode, son: *mut LuaGcNode::LuaGcNode) {
    if father as u8 != 0 && son as u8 != 0 {
        unsafe {
            let f = &mut *father;
            let s = Box::from_raw(son);

            f.add_child_box(s);
        }
    }
}

#[no_mangle]
pub extern "C" fn lua_gc_node_to_str(node: *mut LuaGcNode::LuaGcNode) -> *const c_char {
    if node as u8 != 0 {
        unsafe {
            let n = &mut *node;
            let string = n.to_str();
            return string.as_ptr() as *const c_char;
        }
    } else {
        return 0 as *const c_char;
    }
}

#[no_mangle]
pub extern "C" fn lua_gc_node_diff(
    node1: *mut LuaGcNode::LuaGcNode,
    node2: *mut LuaGcNode::LuaGcNode,
    incr: *mut *const LuaGcNode::LuaGcNode,
    decr: *mut *const LuaGcNode::LuaGcNode,
) {
    if node1 as u8 == 0 || node2 as u8 == 0 {
        return;
    }

    unsafe {
        let n1 = &mut *node1;
        let n2 = &mut *node2;
        if incr as u8 != 0 {
            let res = n2.incr(n1);
            if res.is_none() {
                *incr = 0 as *const LuaGcNode::LuaGcNode;
            } else {
                let res = Box::new(res.unwrap());
                *incr = Box::into_raw(res);
            }
        }

        if decr as u8 != 0 {
            let res = n2.decr(n1);
            if res.is_none() {
                *decr = 0 as *const LuaGcNode::LuaGcNode;
            } else {
                let res = Box::new(res.unwrap());
                *decr = Box::into_raw(res);
            }
        }
    }
}

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
