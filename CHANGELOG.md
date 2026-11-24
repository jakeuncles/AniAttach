# Changelog

## 1.2.0
- Less lag while you work: AniAttach no longer looks through every node of the scene (a node per bone, hundreds per figure) after every undoable action anywhere in Daz. AniAttach props are now tagged (a hidden "AniAttach Prop" property, saved with the scene) and kept in a registry; only the nodes a file or a paste adds are checked, once, and bones are skipped. The watchers on a holder's arm are only rewired when a holder changes, not on every update.
- Fixed props that drifted out of the hand (by up to 40 cm in a test) when the arm is animated with pose controls such as "Arms Up-Down": 1.1's faster updates read positions in a way that ignores pose controls. AniAttach now detects pose-control (and other driven) animation on the holder and uses the exact method there, and spot-checks the fast method everywhere else. Bone-keyed animation keeps the fast path (600 frames: about 20 ms; with pose controls: about 0.5 s).
- Editing a pose control that moves a holder now updates the prop by itself, like editing the bones.
- Props from 1.0/1.1 scenes get the tag at their next AniAttach edit (opening a scene does not change it).

## 1.1.0
- AniAttach Timeline: drag a pick-up or put-down key to another frame; click a key to go to its frame. Right-click a key to change who holds the prop from there (or put it down), delete it or go to it; right-click a bar to pick the prop up at that frame with any hand, or put it down there; right-click any row to select, clear, rebake or remove.
- Fixed a prop that kept moving with its old animation after its Held By property was deleted outright (e.g. from the Parameters pane). AniAttach now finishes the removal: the prop goes back where it was placed and its carrier is removed.
- Fixed a prop that kept following a holder after the holder's figure was deleted. It now stays where it is until another holder picks it up; Undo brings the figure and the motion back.
- Much faster updates: AniAttach used to move the whole scene through every frame of the timeline, once per prop, each time it recomputed a prop (after a pose edit, a key change or Rebake All). It now reads the holder's position at each frame directly. In a test scene with two animated figures, 10 props and 300 frames, Rebake All went from 18 s to 1.7 s, and a pose edit from 9 s of full CPU to about half a second. The result is identical.
- The current frame no longer jumps around while AniAttach updates.

## 1.0.1
- AniAttach Timeline is frame-accurate: the current-frame line, key diamonds, ticks and frame numbers all sit on the middle of their frame (the line used to be half a frame off the keys).
- Each frame of the bars shows exactly the holder AniAttach uses on that frame; a key between two frames shows on the frame where it takes effect.
- The pane shows the play range, like Daz's Timeline, and follows changes to the play range and frame rate.

## 1.0.0
- First release, for Daz Studio 6.
- Pick a prop up with a hand (or any item) and put it down again over the timeline: it keeps its grip while held and stays where it was let go. Hand it over from one hand to the other.
- AniAttach Timeline pane (Window > Panes (Tabs) > AniAttach Timeline) showing, per prop, who holds it when; Edit > AniAttach menu.
- Updates by itself when you change the pick-ups or the holder's animation. The result is ordinary keys: scenes save, open, render and export the same with or without the plugin.
