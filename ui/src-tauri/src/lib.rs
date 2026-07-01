use std::sync::Mutex;
use tauri::{AppHandle, Emitter};

// Global state for storing the application controller
static APP_HANDLE: Mutex<Option<AppHandle>> = Mutex::new(None);

// The callback C++ will call
extern "C" fn on_engine_close() {
  println!("🦀 [Rust Tauri] Got a signal from C++: Window was closed.");
  
  // Emits event to Next.js
  if let Ok(guard) = APP_HANDLE.lock() {
      if let Some(app) = guard.as_ref() {
          let _ = app.emit("engine_stopped", ()); 
      }
  }
}

unsafe extern "C" {
    fn start_media_engine(cb: extern "C" fn()) -> bool;
}

#[tauri::command]
fn start_engine(app: AppHandle) -> Result<bool, String> {
    println!("🦀 [Rust Tauri] Command received from UI. Calling C++...");

    // Stores AppHandle so the callback can use it later
    if let Ok(mut guard) = APP_HANDLE.lock() {
        *guard = Some(app);
    }

    // Because we are calling an external code not managed by Rust, it needs an unsafe block
    let success = unsafe { start_media_engine(on_engine_close) };
    
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
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
