import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'

export default defineConfig({
  base: './',
  plugins: [react()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
      '@components': path.resolve(__dirname, './src/components'),
      '@services': path.resolve(__dirname, './src/services'),
    },
  },
  server: {
    // Deliberately not 3000: the engine serves the built UI there, and running
    // both meant whichever started second silently landed on a different port.
    port: 5173,
    host: '127.0.0.1',
    strictPort: true,
  },
  build: {
    target: 'ES2020',
    minify: 'terser',
    sourcemap: false,
    outDir: 'dist',
    assetsDir: 'assets',
    rollupOptions: {
      output: {
        // Stable filenames: the engine copies these next to the exe, and hashed
        // names left a new orphan in the resources folder on every build.
        entryFileNames: 'assets/[name].js',
        chunkFileNames: 'assets/[name].js',
        assetFileNames: 'assets/[name].[ext]',
      },
    },
  },
})
