# Bonk website

The static site for Bonk, published to GitHub Pages by `.github/workflows/pages.yml`
whenever `site/` changes on `main`: https://ffseestevanli-oss.github.io/bonk/

No build step: `index.html`, `style.css` and `main.js`. `main.js` fills in the newest release's
name and direct AppImage / Flatpak links from GitHub's public API; without it every download
link falls back to the Releases page. Icons in `assets/` and the pixel font in `fonts/` are
copies of the app's own (GPL-3.0-or-later).

Preview locally:

```sh
cd site && python3 -m http.server 8000   # then open http://localhost:8000
```
