/**********************************************************************
	AniAttach for Daz Studio 6 - schedule, carrier and baking.

	This source code and software provided under the terms of the
	Mozilla Public License Version 2.0 (MPL) a copy of which is
	provided with it.
**********************************************************************/

#include "aniattachcore.h"

#include <cmath>

#include <QtCore/QTimer>

#include "dzapp.h"
#include "dzbone.h"
#include "dzboolproperty.h"
#include "dzenumproperty.h"
#include "dzfloatproperty.h"
#include "dzmatrix3.h"
#include "dznode.h"
#include "dznodeproperty.h"
#include "dzquat.h"
#include "dzscene.h"
#include "dzskeleton.h"
#include "dztimerange.h"
#include "dzundostack.h"
#include "dzvec3.h"

const char *AniAttach::held_name = "AniAttachHeld";
const char *AniAttach::target_prefix = "AniAttachTarget";
const char *AniAttach::carrier_mark = "AniAttachCarrier";

namespace {

const char *c_group = "AniAttach";
const int c_bake_delay_ms = 300;

// Daz deletes the scene before the main window on exit.
bool scene_usable()
{
	return dzApp && !dzApp->isClosing() && dzScene;
}

void anim_range(DzTime &start, DzTime &end, DzTime &step)
{
	const DzTimeRange range = dzScene->getAnimRange();
	step = qMax<DzTime>(1, dzScene->getTimeStep());
	start = range.getStart();
	end = qMax(range.getEnd(), start);
}

// The carrier's transform channels that AniAttach bakes.
QList<DzFloatProperty*> transform_controls(const DzNode *node)
{
	return { node->getXPosControl(), node->getYPosControl(), node->getZPosControl(),
	         node->getXRotControl(), node->getYRotControl(), node->getZRotControl() };
}

// Everything that changes where a node is: its own and its parents'
// translate/rotate/scale channels.
QList<DzProperty*> placement_controls(const DzNode *node)
{
	QList<DzProperty*> list;
	for (DzProperty *p : { (DzProperty*)node->getXPosControl(), (DzProperty*)node->getYPosControl(),
	                       (DzProperty*)node->getZPosControl(), (DzProperty*)node->getXRotControl(),
	                       (DzProperty*)node->getYRotControl(), (DzProperty*)node->getZRotControl(),
	                       (DzProperty*)node->getScaleControl(), (DzProperty*)node->getXScaleControl(),
	                       (DzProperty*)node->getYScaleControl(), (DzProperty*)node->getZScaleControl() })
		if (p)
			list.append(p);
	return list;
}

bool is_inside(const DzNode *node, const DzNode *ancestor)
{
	for (const DzNode *n = node; n; n = n->getNodeParent())
		if (n == ancestor)
			return true;
	return false;
}

} // namespace

// ---------------------------------------------------------------------------
// reading

DzEnumProperty *AniAttach::held_property(const DzNode *prop)
{
	return prop ? qobject_cast<DzEnumProperty*>(prop->findProperty(held_name)) : nullptr;
}

DzNodeProperty *AniAttach::target_property(const DzNode *prop, int slot)
{
	if (!prop || slot < 1)
		return nullptr;
	return qobject_cast<DzNodeProperty*>(prop->findProperty(QString("%1%2").arg(target_prefix).arg(slot)));
}

int AniAttach::target_count(const DzNode *prop)
{
	int n = 0;
	while (target_property(prop, n + 1))
		++n;
	return n;
}

DzNode *AniAttach::target_node(const DzNode *prop, int slot)
{
	const DzNodeProperty *p = target_property(prop, slot);
	return p ? p->getValue() : nullptr;
}

bool AniAttach::is_carrier(const DzNode *node)
{
	return node && node->findProperty(carrier_mark);
}

DzNode *AniAttach::carrier_of(const DzNode *prop)
{
	DzNode *parent = prop ? prop->getNodeParent() : nullptr;
	return is_carrier(parent) ? parent : nullptr;
}

bool AniAttach::is_candidate(const DzNode *node)
{
	return node && !qobject_cast<const DzBone*>(node) && !is_carrier(node);
}

bool AniAttach::uses_aniattach(const DzNode *node)
{
	return held_property(node) != nullptr;
}

