// isa-ffi/build.rs

use std::env;
use std::path::PathBuf;

fn main() {
    // Directory where Cargo.toml lives
    let crate_dir = env::var("CARGO_MANIFEST_DIR").unwrap();

    // Tell Cargo: if any of these files change, rerun build.rs
    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=cbindgen.toml");

    // Output header path (in crate root)
    let out_file = PathBuf::from(&crate_dir).join("isa.h");

    // Generate bindings
    let config = cbindgen::Config::from_file("cbindgen.toml").unwrap();
    cbindgen::Builder::new()
        .with_crate(&crate_dir)
        .with_config(config)
        .generate()
        .expect("Unable to generate bindings")
        .write_to_file(out_file);
}
