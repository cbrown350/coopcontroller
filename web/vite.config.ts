import { defineConfig } from "vite";
import tailwindcss from "@tailwindcss/vite";
import solid from "vite-plugin-solid";

const buildDate = new Date().toLocaleDateString('en-US', { 
  year: 'numeric', 
  month: 'short', 
  day: 'numeric' 
});
const buildTime = new Date().toISOString().split('T')[1].substring(0, 8);

export default defineConfig({
  plugins: [solid(), tailwindcss()],
  define: {
    __BUILD_DATE__: JSON.stringify(buildDate),
    __BUILD_TIME__: JSON.stringify(buildTime),
  },
});