DzNode *AniAttach::prop_for(DzNode *node)
{
	if (is_carrier(node))
	{
		for (int i = 0; i < node->getNumNodeChildren(); ++i)
			if (uses_aniattach(node->getNodeChild(i)))
				return node->getNodeChild(i);
		return nullptr;
	}
	return node;
}

int AniAttach::slot_at(const DzEnumProperty *held, DzTime time)
{
	if (!held)
		return 0;
	const int count = held->getNumKeys();
	if (count == 0)
		return held->getValue();
	// Last key at or before `time`; before the first key, the first key.
	int lo = 0, hi = count - 1, found = 0;
	while (lo <= hi)
	{
		const int mid = (lo + hi) / 2;
		if (held->getKeyTime(mid) <= time)
		{
			found = mid;
			lo = mid + 1;
		}
		else
			hi = mid - 1;
	}
	return held->getKeyValue(found);
}

bool AniAttach::has_holds(const DzEnumProperty *held)
{
	if (!held)
		return false;
	const int count = held->getNumKeys();
	if (count == 0)
		return held->getValue() > 0;
	for (int k = 0; k < count; ++k)
		if (held->getKeyValue(k) > 0)
			return true;
	return false;
}

QList<AniAttachSegment> AniAttach::segments(const DzNode *prop)
{
	QList<AniAttachSegment> list;
	const DzEnumProperty *held = held_property(prop);
	if (!held || !scene_usable())
		return list;
	DzTime start, end, step;
	anim_range(start, end, step);
	const DzTime stop = end + step;

	auto add = [&](DzTime from, int slot) {
		from = qBound(start, from, stop);
		if (!list.isEmpty())
		{
			if (list.last().slot == slot)
				return;
			list.last().end = from;
			if (list.last().end <= list.last().start)
				list.removeLast();
			if (!list.isEmpty() && list.last().slot == slot)
				return;
		}
		AniAttachSegment s;
		s.start = from;
		s.end = stop;
		s.slot = slot;
		s.target = slot > 0 ? target_node(prop, slot) : nullptr;
		list.append(s);
	};

	const int count = held->getNumKeys();
	if (count == 0)
		add(start, held->getValue());
	else
	{
		add(start, held->getKeyValue(0));
		for (int k = 0; k < count; ++k)
			add(held->getKeyTime(k), held->getKeyValue(k));
	}
	if (!list.isEmpty() && list.last().end <= list.last().start)
		list.removeLast();
	return list;
}

QString AniAttach::describe(const DzNode *target)
{
	if (!target)
		return tr("(missing)");
	if (const DzBone *bone = qobject_cast<const DzBone*>(target))
		if (const DzSkeleton *skel = bone->getSkeleton())
			return QString("%1: %2").arg(skel->getLabel(), bone->getLabel());
	return target->getLabel();
}

QList<DzNode*> AniAttach::selected_props(const DzNode *holder)
{
	QList<DzNode*> list;
	if (!scene_usable())
		return list;
	for (DzNode *node : dzScene->getSelectedNodeList())
	{
		DzNode *prop = prop_for(node);
		if (!is_candidate(prop) || prop == holder || list.contains(prop))
			continue;
		// Not the figure (or anything else) the holder belongs to.
		if (holder && is_inside(holder, prop))
			continue;
		list.append(prop);
	}
	return list;
}

QList<DzNode*> AniAttach::hands()
{
	QList<DzNode*> list;
	if (!scene_usable())
		return list;
	DzSkeletonListIterator it = dzScene->skeletonListIterator();
	while (it.hasNext())
	{
		DzSkeleton *skel = it.next();
		// Clothing and hair follow a figure and have its bones too.
		if (!skel || skel->getFollowTarget())
			continue;
		for (const char *name : { "r_hand", "rHand", "l_hand", "lHand" })
			if (DzBone *bone = skel->findBone(name))
				if (!list.contains(bone))
					list.append(bone);
	}
	return list;
}

// ---------------------------------------------------------------------------
// setup

AniAttach *AniAttach::instance()
{
	// Never deleted: Daz tears down the scene and unloads plugins in its own
	// order on exit. The OS frees it.
	static AniAttach *core = new AniAttach();
	return core;
}

AniAttach::AniAttach()
{
	bake_timer_ = new QTimer(this);
	bake_timer_->setSingleShot(true);
	bake_timer_->setInterval(c_bake_delay_ms);
	connect(bake_timer_, &QTimer::timeout, this, &AniAttach::run_scheduled_bakes);
	hook_scene();
}

