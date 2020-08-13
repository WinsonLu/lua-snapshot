extern crate libc;
mod lib;

fn main() {
    let a = 1;
    let mut gcnode = lib::LuaGcNode::new(
        lib::LuaGcNodeType::LuaTableType,
        "table",
        &a as *const i32 as *const libc::c_void,
    );
    gcnode.set_desc("(size: 32)");
    println!("{}", gcnode.name);
    gcnode.add_child_box(Box::new(lib::LuaGcNode::new(
        lib::LuaGcNodeType::LuaStringType,
        "string",
        &a as *const i32 as *const libc::c_void,
    )));

    let mut copy_gcnode = gcnode.copyall();

    gcnode.map_childs(|child| {
        println!("{}", child.name);
    });

    println!("{}", gcnode.count());
    copy_gcnode.map_childs(|child| {
        println!("{}", child.name);
    });
    println!("{}", copy_gcnode.count());

    copy_gcnode.map(|node| {
        println!("{}", node.name);
    });

    copy_gcnode.add_child_box(Box::new(lib::LuaGcNode::new(
        lib::LuaGcNodeType::LuaUserDataType,
        "userdata",
        &a as *const i32 as *const libc::c_void,
    )));
    copy_gcnode.map_with_height(|node, height| {
        for _ in 0..height {
            print!("\t|");
        }
        println!("{}", node.name);
    });

    let string = copy_gcnode.to_str();
    println!("{}", string);



    println!("{}", copy_gcnode.to_str());
}
