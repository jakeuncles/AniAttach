# AniAttach for Daz Studio 6

Pick props up and put them down again over the timeline. A character takes a
cup from a table, holds it, and puts it back (or somewhere else). The prop
follows the hand while it is held, keeps the grip it had at the moment it was
picked up, and stays exactly where it was let go.

Daz Studio cannot animate a node's parent, so "parent the cup to the hand at
frame 30, unparent it at frame 80" is not possible on its own. AniAttach
does it for you with ordinary keys.

Version 1.0 · Daz Studio 6.26 or later · Windows 64-bit

## To use

1. Animate the figure: the hand reaches the prop at, say, frame 30 and sets it
   down at frame 80.
2. Open **Window > Panes (Tabs) > AniAttach Timeline** (dock it next to the
   Timeline).
3. Select the prop. In **Holder**, choose the hand (every figure's right and
   left hand are listed), or choose *Last selected item* and Ctrl+click any
   item after the prop.
4. Go to frame 30 and click **Pick Up Here**. Go to frame 80 and click
   **Put Down Here**.

Scrub the timeline: the prop stays put until frame 30, moves with the hand
from 30 to 79, and stays where the hand left it from 80 on.

More:

- **Hand it over**: pick it up again with the other hand at a later frame,
  without putting it down first.
- **Pick it up again later**: another Pick Up Here after a Put Down Here.
- **Change the animation**: edit the arm, the figure or the table as usual;
  AniAttach updates the prop by itself. **Rebake** forces that (for example
  after editing the scene with AniAttach not installed).
- **Fine-tune where it sits**: the prop's own Translate/Rotate are still yours.
  Move it to adjust the grip or its spot on the table.
- **Clear** deletes the pick-ups and put-downs of the selected props;
  **Remove** takes AniAttach off them. Either way the prop goes back to where
  it was first placed.

The same commands are in **Edit > AniAttach** and in the Scene pane's
right-click Edit menu, so you can give them keyboard shortcuts. Each one is a
single undo step.

## The AniAttach Timeline pane

One row per prop that uses AniAttach:

- **grey**: free (on the table, or wherever it was placed)
- **a colour per holder**, with its name: held
- **yellow diamonds**: pick-ups and put-downs
- **white line**: the current frame; click or drag anywhere on the bars to
  move it
- **Now held by**: who holds it at the current frame

Click a name to select the prop (Ctrl+click to add to the selection).

## How it works

A prop that uses AniAttach gets:

- **Held By**: a keyed list, *Free* or one of the holders. Each key holds
  until the next one. You can also key it yourself in the Parameters pane
  (group *AniAttach*).
- **Hold Target 1, 2, …**: the holders (usually hand bones). Pick another node
  here to swap a holder.
- A null called **"<prop> (AniAttach)"** right above it in the Scene pane: the
  *carrier*. AniAttach keys the carrier on every frame where the prop is held,
  and at the start and end of every stretch where it is put down. Don't edit
  the carrier's keys: they are recomputed.

The carrier's motion is computed from the holders' full world matrices, so a
held prop moves exactly like a child parented to the hand.

## Saving, loading and sharing

Everything is stored as normal Daz properties, keys and nodes. Scenes:

- save and open with all pick-ups, put-downs and holders,
- play, render and export (FBX and others) **the same without the plugin**,
  for example on another computer: the carrier keys are already there.

Only *changing* the animation without the plugin needs a **Rebake** once the
plugin is back.

## Limitations

- The holder's scale is not passed on to the prop.
- Edits made while the animation plays are applied when playback stops.

## Releases

Download the installer or the zip from the
[Releases](https://github.com/jakeuncles/AniAttach/releases) page. See
`packaging/INSTALL.txt`.

### Making a release

1. Set the version in `CMakeLists.txt` (`project(aniattach VERSION x.y.z)`),
   `version.h` and `CHANGELOG.md` (a `## x.y.z` section).
2. Commit, then tag and push: `git tag vx.y.z && git push origin vx.y.z`.

The **Release** workflow builds the plugin with the Daz Studio 6 SDK and Qt
6.10.3, then publishes `AniAttach-x.y.z-win64.zip`, `AniAttach-x.y.z-Setup.exe`
and `SHA256SUMS.txt` with the changelog section as release notes. Running the
workflow by hand builds the same files as a run artifact without publishing.

The Daz Studio 6 SDK may not be redistributed, so the workflow reads it from a
private repository with a read-only deploy key (secret `DAZ_SDK_DEPLOY_KEY`).

## To build

Requirements: Visual Studio 2022 (MSVC v143), CMake 3.21+, Qt 6.10.3
(msvc2022_64) and the Daz Studio 6 SDK.

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DDAZ_SDK_DIR="C:/path/to/Daz Studio 6 SDK" ^
  -DCMAKE_PREFIX_PATH="C:/Qt/6.10.3/msvc2022_64"
cmake --build build --config Release
```

Copy `build/Release/dsp_aniattach.dll` into `<Daz Studio 6>/plugins`.

## License

Mozilla Public License 2.0, see `LICENSE`.