void AniAttach::hook_scene()
{
	if (hooked_)
		return;
	if (!dzScene)
	{
		QTimer::singleShot(250, this, &AniAttach::hook_scene);
		return;
	}
	hooked_ = true;
	connect(dzScene, &DzScene::sceneLoaded, this, &AniAttach::schedule_rescan);
	connect(dzScene, &DzScene::nodeListChanged, this, &AniAttach::schedule_rescan);
	connect(dzScene, &DzScene::sceneCleared, this, [this]() {
		tracked_.clear();
		pending_bakes_.clear();
		for (auto &list : own_connections_)
			for (const auto &c : list) QObject::disconnect(c);
		for (auto &list : chain_connections_)
			for (const auto &c : list) QObject::disconnect(c);
		own_connections_.clear();
		chain_connections_.clear();
		notify();
	});
	connect(dzScene, &DzScene::playbackFinished, this, [this]() {
		if (!pending_bakes_.isEmpty())
			bake_timer_->start();
	});
	schedule_rescan();
}

void AniAttach::notify()
{
	if (notify_scheduled_)
		return;
	notify_scheduled_ = true;
	QTimer::singleShot(0, this, [this]() {
		notify_scheduled_ = false;
		if (scene_usable())
			emit changed();
	});
}

void AniAttach::schedule_rescan()
{
	if (rescan_scheduled_)
		return;
	rescan_scheduled_ = true;
	QTimer::singleShot(0, this, &AniAttach::rescan);
}

void AniAttach::rescan()
{
	rescan_scheduled_ = false;
	if (!scene_usable())
		return;
	// Saved AniAttach properties only exist once a file is fully loaded.
	if (dzScene->isLoading())
	{
		rescan_scheduled_ = true;
		QTimer::singleShot(200, this, &AniAttach::rescan);
		return;
	}

	// Forget deleted props.
	for (auto it = own_connections_.begin(); it != own_connections_.end();)
	{
		bool alive = false;
		for (const QPointer<DzNode> &p : tracked_)
			if (p == it.key())
				alive = true;
		if (!alive)
		{
			for (const auto &c : it.value()) QObject::disconnect(c);
			for (const auto &c : chain_connections_.value(it.key())) QObject::disconnect(c);
			chain_connections_.remove(it.key());
			it = own_connections_.erase(it);
		}
		else
			++it;
	}
	tracked_.removeAll(QPointer<DzNode>());

	DzNodeListIterator nli = dzScene->nodeListIterator();
	while (nli.hasNext())
	{
		DzNode *node = nli.next();
		if (uses_aniattach(node))
		{
			track(node);
			watch_chains(node);
		}
	}
	notify();
}

void AniAttach::track(DzNode *prop)
{
	for (const QPointer<DzNode> &p : tracked_)
		if (p == prop)
			return;
	tracked_.append(prop);

	QList<QMetaObject::Connection> &conns = own_connections_[prop];
	QPointer<DzNode> guard(prop);
	if (DzEnumProperty *held = held_property(prop))
	{
		// Keys added, moved or deleted, also from the Timeline or the
		// Parameters pane.
		conns << connect(held, &DzProperty::valueChanged, this, [this, guard]() {
			if (guard && !baking_)
			{
				schedule_bake(guard);
				notify();
			}
		});
	}
	for (int slot = 1; slot <= target_count(prop); ++slot)
	{
		DzNodeProperty *tp = target_property(prop, slot);
		conns << connect(tp, &DzProperty::currentValueChanged, this, [this, guard]() {
			if (guard && !baking_)
			{
				update_items(guard);
				watch_chains(guard);
				schedule_bake(guard);
				notify();
			}
		});
	}
}

void AniAttach::untrack(DzNode *prop)
{
	for (const auto &c : own_connections_.take(prop)) QObject::disconnect(c);
	for (const auto &c : chain_connections_.take(prop)) QObject::disconnect(c);
	tracked_.removeAll(QPointer<DzNode>(prop));
	pending_bakes_.removeAll(QPointer<DzNode>(prop));
}

