use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let engine_path = PathBuf::from(manifest_dir).join("../../engine/build");
    
    println!("cargo:rustc-link-search=native={}", engine_path.display());
    println!("cargo:rustc-link-lib=dylib=core_engine");
    println!("cargo:rustc-link-arg=-Wl,-rpath={}", engine_path.display());
    tauri_build::build()
}
