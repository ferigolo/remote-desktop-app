import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  output: "export", // Fundamental for Tauri to bundle HTML/JS
  reactCompiler: true,
};

export default nextConfig;
