use tauri::{AppHandle, Emitter};
use tauri_plugin_shell::{ShellExt, process::CommandEvent};

#[tauri::command]
fn start_engine(app: AppHandle) -> Result<bool, String> {
    println!("🦀 [Rust Tauri] Command received from UI. Spawning C++ Sidecar...");

    let sidecar_command = app
        .shell()
        .sidecar("core-engine")
        .map_err(|e| format!("Failed to create sidecar command: {}", e))?;

    let (mut rx, mut _child) = sidecar_command
        .spawn()
        .map_err(|e| format!("Failed to spawn engine: {}", e))?;

    tauri::async_runtime::spawn(async move {
        while let Some(event) = rx.recv().await {
            match event {
                CommandEvent::Stdout(line) => {
                    let line_str = String::from_utf8_lossy(&line);
                    print!("🚀 [C++]: {}", line_str);
                }
                CommandEvent::Stderr(line) => {
                    let line_str = String::from_utf8_lossy(&line);
                    eprint!("[C++ ERROR]: {}", line_str);
                }
                CommandEvent::Terminated(payload) => {
                    println!("🦀 [Rust Tauri] C++ Sidecar Terminated: {:?}", payload);
                    let _ = app.emit("engine_stopped", ());
                }
                CommandEvent::Error(err) => {
                    eprintln!("🦀 [Rust Tauri] Sidecar Error: {}", err);
                }
                _ => {}
            }
        }
    });

    Ok(true)
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
  tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .invoke_handler(tauri::generate_handler![start_engine])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