void AniAttach::watch_chains(DzNode *prop)
{
	for (const auto &c : chain_connections_.take(prop)) QObject::disconnect(c);
	DzNode *carrier = carrier_of(prop);
	if (!carrier)
		return;

	// Holders and their parents (a hand moves with the arm and the figure),
	// and the carrier's parents (e.g. a table that is moved).
	QList<DzNode*> movers;
	auto add_chain = [&](DzNode *n) {
		for (; n; n = n->getNodeParent())
			if (n != carrier && n != prop && !movers.contains(n))
				movers.append(n);
	};
	for (int slot = 1; slot <= target_count(prop); ++slot)
		add_chain(target_node(prop, slot));
	add_chain(carrier->getNodeParent());

	QList<QMetaObject::Connection> &conns = chain_connections_[prop];
	QPointer<DzNode> guard(prop);
	for (DzNode *n : movers)
		for (DzProperty *p : placement_controls(n))
			conns << connect(p, &DzProperty::valueChanged, this, [this, guard]() {
				if (guard && !baking_)
					schedule_bake(guard);
			});
}

QList<DzNode*> AniAttach::props() const
{
	QList<DzNode*> list;
	if (!scene_usable())
		return list;
	DzNodeListIterator nli = dzScene->nodeListIterator();
	while (nli.hasNext())
	{
		DzNode *node = nli.next();
		if (uses_aniattach(node))
			list.append(node);
	}
	return list;
}

DzEnumProperty *AniAttach::ensure_setup(DzNode *prop)
{
	DzEnumProperty *held = held_property(prop);
	if (!held)
	{
		held = new DzEnumProperty(held_name, true /*canAnimate*/, true /*isUserProperty*/);
		held->setLabel("Held By");
		held->setPath(c_group);
		held->setItems(QStringList() << "Free");
		prop->addProperty(held);
		held->setValue(0);
	}

	if (!carrier_of(prop))
	{
		auto carrier = new DzNode();
		carrier->setName("AniAttachCarrier");
		carrier->setLabel(prop->getLabel() + " (AniAttach)");
		auto mark = new DzBoolProperty(carrier_mark, false, true, true);
		mark->setLabel("AniAttach Carrier");
		mark->setPath(c_group);
		mark->setHidden(true);
		carrier->addProperty(mark);
		dzScene->addNode(carrier);

		// The carrier takes the prop's place in the hierarchy with an
		// identity transform, so the prop does not move.
		DzNode *parent = prop->getNodeParent();
		if (parent)
		{
			parent->removeNodeChild(prop, false);
			parent->addNodeChild(carrier, false);
		}
		carrier->addNodeChild(prop, false);
	}

	track(prop);
	return held;
}

int AniAttach::slot_for(DzNode *prop, DzNode *target)
{
	const int count = target_count(prop);
	for (int slot = 1; slot <= count; ++slot)
		if (target_node(prop, slot) == target)
			return slot;
	for (int slot = 1; slot <= count; ++slot)
		if (!target_node(prop, slot))
		{
			target_property(prop, slot)->setValue(target);
			update_items(prop);
			return slot;
		}

	const int slot = count + 1;
	auto tp = new DzNodeProperty(QString("%1%2").arg(target_prefix).arg(slot), false, true);
	tp->setLabel(QString("Hold Target %1").arg(slot));
	tp->setPath(c_group);
	prop->addProperty(tp);
	tp->setValue(target);
	// New property: follow its changes too.
	QPointer<DzNode> guard(prop);
	own_connections_[prop] << connect(tp, &DzProperty::currentValueChanged, this, [this, guard]() {
		if (guard && !baking_)
		{
			update_items(guard);
			watch_chains(guard);
			schedule_bake(guard);
			notify();
		}
	});
	update_items(prop);
	return slot;
}

void AniAttach::update_items(DzNode *prop)
{
	DzEnumProperty *held = held_property(prop);
	if (!held)
		return;
	QStringList items;
	items << "Free";
	for (int slot = 1; slot <= target_count(prop); ++slot)
	{
		DzNode *t = target_node(prop, slot);
		items << (t ? describe(t) : QString("Hold Target %1 (empty)").arg(slot));
	}
	QStringList current;
	for (int i = 0; i < held->getNumItems(); ++i)
		current << held->getItem(i);
	if (current != items)
		held->setItems(items);
}

// ---------------------------------------------------------------------------
// editing

void AniAttach::key_held(DzEnumProperty *held, DzTime time, int slot)
{
	DzTime start, end, step;
	anim_range(start, end, step);
	// First real key: free before it.
	if (!has_holds(held))
	{
		held->deleteAllKeys();
		if (time > start)
			held->setValue(start, 0);
	}
	held->setValue(time, slot);
	// Step keys, so Daz itself shows the same as AniAttach.
	for (int k = 0; k < held->getNumKeys(); ++k)
		held->setKeyInterpolationType(k, DzProperty::InterpConstant);
}

