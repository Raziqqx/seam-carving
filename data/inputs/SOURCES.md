# Test images: sources and licences

All three photographs come from Wikimedia Commons. We downloaded the 500-pixel-wide
thumbnail of each and converted it to PNG (no other edits), so the benchmarks run quickly
and the files are lossless inputs.

| File | Size (w x h) | Why we chose it | Title on Commons | Author | Licence |
|---|---|---|---|---|---|
| `tower.png` | 500 x 339 | Clear subject on a plain background (sky and grass). The same picture as the figure in the assignment brief. | [Broadway tower edit.jpg](https://commons.wikimedia.org/wiki/File:Broadway_tower_edit.jpg) | Newton2 (cropped by Diliff) | [CC BY 2.5](https://creativecommons.org/licenses/by/2.5/) |
| `bridge.png` | 500 x 313 | Strong straight lines and regular structure (cables, deck, towers): the hard case for seam carving. | [Golden Gate Bridge as seen from Battery East.jpg](https://commons.wikimedia.org/wiki/File:Golden_Gate_Bridge_as_seen_from_Battery_East.jpg) | Frank Schulenburg | [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) |
| `castle.png` | 500 x 375 | Textured subject, clouds and a detailed foreground: a "general photo" case. | [Bamburgh Castle, beautiful day.jpg](https://commons.wikimedia.org/wiki/File:Bamburgh_Castle,_beautiful_day.jpg) | Matthew Hartley | [CC BY-SA 2.0](https://creativecommons.org/licenses/by-sa/2.0/) |

`tower_small.png` (150 x 101) is `tower.png` scaled down with plain area interpolation
(`cv::resize`, the same function the benchmark uses for its small copies). It exists so that
the slow graph-cut method can be tried and tested in a few seconds.

The resized versions in `results/` are derivative works of these images and are shared
under the same licences.
