// Checks that every internal link and image in the built site points at a file that exists.
// Run after `npm run build`. Fails the build, so a broken path cannot reach the published site.
import { readdir, readFile, stat } from 'node:fs/promises';
import { join, relative } from 'node:path';

const DIST = new URL('../dist/', import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, '$1');
const BASE = '/Thalassa/';

async function htmlFiles(dir) {
  const entries = await readdir(dir, { withFileTypes: true });
  const nested = await Promise.all(
    entries.map((entry) => {
      const path = join(dir, entry.name);
      if (entry.isDirectory()) return htmlFiles(path);
      return entry.name.endsWith('.html') ? [path] : [];
    }),
  );
  return nested.flat();
}

async function exists(path) {
  try {
    const info = await stat(path);
    return info.isDirectory() ? exists(join(path, 'index.html')) : true;
  } catch {
    return false;
  }
}

const problems = [];
const files = await htmlFiles(DIST);

for (const file of files) {
  const html = await readFile(file, 'utf8');
  for (const [, url] of html.matchAll(/(?:href|src)="([^"]+)"/g)) {
    if (/^(https?:|mailto:|#|data:)/.test(url)) continue;
    const page = relative(DIST, file);
    if (!url.startsWith(BASE)) {
      problems.push(`${page}: "${url}" does not start with ${BASE}`);
      continue;
    }
    const target = decodeURIComponent(url.slice(BASE.length).split(/[?#]/)[0]);
    if (!(await exists(join(DIST, target)))) {
      problems.push(`${page}: "${url}" points at nothing`);
    }
  }
}

if (problems.length) {
  console.error(problems.join('\n'));
  console.error(`\n${problems.length} broken link(s).`);
  process.exit(1);
}
console.log(`Links OK across ${files.length} pages.`);