bool AniAttach::attach(DzNode *prop, DzNode *target, DzTime time)
{
	if (!scene_usable() || !is_candidate(prop) || !target || target == prop)
		return false;
	// A prop cannot be held by something inside itself (or its carrier).
	DzNode *carrier = carrier_of(prop);
	if (is_inside(target, prop) || (carrier && is_inside(target, carrier)))
		return false;

	dzUndoStack->beginHold();
	baking_ = true;
	DzEnumProperty *held = ensure_setup(prop);
	const int slot = slot_for(prop, target);
	key_held(held, time, slot);
	baking_ = false;
	bake_now(prop);
	dzUndoStack->accept("AniAttach Pick Up");
	notify();
	return true;
}

bool AniAttach::release(DzNode *prop, DzTime time)
{
	DzEnumProperty *held = held_property(prop);
	if (!scene_usable() || !held || !has_holds(held))
		return false;
	dzUndoStack->beginHold();
	baking_ = true;
	key_held(held, time, 0);
	baking_ = false;
	bake_now(prop);
	dzUndoStack->accept("AniAttach Put Down");
	notify();
	return true;
}

bool AniAttach::clear(DzNode *prop)
{
	DzEnumProperty *held = held_property(prop);
	if (!scene_usable() || !held)
		return false;
	dzUndoStack->beginHold();
	baking_ = true;
	held->deleteAllKeys();
	held->setValue(0);
	baking_ = false;
	bake_now(prop);
	dzUndoStack->accept("AniAttach Clear");
	notify();
	return true;
}

bool AniAttach::remove(DzNode *prop)
{
	DzEnumProperty *held = held_property(prop);
	if (!scene_usable() || !held)
		return false;

	dzUndoStack->beginHold();
	baking_ = true;
	held->deleteAllKeys();
	held->setValue(0);
	baking_ = false;
	bake_now(prop); // the carrier is back to identity: nothing moves below

	untrack(prop);
	if (DzNode *carrier = carrier_of(prop))
	{
		DzNode *parent = carrier->getNodeParent();
		carrier->removeNodeChild(prop, false);
		if (parent)
			parent->addNodeChild(prop, false);
		dzScene->removeNode(carrier);
	}
	for (int slot = target_count(prop); slot >= 1; --slot)
		prop->removeProperty(target_property(prop, slot));
	prop->removeProperty(held);
	dzUndoStack->accept("AniAttach Remove");
	notify();
	return true;
}

bool AniAttach::bake(DzNode *prop)
{
	return bake_now(prop);
}

int AniAttach::bake_all()
{
	int n = 0;
	for (DzNode *prop : props())
		if (bake_now(prop))
			++n;
	return n;
}

void AniAttach::schedule_bake(DzNode *prop)
{
	if (!scene_usable() || dzScene->isLoading())
		return;
	if (!pending_bakes_.contains(QPointer<DzNode>(prop)))
		pending_bakes_.append(prop);
	bake_timer_->start(); // restarts: wait until edits settle
}

void AniAttach::run_scheduled_bakes()
{
	if (!scene_usable())
		return;
	// Never move the timeline under playback or while a file loads.
	if (dzScene->isPlaying() || dzScene->isLoading())
	{
		if (!dzScene->isPlaying())
			bake_timer_->start();
		return;
	}
	const QList<QPointer<DzNode>> props = pending_bakes_;
	pending_bakes_.clear();
	for (const QPointer<DzNode> &p : props)
		if (p)
			bake_now(p);
}

