# THALASSA devlog

The public development log. A static site built with [Astro](https://astro.build) from Markdown files, published to GitHub Pages. No database, no accounts, no tracking.

**Stack choice:** Astro validates each post's metadata at build time, so a typo in a status label fails the build instead of reaching the site, and it ships almost no JavaScript. Versions are pinned in `package.json` and `package-lock.json`.

You need [Node.js](https://nodejs.org) 24 or newer.

## Preview locally

```bash
cd site
npm ci
npm run dev
```

Open <http://localhost:4321/Thalassa/>. The page reloads as you edit.

## Write a new entry

Add one Markdown file to `src/content/devlog/`. The filename becomes the URL and the log number, so `0002-first-sonar.md` is published at `/devlog/0002-first-sonar/` as `EXP-0002`.

```markdown
---
title: "First Sonar"
date: 2026-12-01
summary: "One or two sentences. Shown on cards, in RSS and when the link is shared."
category: "Engineering"
tags: ["sonar"]
status: "prototype"
---

Your text here.
```

That is all. The home page, archive, previous/next links and RSS feed update on their own.

**`status` must be one of these**, and the build fails on anything else:

| Value | Use it when |
|---|---|
| `concept` | It is an idea. Nothing has been built. |
| `prototype` | A rough version exists to test something. |
| `in-development` | It is being built for real. |
| `implemented` | It works in a real build. |
| `tested` | It works, and tests confirm it. |

Only use `implemented` or `tested` when a working build or test confirms it. To hold an entry back, add `draft: true`.

### Add images

Put images in `public/images/devlog/`, then reference them from the root of the site:

```markdown
![A description of the image for people who cannot see it.](/Thalassa/images/devlog/first-sonar.jpg)
```

Paths in Markdown body text need the `/Thalassa/` prefix. For a cover image, add this to the frontmatter, without the prefix; the alt text is required:

```yaml
cover: "/images/devlog/first-sonar.jpg"
coverAlt: "What the image shows."
```

Keep images under about 300 KB. Commit them normally: **do not use Git LFS for anything in `site/`**, because GitHub Pages cannot serve LFS files. `.gitattributes` already excludes this folder.

## Update the project status

Edit `PROJECT_STATUS` at the top of `src/site.ts`. It is written by hand on purpose, so that nothing can imply a feature is finished. The latest entry on the home page updates by itself.

## Build and check

```bash
npm run build
npm run check-links
```

`check-links` fails if any internal link or image in the built site points at nothing.

## Publish

Push changes under `site/` to `main`. The workflow in `.github/workflows/deploy-devlog.yml` builds the site, checks its links, and deploys it. If the build or link check fails, the last good version stays online.

The site is published at <https://badsadcode.github.io/Thalassa/> once GitHub Pages is enabled with its source set to **GitHub Actions** (repository Settings, then Pages).

To publish without changing anything, use **Run workflow** on the workflow's page in the Actions tab.

## Third-party licences

Fonts are self-hosted through Fontsource and are under the SIL Open Font License: Saira, Source Serif 4, JetBrains Mono. See `docs/ASSET_LICENSES.md`.
