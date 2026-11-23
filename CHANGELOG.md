# Changelog

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