// The bake. Walks the frames, keeping the carrier's world transform W:
//   before the first pick-up: identity (no keys moved),
//   held by T:   W(t) = G * T(t), with the grip G = W(t0 - 1) * T(t0)^-1
//                fixed at the pick-up frame t0 (Daz matrices: row vectors),
//   put down:    W(t) = W(t1 - 1), where the holder left it at t1.
// So nothing jumps at a pick-up or a put-down.
bool AniAttach::bake_now(DzNode *prop)
{
	if (!scene_usable() || dzScene->isLoading() || dzScene->isPlaying())
		return false;
	DzNode *carrier = carrier_of(prop);
	DzEnumProperty *held = held_property(prop);
	if (!carrier || !held)
		return false;

	baking_ = true;
	const QList<DzFloatProperty*> controls = transform_controls(carrier);
	for (DzFloatProperty *c : controls)
		c->deleteAllKeys();

	const QList<AniAttachSegment> segs = segments(prop);
	int first = -1;
	for (int i = 0; i < segs.size(); ++i)
		if (segs.at(i).slot > 0 && segs.at(i).target)
		{
			first = i;
			break;
		}
	if (first < 0)
	{
		// Never held: the carrier stays still. (Daz may store this as one
		// key at the current frame; one key is a constant.)
		for (DzFloatProperty *c : controls)
			c->setValue(0.0f);
		baking_ = false;
		notify();
		return true;
	}

	DzTime start, end, step;
	anim_range(start, end, step);
	const DzTime original_time = dzScene->getTime();
	DzNode *parent = carrier->getNodeParent();

	const DzTime t0 = segs.at(first).start;
	DzTime t = t0 > start ? t0 - step : start;

	// Identity up to the first pick-up. Keys at explicit times only: in
	// Daz 6 a plain setValue() after deleting the keys adds a key at the
	// current frame, which could land anywhere in the timeline.
	for (DzFloatProperty *c : controls)
		c->setValue(t, 0.0f);

	// The carrier's world transform just before the first pick-up.
	// Full world matrices (Daz: row vectors, child world = local * parent
	// world): a bone's matrix also carries its centre point and orientation,
	// which its position and rotation alone do not.
	dzScene->setTime(t);
	DzMatrix3 prev = carrier->getWSTransform();
	if (t < t0)
		t += step;

	enum Mode { Still, Held, Placed } mode = Still;
	DzNode *holder = nullptr;
	DzMatrix3 grip(true), placed(true), prev_parent(true);
	int seg_index = first;
	int last_seg = -1;

	auto moved = [](const DzMatrix3 &a, const DzMatrix3 &b) {
		for (int r = 0; r < 4; ++r)
			if ((a.row(r) - b.row(r)).length() > 1e-4f)
				return true;
		return false;
	};

	for (; t <= end; t += step)
	{
		dzScene->setTime(t);
		while (seg_index + 1 < segs.size() && segs.at(seg_index + 1).start <= t)
			++seg_index;
		const AniAttachSegment &seg = segs.at(seg_index);
		DzNode *target = seg.slot > 0 ? seg.target.data() : nullptr;

		const bool new_segment = seg_index != last_seg;
		last_seg = seg_index;
		if (new_segment)
		{
			if (target)
			{
				// Picked up (or handed over): keep where it is relative to
				// the new holder.
				grip = prev * target->getWSTransform().inverse();
				holder = target;
				mode = Held;
			}
			else if (mode == Held)
			{
				placed = prev;
				mode = Placed;
			}
		}

		DzMatrix3 world;
		bool write = true;
		const DzMatrix3 parent_now = parent ? parent->getWSTransform() : DzMatrix3(true);
		if (mode == Held)
			world = grip * holder->getWSTransform();
		else
		{
			world = placed;
			// Placed on something still: keys only at the ends of the stretch.
			const bool last_frame = t + step > end ||
				(seg_index + 1 < segs.size() && segs.at(seg_index + 1).start <= t + step);
			write = new_segment || last_frame || moved(parent_now, prev_parent);
		}
		prev_parent = parent_now;
		if (write)
			carrier->setWSTransform(t, world.row(3), DzQuat(world), carrier->getWSScale());
		prev = world;
	}

	// Linear between the per-frame keys, and no 360-degree spins between
	// two keys that are really close (Euler angles wrap around).
	for (int i = 0; i < controls.size(); ++i)
	{
		DzFloatProperty *c = controls.at(i);
		const int n = c->getNumKeys();
		for (int k = 0; k < n; ++k)
		{
			if (i >= 3 && k > 0)
			{
				const double before = c->getKeyValue(k - 1);
				double v = c->getKeyValue(k);
				while (v - before > 180.0) v -= 360.0;
				while (v - before < -180.0) v += 360.0;
				if (v != c->getKeyValue(k))
					c->setKeyValue(k, float(v));
			}
			c->setKeyInterpolationType(k, DzProperty::InterpLinear);
		}
	}

	dzScene->setTime(original_time);
	baking_ = false;
	watch_chains(prop);
	notify();
	return true;
}
