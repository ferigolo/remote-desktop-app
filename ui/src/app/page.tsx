"use client";

import { useEffect, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";

export default function Home() {
  const [status, setStatus] = useState<"Inativo" | "Conectando" | "Online">(
    "Inativo",
  );

  useEffect(() => {
    // Register listener for 'engine_stopped'
    const unlisten = listen("engine_stopped", () => {
      console.log("Received event from Rust: the window was closed");
      setStatus("Inativo");
    });

    return () => {
      unlisten.then((f) => f());
    };
  }, []);

  const handleStartEngine = async () => {
    try {
      setStatus("Conectando");
      console.log("Requesting Rust to start engine...")
      const success = await invoke<boolean>("start_engine"); // Invoques Rust
      if (success) setStatus("Online");
    } catch (error) {
      console.error("Error on IPC bridge:", error);
      setStatus("Inativo");
    }
  };

  return (
    <main className="flex min-h-screen flex-col items-center justify-center bg-gray-900 text-white p-24">
      <h1 className="text-4xl font-bold mb-8">Remote Desktop</h1>

      <div className="flex flex-col items-center gap-4 bg-gray-800 p-8 rounded-xl border border-gray-700">
        <p className="text-lg">
          Status do Motor C++:
          <span
            className={`ml-2 font-mono ${status === "Online" ? "text-green-400" : "text-yellow-400"}`}>
            {status}
          </span>
        </p>

        <button
          onClick={handleStartEngine}
          disabled={status === "Online"}
          className="px-6 py-3 bg-blue-600 hover:bg-blue-500 disabled:bg-gray-600 rounded-lg font-semibold transition-colors">
          {status === "Online" ? "Motor Rodando" : "Iniciar Motor"}
        </button>
      </div>
    </main>
  );
}
