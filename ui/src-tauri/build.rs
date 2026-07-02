use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    let target = env::var("TARGET").unwrap();
    
    // 1. Build the C++ engine using the cmake crate
    let dst = cmake::Config::new("../../engine")
        .build();

    // 2. Identify the built executable path
    let exe_name = if target.contains("windows") {
        "core-engine.exe"
    } else {
        "core-engine"
    };
    
    let engine_bin = dst.join("bin").join(exe_name);
    
    // 3. Move/Copy to Tauri's sidecar binaries folder with the TARGET triple appended
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let binaries_dir = PathBuf::from(&manifest_dir).join("binaries");
    
    fs::create_dir_all(&binaries_dir).expect("Failed to create binaries directory");
    
    let sidecar_filename = if target.contains("windows") {
        format!("core-engine-{}.exe", target)
    } else {
        format!("core-engine-{}", target)
    };
    
    let target_bin = binaries_dir.join(sidecar_filename);
    
    // Copy the executable
    fs::copy(&engine_bin, &target_bin)
        .unwrap_or_else(|e| panic!("Failed to copy C++ engine from {} to {}: {}", engine_bin.display(), target_bin.display(), e));
    
    // Tell Cargo that if any C++ file changes, rerun this build script
    println!("cargo:rerun-if-changed=../../engine");

    tauri_build::build()
}
