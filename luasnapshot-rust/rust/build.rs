extern crate cc;
use std::env;
fn main() {
    let mut builder = cc::Build::new();
    builder
        .file("src/c/snapshot.c")
        .file("src/c/cJSON.c")
        .include("src/c")
        .compile("libsnapshot1.a");

    let project_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    println!("cargo:rustc-link-search={}", project_dir);
    println!("cargo:rustc-link-lib=snapshot1");
}
