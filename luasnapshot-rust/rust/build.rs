extern crate cc;
use std::fs::read_dir;
use std::env;
fn main() {
    let mut builder = cc::Build::new();

    add_files_to_builder(&mut builder, &get_files_with_postfix_in_dir("src/c", ".c").ok().unwrap());
    builder
        .include("src/c")
        .compile("libsnapshot1.a");

    let project_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    println!("cargo:rustc-link-search={}", project_dir);
    println!("cargo:rustc-link-lib=snapshot1");
}

fn add_files_to_builder(builder: &mut cc::Build, files: &Vec::<String>) {
    for file in files {
        builder.file(file);
    }
}

fn get_files_with_postfix_in_dir(dir: &str, postfix: &str) -> Result<Vec<String>, ()> {
    let gen = read_dir(dir);
    if gen.is_err() {
        return Err(());
    } else {
        let mut ret = Vec::<String>::new();
        for entry in gen.ok().unwrap() {
            let item = entry.ok().unwrap();
            if item.file_name().to_str().unwrap().ends_with(postfix) {
                ret.push(format!("{}/{}", dir, item.file_name().to_str().unwrap().to_string()));
            }
        }

        return Ok(ret);
    }
}
