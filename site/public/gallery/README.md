# Performance gallery manifest

`gallery.json` is read by the gallery page at runtime. Adding a performance is
one entry here (plus the video where the entry points) — no page edit.

```json
{
  "performances": [
    {
      "title": "Jankó, ten fingers, Osmose underneath",
      "device": "iPad Air 11\" (M4) + Apple Pencil Pro",
      "layout": "Jankó",
      "modes": ["Play", "Ripple bend", "Swirl pressure"],
      "notes": "What the caption should tell a reader deciding whether to read the guide page.",
      "guide": "/guide/play-mode/",
      "src": "/gallery/janko-osmose.mp4",
      "poster": "/gallery/janko-osmose.jpg"
    },
    {
      "title": "…",
      "device": "Galaxy Tab S8 Ultra + S-Pen",
      "layout": "Piano grid",
      "modes": ["Play", "Stylus legato"],
      "youtube": "VIDEO_ID"
    }
  ]
}
```

* `title`, `device`, `layout`, `modes` are required — the gallery doubles as a
  "what it can do" index into the user guide, so every card is captioned with
  the device, the layout and the modes used. `guide` is an optional link into
  the guide; `notes` is free text; `synth` (the sound the performance was
  played through) renders as a tag; `based` (`{ "label", "url" }`) links the
  piece the performance is based on, rendered as "Based on …".
* Exactly one of `src` (a self-hosted file, relative to the site root — put it
  in this folder) or `youtube` (an unlisted-video id, embedded through
  `youtube-nocookie.com`). An entry with neither renders as **coming soon**
  (step 67: the 2.0 cards wait for the author's captures; their captions are
  placeholders to correct when the video lands).
* `replay` (optional, step 67): the performance's own `.sumireplay`, relative
  to the site root — put it in this folder. The card gains a *watch it
  again* link that opens the marble app with `?replay=` on that file, so the
  water is redrawn from the recorded bytes in the reader's browser (the
  picture alone; the web has no sound core). The link renders only once the
  card has its video; a coming-soon card says its recording will replay.
  The pending cards name their files here; the author records them and
  drops the files in.
* Keep self-hosted files modest (GitHub Pages serves them; a 1080p H.264 at
  ~4 Mbit/s is fine) and add a `poster` frame so the grid does not load video
  data until played.
* `npm run check` validates the manifest after every build.
