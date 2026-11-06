import { defineConfig } from 'astro/config';
import sitemap from '@astrojs/sitemap';

// A GitHub Pages project site lives under /Thalassa/, so every internal link must carry the base.
// Use the withBase() helper in src/site.ts for that.
export default defineConfig({
  site: 'https://badsadcode.github.io',
  base: '/Thalassa',
  trailingSlash: 'always',
  integrations: [sitemap()],
  markdown: {
    shikiConfig: { theme: 'github-dark-dimmed' },
  },
});
