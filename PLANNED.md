# Planned features

Queued ideas that haven't shipped yet, grouped by the build they're planned for.
Shipped work is in [CHANGELOG.md](CHANGELOG.md).

## Build 3: Textures
- Find and select every face using a texture, and replace it (buttons in the Texture Editor)
- Small preview of the selected face's texture, with its name
- Paint while flying (mouselook)
- Hold right-click and drag to paint across many brushes (normal and fly mode)
- Shader work: shader picker (MonitorScreen included), more texture slots, live VMT editing
- Animated, scrolling and pulsing materials; material preview in the 3D view

## Build 4: Physics
- Simplified collision: when a collision layer is made, choose Complex (all detail) or Simplified
  (details removed, walls become solids, open holes such as doors and windows kept), with a short
  explanation of each
- Join brushes (Ctrl+J): hull join by default; Settings → Editing can make it "exact only" (refuse
  and ask when the result wouldn't be one convex brush); Edit → Join Brushes offers both
- Set Mass… (Tools → Model Properties): a mass field with presets Feather-light, Light (ragdoll),
  Moderate, Heavy, Very Heavy (house)
- Collision budget: the bar turns yellow at high piece counts (mid to high 1000s) with a note like
  "Expensive collision mesh, model may still work fine"
- Compiler choice: Hammer++ / StudioMDL-CE besides Garry's Mod's studiomdl

## Build 5: Interface
- Settings in tabs: Editing, Tools & Keys, Appearance (nodraw look, grid, background color),
  Advanced (budget limits)
- Low-resolution mode with pixel fonts (no anti-aliasing) down to 6 px; rename "Compact" to
  "Compact (Low Resolution)"
- Sliders for the nodraw stripe length and the Rainbow Flow marquee
- Prefabs tab beside Layers and the Texture Picker: textured brush sets to paste in, optionally
  keeping their height above the floor

## Build 6: Workshop Manager
- File → Manage Workshop…, publishing with gmpublish through Proton
- Checks for a Steam sign-in and explains, with OK / No, that Bonk only uses the Steam profile for
  the Workshop
- Your addons on the left (hide duplicates): thumbnail, name, ID, last updated
- New Submission form on the right: thumbnail (JPG / PNG), addon path set to the project's addon,
  a banner "You are updating [project]…", and the Workshop types and tags

## Smaller follow-ups
- Collision edges: heavier when seen from far away, if they still blend together
- Arch primitive (several brushes), if wanted

## Longer term
- Skins, bodygroups, LODs and an in-editor model preview
- Flathub release
