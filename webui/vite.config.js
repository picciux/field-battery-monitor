import { defineConfig } from 'vite';
import viteCompression from 'vite-plugin-compression';

export default defineConfig({
  plugins: [
    viteCompression({
      algorithm: 'gzip',
      ext: '.gz',
      deleteOriginFile: true // Mantiene anche i file .html/.js normali per debug
    })
  ],
  build: {
    // Unifica tutto in file unici per ridurre le richieste HTTP sull'ESP32
    outDir: '../data',
    emptyOutDir: true, 
    assetsInlineLimit: 100000, 
    rollupOptions: {
      output: {
        assetFileNames: '[name].[ext]',
        chunkFileNames: '[name].js',
        entryFileNames: '[name].js',
      }
    }
  }
});
