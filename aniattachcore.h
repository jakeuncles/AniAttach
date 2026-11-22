/**********************************************************************
	AniAttach for Daz Studio 6 - pick up and put down props over the
	timeline.

	How it works
	------------
	Daz cannot animate a node's parent. Instead, a held item ("the prop")
	gets:
	  - "Held By" (AniAttachHeld): a keyed list, 0 = Free, N = Hold Target N.
	    Each key holds until the next one.
	  - "Hold Target N" (AniAttachTarget<N>): the node (usually a hand bone)
	    that holds the prop while Held By is N.
	and is put under a "carrier" null. AniAttach bakes the carrier's
	transform keys so that
	  - before the first pick-up the carrier is still (the prop stays where
	    you placed it),
	  - while held, the carrier follows the target and keeps the grip the
	    prop had at the moment it was picked up,
	  - after it is put down, the carrier stays where the hand released it.
	The result is plain properties and keys: it saves, loads, renders and
	exports with or without this plugin. The prop's own transform is never
	touched, so you can still place it, or adjust the grip.

	This source code and software provided under the terms of the
	Mozilla Public License Version 2.0 (MPL) a copy of which is
	provided with it.
**********************************************************************/
#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaObject>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include "dztypes.h"

class QTimer;
class DzEnumProperty;
class DzNode;
class DzNodeProperty;
class DzProperty;

// A stretch of the timeline with one holder: [start, end) in Daz time.
struct AniAttachSegment
{
	DzTime start = 0;
	DzTime end = 0;
	int slot = 0;               // 0 = free
	QPointer<DzNode> target;    // null when free (or the target is missing)
};

class AniAttach : public QObject {
	Q_OBJECT
public:
	static AniAttach *instance();

	static const char *held_name;     // "AniAttachHeld"
	static const char *target_prefix; // "AniAttachTarget"
	static const char *carrier_mark;  // "AniAttachCarrier"

	// --- reading ---------------------------------------------------------
	static DzEnumProperty *held_property(const DzNode *prop);
	static DzNodeProperty *target_property(const DzNode *prop, int slot);
	static int  target_count(const DzNode *prop);
	static DzNode *target_node(const DzNode *prop, int slot);
	static bool is_carrier(const DzNode *node);
	static DzNode *carrier_of(const DzNode *prop);
	// Something that can be picked up (not a bone, not a carrier).
	static bool is_candidate(const DzNode *node);
	// A node that uses AniAttach (has Held By).
	static bool uses_aniattach(const DzNode *node);
	// The prop for a node the user selected: the prop itself, or the prop
	// inside a selected carrier.
	static DzNode *prop_for(DzNode *node);
	// The holder slot at a time: each key holds until the next one.
	static int slot_at(const DzEnumProperty *held, DzTime time);
	static bool has_holds(const DzEnumProperty *held);
	static QList<AniAttachSegment> segments(const DzNode *prop);
	// "Genesis 9: Right Hand"
	static QString describe(const DzNode *target);
	// The selected props (a selected carrier counts as its prop), leaving
	// out `holder`, bones and anything `holder` is part of.
	static QList<DzNode*> selected_props(const DzNode *holder = nullptr);
	// The hands of the figures in the scene (Genesis 3/8: rHand/lHand,
	// Genesis 9: r_hand/l_hand), right hand first.
	static QList<DzNode*> hands();

	// --- editing (each wraps itself in one undo step) --------------------
	// Pick `prop` up with `target` at `time` (keeps the current grip).
	bool attach(DzNode *prop, DzNode *target, DzTime time);
	// Put `prop` down at `time` (it stays where the holder leaves it).
	bool release(DzNode *prop, DzTime time);
	// Delete all Held By keys: the prop stays where it was first placed.
	bool clear(DzNode *prop);
	// Take AniAttach off the prop: clear, then remove the carrier and the
	// properties.
	bool remove(DzNode *prop);
	// Recompute the carrier keys now.
	bool bake(DzNode *prop);
	int  bake_all();

	QList<DzNode*> props() const;

signals:
	// Something to redraw: keys, targets, props added or removed.
	void changed();

private:
	AniAttach();
	void hook_scene();
	void notify();
	DzEnumProperty *ensure_setup(DzNode *prop);
	int  slot_for(DzNode *prop, DzNode *target);
	void update_items(DzNode *prop);
	void track(DzNode *prop);
	void untrack(DzNode *prop);
	void watch_chains(DzNode *prop);
	void rescan();
	void schedule_rescan();
	void key_held(DzEnumProperty *held, DzTime time, int slot);
	void schedule_bake(DzNode *prop);
	void run_scheduled_bakes();
	bool bake_now(DzNode *prop);

	bool hooked_ = false;
	bool baking_ = false;
	bool notify_scheduled_ = false;
	bool rescan_scheduled_ = false;
	QTimer *bake_timer_ = nullptr;           // waits for edits to settle
	QList<QPointer<DzNode>> pending_bakes_;
	// Per prop: its own properties, and the transforms of everything that
	// moves it (holders and their parents, the carrier's parents).
	QHash<DzNode*, QList<QMetaObject::Connection>> own_connections_;
	QHash<DzNode*, QList<QMetaObject::Connection>> chain_connections_;
	QList<QPointer<DzNode>> tracked_;
};
