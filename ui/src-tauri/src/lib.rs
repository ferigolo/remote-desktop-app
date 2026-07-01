unsafe extern "C" {
    fn start_media_engine() -> bool;
}

#[tauri::command]
fn start_engine() -> Result<bool, String> {
    println!("🦀[Rust Tauri] Command received from UI. Calling C++...");
    // Because we are calling an external code not managed by Rust, it needs an unsafe block
    let success = unsafe { start_media_engine() };
    
    if success {
        Ok(true)
    } else {
        Err("Failed to initialize C++ engine".to_string())
    }
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
  tauri::Builder::default()
    .invoke_handler(tauri::generate_handler![start_engine])
    .setup(|app| {
      if cfg!(debug_assertions) {
        app.handle().plugin(
          tauri_plugin_log::Builder::default()
            .level(log::LevelFilter::Info)
            .build(),
        )?;
      }
      Ok(())
    })
    .run(tauri::generate_context!())
    .expect("error while running tauri application");
}
