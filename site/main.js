// Fill in the newest release's name and direct download links from GitHub's public API.
// Without JavaScript (or if the API is unreachable) every link falls back to the Releases page.
(function () {
  var API = "https://api.github.com/repos/ffseestevanli-oss/bonk/releases?per_page=10";

  fetch(API, { headers: { Accept: "application/vnd.github+json" } })
    .then(function (r) { return r.ok ? r.json() : Promise.reject(r.status); })
    .then(function (releases) {
      // Newest published release that has packages attached (betas are pre-releases, so
      // /releases/latest would skip them).
      var rel = releases.find(function (r) { return !r.draft && r.assets && r.assets.length; }) ||
                releases.find(function (r) { return !r.draft; });
      if (!rel) return;

      var name = (rel.name || rel.tag_name).replace(/^Bonk!?\s*/i, "").split(":")[0].trim() || rel.tag_name;
      document.querySelectorAll("[data-release-name]").forEach(function (el) { el.textContent = name; });
      document.querySelectorAll("[data-release-link]").forEach(function (el) { el.href = rel.html_url; });

      document.querySelectorAll("[data-asset]").forEach(function (el) {
        var ext = el.getAttribute("data-asset").toLowerCase();
        // Prefer the main app over the standalone Spawnlist Creator.
        var matches = rel.assets.filter(function (a) { return a.name.toLowerCase().endsWith("." + ext); });
        var main = matches.find(function (a) { return !/spawnlist/i.test(a.name); }) || matches[0];
        el.href = main ? main.browser_download_url : (el.getAttribute("data-fallback") || rel.html_url);
      });
    })
    .catch(function () { /* keep the static fallbacks */ });
})();
