/**********************************************************************
	AniAttach for Daz Studio 6 - the AniAttach Timeline pane.

	This source code and software provided under the terms of the
	Mozilla Public License Version 2.0 (MPL) a copy of which is
	provided with it.
**********************************************************************/

#include "aniattachpane.h"

#include <cmath>
#include <functional>

#include <QtCore/QTimer>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QVBoxLayout>

#include "dzapp.h"
#include "dzbone.h"
#include "dzenumproperty.h"
#include "dznode.h"
#include "dzscene.h"
#include "dztimerange.h"
#include "dzundostack.h"

#include "aniattachcore.h"

namespace {

const QColor c_free(110, 110, 110);
const QColor c_key(255, 213, 79);
const QColor c_now(255, 255, 255);
// One colour per hold target.
const QColor c_holders[] = {
	QColor(66, 165, 245), QColor(255, 152, 0), QColor(171, 71, 188),
	QColor(38, 166, 154), QColor(236, 64, 122), QColor(124, 179, 66),
};

const int c_ruler_h = 22;
const int c_row_h = 22;
const int c_now_w = 150;
const int c_margin = 8;

AniAttach *core() { return AniAttach::instance(); }

bool scene_usable()
{
	return dzApp && !dzApp->isClosing() && dzScene;
}

// The frames shown: the play range, like Daz's own Timeline ruler. Start and
// end are snapped to whole frames.
void view_range(DzTime &start, DzTime &end, DzTime &step)
{
	const DzTimeRange range = dzScene->getPlayRange();
	step = qMax<DzTime>(1, dzScene->getTimeStep());
	start = (range.getStart() / step) * step;
	end = qMax((range.getEnd() / step) * step, start);
}

// The frame (index from `start`) a time falls on. A key between two frames
// takes effect on the next frame (AniAttach::slot_at and the bake only look
// at whole frames).
int frame_of(DzTime t, DzTime start, DzTime step, bool round_up)
{
	const DzTime d = t - start;
	if (round_up && d > 0)
		return int((d + step - 1) / step);
	return int(d >= 0 ? d / step : -((-d + step - 1) / step));
}

QColor slot_color(int slot)
{
	if (slot <= 0)
		return c_free;
	return c_holders[(slot - 1) % int(sizeof(c_holders) / sizeof(c_holders[0]))];
}

// Short name for inside a bar: "Right Hand" rather than "Genesis 9: Right Hand".
QString short_name(const DzNode *target)
{
	return target ? target->getLabel() : QObject::tr("(missing)");
}

void status(const QString &message)
{
	dzApp->statusLine("AniAttach: " + message, true);
}

} // namespace

// ---------------------------------------------------------------------------
// view

DzAniAttachTimelineView::DzAniAttachTimelineView(QWidget *parent) :
	QWidget(parent)
{
	setMinimumHeight(c_ruler_h + 2 * c_row_h);
	setMouseTracking(true); // a sideways cursor over keys: they can be dragged
	setFocusPolicy(Qt::NoFocus);
}

DzAniAttachTimelineView::~DzAniAttachTimelineView()
{
	end_key_drag();
}

int DzAniAttachTimelineView::frame_at(int x) const
{
	DzTime start, end, step;
	view_range(start, end, step);
	const QRect bars = bars_rect();
	const int frames = int((end - start) / step) + 1;
	// Each frame is one cell; this is the cell under x.
	return qBound(0, int(std::floor(double(x - bars.left()) * frames / bars.width())), frames - 1);
}

DzTime DzAniAttachTimelineView::time_of(int frame) const
{
	DzTime start, end, step;
	view_range(start, end, step);
	return start + DzTime(frame) * step;
}

int DzAniAttachTimelineView::key_at(const QPoint &pos, int row) const
{
	DzNode *node = (row >= 0 && row < rows_.size()) ? rows_.at(row).data() : nullptr;
	const DzEnumProperty *held = AniAttach::held_property(node);
	if (!held || !scene_usable() || pos.x() < bars_rect().left())
		return -1;
	DzTime start, end, step;
	view_range(start, end, step);
	const QRect bars = bars_rect();
	const int frames = int((end - start) / step) + 1;
	const double px_per_frame = double(bars.width()) / frames;
	const double reach = qMax(5.0, px_per_frame / 2);
	int best = -1;
	double best_d = reach + 1;
	for (int k = 0; k < held->getNumKeys(); ++k)
	{
		// The first "Free" key at the start is not drawn: not a real event.
		if (k == 0 && held->getKeyValue(k) == 0 && held->getKeyTime(k) <= start)
			continue;
		const int f = frame_of(held->getKeyTime(k), start, step, true);
		if (f < 0 || f >= frames)
			continue;
		const double d = std::abs(bars.left() + (f + 0.5) * px_per_frame - pos.x());
		if (d <= reach && d < best_d)
		{
			best = k;
			best_d = d;
		}
	}
	return best;
}

void DzAniAttachTimelineView::end_key_drag()
{
	if (drag_hold_ && scene_usable() && dzUndoStack)
	{
		if (drag_moved_)
		{
			// Bake now, inside the same undo step as the move.
			if (drag_prop_)
				core()->bake(drag_prop_);
			dzUndoStack->accept(tr("AniAttach Move Key"));
		}
		else
			dzUndoStack->drop();
	}
	drag_hold_ = false;
	drag_moved_ = false;
	drag_key_ = -1;
	drag_prop_ = nullptr;
	update();
}

void DzAniAttachTimelineView::set_rows(const QList<QPointer<DzNode>> &rows)
{
	rows_ = rows;
	first_row_ = qBound(0, first_row_, qMax(0, rows_.size() - 1));
	layout_columns();
	update();
}

void DzAniAttachTimelineView::set_scroll(int first_row)
{
	first_row_ = qMax(0, first_row);
	update();
}

int DzAniAttachTimelineView::visible_rows() const
{
	return qMax(1, (height() - c_ruler_h) / c_row_h);
}

void DzAniAttachTimelineView::layout_columns()
{
	const QFontMetrics fm(font());
	int widest = fm.horizontalAdvance(tr("Prop"));
	for (const QPointer<DzNode> &n : rows_)
		if (n)
			widest = qMax(widest, fm.horizontalAdvance(n->getLabel()));
	name_width_ = qBound(110, widest + 2 * c_margin, qMax(110, width() / 4));
}

void DzAniAttachTimelineView::resizeEvent(QResizeEvent *e)
{
	layout_columns();
	QWidget::resizeEvent(e);
}

QRect DzAniAttachTimelineView::bars_rect() const
{
	const int left = name_width_ + c_now_w;
	return QRect(left, 0, qMax(10, width() - left - c_margin), height());
}

int DzAniAttachTimelineView::row_at(int y) const
{
	if (y < c_ruler_h)
		return -1;
	const int r = first_row_ + (y - c_ruler_h) / c_row_h;
	return r < rows_.size() ? r : -1;
}

void DzAniAttachTimelineView::scrub_to(int x)
{
	if (!scene_usable())
		return;
	const DzTime t = time_of(frame_at(x));
	if (t != dzScene->getTime())
		dzScene->setTime(t);
}

void DzAniAttachTimelineView::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	const QPalette pal = palette();
	p.fillRect(rect(), pal.color(QPalette::Base));
	if (!scene_usable())
		return;

	DzTime start, end, step;
	view_range(start, end, step);
	const QRect bars = bars_rect();
	// One cell per frame. Ticks, numbers, key diamonds and the current-frame
	// line all sit on the middle of their frame's cell, and each cell is
	// colored with the holder the bake uses on that frame.
	const int frames = int((end - start) / step) + 1;
	const double px_per_frame = double(bars.width()) / frames;
	auto cell_left = [&](int f) { return bars.left() + f * px_per_frame; };
	auto cell_mid = [&](int f) { return bars.left() + (f + 0.5) * px_per_frame; };
	// The scene can sit between frames: show the frame being shown.
	const int now_frame = frame_of(dzScene->getTime(), start, step, false);
	const QColor text = pal.color(QPalette::Text);
	QColor dim = text;
	dim.setAlpha(130);

	// Ruler.
	p.fillRect(QRect(0, 0, width(), c_ruler_h), pal.color(QPalette::Window));
	p.setPen(dim);
	p.drawText(QRect(c_margin, 0, name_width_ - c_margin, c_ruler_h), Qt::AlignVCenter | Qt::AlignLeft, tr("Prop"));
	p.drawText(QRect(name_width_, 0, c_now_w, c_ruler_h), Qt::AlignVCenter | Qt::AlignLeft, tr("Now held by"));
	int label_every = 1;
	for (int k : {1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 5000})
	{
		label_every = k;
		if (k * px_per_frame >= 36)
			break;
	}
	const int first_frame = int(start / step);
	for (int f = 0; f < frames; ++f)
	{
		const int frame = first_frame + f;
		const double x = cell_mid(f);
		const bool major = frame % label_every == 0;
		if (!major && px_per_frame < 4)
			continue;
		p.drawLine(QPointF(x, c_ruler_h - (major ? 8 : 4)), QPointF(x, c_ruler_h - 1));
		if (major)
			p.drawText(QRectF(x - 20, 0, 40, c_ruler_h - 7), Qt::AlignHCenter | Qt::AlignVCenter, QString::number(frame));
	}

	if (rows_.isEmpty())
	{
		p.setPen(dim);
		p.drawText(rect().adjusted(c_margin, c_ruler_h, -c_margin, 0), Qt::AlignCenter | Qt::TextWordWrap,
			tr("No props use AniAttach yet.\nSelect a prop, choose the hand above, go to the frame "
			   "where the hand touches it and click Pick Up Here."));
	}

	const DzTime now = start + DzTime(qBound(0, now_frame, frames - 1)) * step;
	const QColor highlight = pal.color(QPalette::Highlight);
	const int shown = visible_rows() + 1;
	for (int i = 0; i < shown; ++i)
	{
		const int r = first_row_ + i;
		if (r >= rows_.size())
			break;
		DzNode *node = rows_.at(r).data();
		if (!node)
			continue;
		const int y = c_ruler_h + i * c_row_h;
		const QRect line(0, y, width(), c_row_h);

		DzNode *carrier = AniAttach::carrier_of(node);
		if (node->isSelected() || (carrier && carrier->isSelected()))
		{
			QColor h = highlight;
			h.setAlpha(110);
			p.fillRect(line, h);
		}
		else if (r % 2)
		{
			const QColor base = pal.color(QPalette::Base);
			p.fillRect(line, base.lightness() < 128 ? base.lighter(125) : base.darker(106));
		}

		// Name.
		p.setPen(text);
		const QRect name_rect(c_margin, y, name_width_ - c_margin, c_row_h);
		p.drawText(name_rect, Qt::AlignVCenter | Qt::AlignLeft,
			p.fontMetrics().elidedText(node->getLabel(), Qt::ElideRight, name_rect.width()));

		// Now held by.
		const DzEnumProperty *held = AniAttach::held_property(node);
		const int slot_now = AniAttach::slot_at(held, now);
		DzNode *holder_now = slot_now > 0 ? AniAttach::target_node(node, slot_now) : nullptr;
		p.setRenderHint(QPainter::Antialiasing, true);
		p.setPen(Qt::NoPen);
		p.setBrush(slot_color(slot_now));
		p.drawEllipse(QRect(name_width_, y + c_row_h / 2 - 5, 10, 10));
		p.setPen(text);
		const QRect now_rect(name_width_ + 16, y, c_now_w - 20, c_row_h);
		p.drawText(now_rect, Qt::AlignVCenter | Qt::AlignLeft,
			p.fontMetrics().elidedText(slot_now > 0 ? AniAttach::describe(holder_now) : tr("Free"),
				Qt::ElideRight, now_rect.width()));

		// Bar: every frame colored with its holder (slot_at, the same rule
		// as the bake), merged into runs of the same holder.
		const QRect strip(bars.left(), y + 4, bars.width(), c_row_h - 8);
		QPainterPath clip;
		clip.addRoundedRect(strip, 3, 3);
		p.save();
		p.setClipPath(clip);
		int run_start = 0;
		int run_slot = AniAttach::slot_at(held, start);
		for (int f = 1; f <= frames; ++f)
		{
			const int s = (f < frames) ? AniAttach::slot_at(held, start + DzTime(f) * step) : -1;
			if (s == run_slot)
				continue;
			const double x0 = cell_left(run_start), x1 = cell_left(f);
			const QRectF r(x0, strip.top(), x1 - x0, strip.height());
			QColor c = slot_color(run_slot);
			if (run_slot <= 0)
				c.setAlpha(120);
			p.fillRect(r, c);
			if (run_slot > 0 && r.width() > 30)
			{
				p.setPen(QColor(255, 255, 255));
				p.drawText(r.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft,
					p.fontMetrics().elidedText(short_name(AniAttach::target_node(node, run_slot)),
						Qt::ElideRight, int(r.width()) - 8));
			}
			run_start = f;
			run_slot = s;
		}
		p.restore();

		// Keys.
		if (held)
		{
			p.setBrush(c_key);
			p.setPen(QPen(QColor(40, 40, 40), 1));
			const double cy = strip.center().y() + 0.5;
			for (int k = 0; k < held->getNumKeys(); ++k)
			{
				// The first "Free" key at the start is not a real event.
				if (k == 0 && held->getKeyValue(k) == 0 && held->getKeyTime(k) <= start)
					continue;
				// A key takes effect on its frame (or the next frame when it
				// sits between frames): draw it there.
				const int f = frame_of(held->getKeyTime(k), start, step, true);
				if (f < 0 || f >= frames)
					continue;
				const double cx = cell_mid(f);
				const bool dragged = drag_hold_ && node == drag_prop_ && k == drag_key_;
				const double r = dragged ? 7 : 5;
				QPolygonF diamond;
				diamond << QPointF(cx, cy - r) << QPointF(cx + r - 1, cy) << QPointF(cx, cy + r) << QPointF(cx - r + 1, cy);
				if (dragged)
				{
					p.setBrush(c_now);
					p.drawPolygon(diamond);
					// The frame it will land on.
					p.setPen(text);
					p.drawText(QRectF(cx + 8, y, 60, c_row_h), Qt::AlignVCenter | Qt::AlignLeft,
						QString::number(first_frame + f));
					p.setPen(QPen(QColor(40, 40, 40), 1));
					p.setBrush(c_key);
				}
				else
					p.drawPolygon(diamond);
			}
		}
		p.setRenderHint(QPainter::Antialiasing, false);
	}

	// Current frame.
	if (now_frame < 0 || now_frame >= frames)
		return; // outside the play range, like Daz's own Timeline
	const double nx = cell_mid(now_frame);
	p.setPen(QPen(c_now, 2));
	p.drawLine(QPointF(nx, 0), QPointF(nx, height()));
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setPen(Qt::NoPen);
	p.setBrush(c_now);
	QPolygonF marker;
	marker << QPointF(nx - 5, 0) << QPointF(nx + 5, 0) << QPointF(nx, 7);
	p.drawPolygon(marker);
}

void DzAniAttachTimelineView::mousePressEvent(QMouseEvent *e)
{
	if (e->button() != Qt::LeftButton || !scene_usable())
		return;
	const QPoint pos = e->position().toPoint();
	const int key_row = row_at(pos.y());
	const int key = key_at(pos, key_row);
	if (key >= 0 && dzUndoStack)
	{
		// Press on a key: drag it (one undo step). Without moving, the click
		// only goes to its frame.
		drag_prop_ = rows_.at(key_row).data();
		drag_key_ = key;
		drag_moved_ = false;
		drag_hold_ = true;
		dzUndoStack->beginHold();
		update();
		return;
	}
	if (pos.x() >= name_width_)
	{
		scrubbing_ = true;
		scrub_to(qMax(pos.x(), bars_rect().left()));
		return;
	}
	const int r = row_at(pos.y());
	DzNode *node = r >= 0 ? rows_.at(r).data() : nullptr;
	if (!node)
		return;
	if (e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))
		node->select(!node->isSelected());
	else
	{
		dzScene->selectAllNodes(false);
		node->select(true);
	}
}

void DzAniAttachTimelineView::mouseMoveEvent(QMouseEvent *e)
{
	const QPoint pos = e->position().toPoint();
	if (drag_hold_)
	{
		DzNode *prop = drag_prop_.data();
		const DzEnumProperty *held = AniAttach::held_property(prop);
		if (!prop || !held || !scene_usable() || drag_key_ >= held->getNumKeys())
		{
			end_key_drag();
			return;
		}
		const DzTime to = time_of(frame_at(pos.x()));
		if (to != held->getKeyTime(drag_key_))
		{
			// A frame that already has a key is skipped: the key stays put.
			const int moved = core()->move_key(prop, drag_key_, to);
			if (moved >= 0)
			{
				drag_key_ = moved;
				drag_moved_ = true;
				update();
			}
		}
		return;
	}
	if (scrubbing_)
	{
		scrub_to(pos.x());
		return;
	}
	setCursor(key_at(pos, row_at(pos.y())) >= 0 ? Qt::SizeHorCursor : Qt::ArrowCursor);
}

void DzAniAttachTimelineView::mouseReleaseEvent(QMouseEvent *e)
{
	scrubbing_ = false;
	if (!drag_hold_)
		return;
	const bool moved = drag_moved_;
	end_key_drag();
	if (!moved && scene_usable())
		scrub_to(e->position().toPoint().x()); // a click on a key: go to its frame
}

void DzAniAttachTimelineView::contextMenuEvent(QContextMenuEvent *e)
{
	if (!scene_usable() || drag_hold_)
		return;
	const QPoint pos = e->pos();
	const int row = row_at(pos.y());
	DzNode *prop = row >= 0 ? rows_.at(row).data() : nullptr;
	const bool on_timeline = pos.x() >= bars_rect().left();
	const int frame = frame_at(pos.x());
	const DzTime time = time_of(frame);
	DzTime start, end, step;
	view_range(start, end, step);
	const int frame_no = int(start / step) + frame;
	const int key = key_at(pos, row);

	QMenu menu(this);
	QPointer<DzNode> guard(prop);
	const DzEnumProperty *held = AniAttach::held_property(prop);

	if (key >= 0 && held)
	{
		const int key_frame = frame_of(held->getKeyTime(key), start, step, true) + int(start / step);
		const int slot = held->getKeyValue(key);
		menu.addSection(tr("Key at frame %1").arg(key_frame));
		QAction *free = menu.addAction(tr("Put Down (Free)"));
		free->setCheckable(true);
		free->setChecked(slot == 0);
		connect(free, &QAction::triggered, this, [guard, key]() {
			if (guard && scene_usable()) core()->set_key_slot(guard, key, 0);
		});
		for (int s = 1; s <= AniAttach::target_count(prop); ++s)
		{
			DzNode *t = AniAttach::target_node(prop, s);
			if (!t)
				continue;
			QAction *a = menu.addAction(tr("Held by %1").arg(AniAttach::describe(t)));
			a->setCheckable(true);
			a->setChecked(slot == s);
			connect(a, &QAction::triggered, this, [guard, key, s]() {
				if (guard && scene_usable()) core()->set_key_slot(guard, key, s);
			});
		}
		menu.addSeparator();
		connect(menu.addAction(tr("Delete Key")), &QAction::triggered, this, [guard, key]() {
			if (guard && scene_usable()) core()->delete_key(guard, key);
		});
		const DzTime key_time = start + DzTime(frame_of(held->getKeyTime(key), start, step, true)) * step;
		connect(menu.addAction(tr("Go to Frame %1").arg(key_frame)), &QAction::triggered, this, [key_time]() {
			if (scene_usable()) dzScene->setTime(key_time);
		});
	}
	else if (on_timeline)
	{
		if (prop)
		{
			menu.addSection(prop->getLabel());
			// Holders: the figures' hands, plus anything that held it before.
			QList<DzNode*> holders = AniAttach::hands();
			for (int s = 1; s <= AniAttach::target_count(prop); ++s)
				if (DzNode *t = AniAttach::target_node(prop, s))
					if (!holders.contains(t))
						holders.append(t);
			QMenu *pick = menu.addMenu(tr("Pick Up at Frame %1 by").arg(frame_no));
			for (DzNode *h : holders)
			{
				QPointer<DzNode> hg(h);
				connect(pick->addAction(AniAttach::describe(h)), &QAction::triggered, this, [guard, hg, time]() {
					if (guard && hg && scene_usable()) core()->attach(guard, hg, time);
				});
			}
			pick->setEnabled(!holders.isEmpty());
			QAction *put = menu.addAction(tr("Put Down at Frame %1").arg(frame_no));
			put->setEnabled(AniAttach::has_holds(held));
			connect(put, &QAction::triggered, this, [guard, time]() {
				if (guard && scene_usable()) core()->release(guard, time);
			});
		}
		connect(menu.addAction(tr("Go to Frame %1").arg(frame_no)), &QAction::triggered, this, [time]() {
			if (scene_usable()) dzScene->setTime(time);
		});
	}

	if (prop)
	{
		menu.addSeparator();
		connect(menu.addAction(tr("Select %1").arg(prop->getLabel())), &QAction::triggered, this, [guard]() {
			if (!guard || !scene_usable()) return;
			dzScene->selectAllNodes(false);
			guard->select(true);
		});
		connect(menu.addAction(tr("Clear Pick-Ups")), &QAction::triggered, this, [guard]() {
			if (guard && scene_usable()) core()->clear(guard);
		});
		connect(menu.addAction(tr("Rebake")), &QAction::triggered, this, [guard]() {
			if (guard && scene_usable()) core()->bake(guard);
		});
		connect(menu.addAction(tr("Remove AniAttach")), &QAction::triggered, this, [guard]() {
			if (guard && scene_usable()) core()->remove(guard);
		});
	}

	if (!menu.isEmpty())
		menu.exec(e->globalPos());
	e->accept();
}

void DzAniAttachTimelineView::wheelEvent(QWheelEvent *e)
{
	const int steps = e->angleDelta().y() / 120;
	if (steps)
		emit scroll_requested(-steps * 3);
	e->accept();
}

// ---------------------------------------------------------------------------
// pane

DzAniAttachPane::DzAniAttachPane() :
	DzPane(tr("AniAttach Timeline"))
{
	setObjectName("AniAttachTimelinePane");

	auto layout = new QVBoxLayout();
	layout->setContentsMargins(4, 4, 4, 4);
	layout->setSpacing(4);

	auto buttons = new QHBoxLayout();
	buttons->setSpacing(4);
	buttons->addWidget(new QLabel(tr("Holder:")));
	holder_ = new QComboBox();
	holder_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	holder_->setToolTip(tr("What picks the selected props up: a hand of a figure in the scene, "
		"or the item you selected last (Ctrl+click it after the props)"));
	buttons->addWidget(holder_);
	auto make_button = [&](const QString &text, const QString &tip) {
		auto b = new QPushButton(text);
		b->setToolTip(tip);
		buttons->addWidget(b);
		return b;
	};
	QPushButton *pick = make_button(tr("Pick Up Here"),
		tr("The holder picks the selected props up at the current frame, keeping their current grip"));
	QPushButton *put = make_button(tr("Put Down Here"),
		tr("The selected props are let go at the current frame and stay where they are"));
	buttons->addSpacing(8);
	QPushButton *clear = make_button(tr("Clear"),
		tr("Delete the pick-ups and put-downs of the selected props"));
	QPushButton *bake = make_button(tr("Rebake"),
		tr("Recompute how the props follow their holders (automatic when you edit; "
		   "use it after changing the animation with AniAttach not installed)"));
	QPushButton *remove = make_button(tr("Remove"),
		tr("Take AniAttach off the selected props"));
	buttons->addStretch(1);
	layout->addLayout(buttons);

	auto body = new QHBoxLayout();
	body->setSpacing(0);
	view_ = new DzAniAttachTimelineView();
	view_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	scroll_ = new QScrollBar(Qt::Vertical);
	body->addWidget(view_, 1);
	body->addWidget(scroll_);
	layout->addLayout(body, 1);

	summary_ = new QLabel();
	summary_->setWordWrap(true);
	summary_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	layout->addWidget(summary_);

	setLayout(layout);
	setMinimumSize(360, 160);
	resize(760, 240);

	connect(pick, &QPushButton::clicked, this, &DzAniAttachPane::pick_up);
	auto for_selected = [this](const std::function<bool(DzNode*)> &fn, const QString &done) {
		if (!alive())
			return;
		int n = 0;
		for (DzNode *prop : AniAttach::selected_props())
			if (AniAttach::uses_aniattach(prop) && fn(prop))
				++n;
		status(n ? done.arg(n) : tr("select one or more props that use AniAttach first."));
	};
	connect(put, &QPushButton::clicked, this, [for_selected]() {
		for_selected([](DzNode *p) { return core()->release(p, dzScene->getTime()); },
			tr("%1 prop(s) put down at this frame."));
	});
	connect(clear, &QPushButton::clicked, this, [for_selected]() {
		for_selected([](DzNode *p) { return core()->clear(p); }, tr("cleared %1 prop(s)."));
	});
	connect(remove, &QPushButton::clicked, this, [for_selected]() {
		for_selected([](DzNode *p) { return core()->remove(p); }, tr("removed from %1 prop(s)."));
	});
	connect(bake, &QPushButton::clicked, this, [this]() {
		if (alive())
			status(tr("rebaked %1 prop(s).").arg(core()->bake_all()));
	});
	connect(scroll_, &QScrollBar::valueChanged, view_, &DzAniAttachTimelineView::set_scroll);
	connect(view_, &DzAniAttachTimelineView::scroll_requested, this, [this](int delta) {
		scroll_->setValue(scroll_->value() + delta);
	});

	connect_scene();
}

DzAniAttachPane::~DzAniAttachPane()
{
	closing_ = true;
	for (const QMetaObject::Connection &c : connections_)
		QObject::disconnect(c);
	connections_.clear();
}

void DzAniAttachPane::connect_scene()
{
	if (closing_ || !dzApp || dzApp->isClosing())
		return;
	if (!dzScene)
	{
		QTimer::singleShot(250, this, &DzAniAttachPane::connect_scene);
		return;
	}
	if (!connections_.isEmpty())
		return;

	connections_
		<< connect(core(), &AniAttach::changed, this, &DzAniAttachPane::schedule_rebuild)
		<< connect(dzScene, &DzScene::nodeListChanged, this, &DzAniAttachPane::schedule_rebuild)
		<< connect(dzScene, &DzScene::skeletonListChanged, this, &DzAniAttachPane::schedule_rebuild)
		<< connect(dzScene, &DzScene::sceneLoaded, this, &DzAniAttachPane::schedule_rebuild)
		<< connect(dzScene, &DzScene::sceneCleared, this, &DzAniAttachPane::schedule_rebuild)
		<< connect(dzScene, &DzScene::timeChanged, this, &DzAniAttachPane::repaint_view)
		<< connect(dzScene, &DzScene::animRangeChanged, this, &DzAniAttachPane::repaint_view)
		<< connect(dzScene, &DzScene::playRangeChanged, this, &DzAniAttachPane::repaint_view)
		<< connect(dzScene, &DzScene::timeStepChanged, this, &DzAniAttachPane::repaint_view)
		<< connect(dzScene, &DzScene::nodeSelectionListChanged, this, &DzAniAttachPane::repaint_view)
		<< connect(dzScene, &QObject::destroyed, this, [this]() {
			closing_ = true;
			view_->set_rows(QList<QPointer<DzNode>>());
		});
	schedule_rebuild();
}

bool DzAniAttachPane::alive() const
{
	return !closing_ && !connections_.isEmpty() && scene_usable();
}

void DzAniAttachPane::refresh()
{
	schedule_rebuild();
}

void DzAniAttachPane::showEvent(QShowEvent *e)
{
	if (dirty_)
		schedule_rebuild();
	DzPane::showEvent(e);
}

void DzAniAttachPane::resizeEvent(QResizeEvent *e)
{
	DzPane::resizeEvent(e);
	update_scrollbar();
}

void DzAniAttachPane::schedule_rebuild()
{
	dirty_ = true;
	if (!alive() || rebuild_scheduled_ || !isVisible())
		return;
	rebuild_scheduled_ = true;
	QTimer::singleShot(0, this, &DzAniAttachPane::rebuild);
}

void DzAniAttachPane::repaint_view()
{
	if (alive() && isVisible())
		view_->update();
}

void DzAniAttachPane::rebuild()
{
	rebuild_scheduled_ = false;
	if (!alive())
		return;
	if (dzScene->isLoading())
	{
		rebuild_scheduled_ = true;
		QTimer::singleShot(250, this, &DzAniAttachPane::rebuild);
		return;
	}
	dirty_ = false;

	QList<QPointer<DzNode>> rows;
	int held = 0;
	for (DzNode *prop : core()->props())
	{
		rows.append(prop);
		if (AniAttach::has_holds(AniAttach::held_property(prop)))
			++held;
	}
	view_->set_rows(rows);
	update_scrollbar();
	rebuild_holders();

	if (rows.isEmpty())
		summary_->setText(tr("Select a prop, choose the holder, go to the frame where the hand "
			"touches it and click Pick Up Here. Later, at the frame where it is set down, click Put Down Here."));
	else
		summary_->setText(tr("%1 prop(s) with AniAttach, %2 picked up at some point. "
			"Click or drag the bars to move the timeline, drag a key to move it, right-click for more.")
			.arg(rows.size()).arg(held));
}

void DzAniAttachPane::rebuild_holders()
{
	DzNode *was = chosen_holder();
	const bool was_last = holder_->currentIndex() == holders_.size();
	holders_.clear();
	holder_->blockSignals(true);
	holder_->clear();
	for (DzNode *hand : AniAttach::hands())
	{
		holders_.append(hand);
		holder_->addItem(AniAttach::describe(hand));
	}
	holder_->addItem(tr("Last selected item"));
	int index = 0;
	if (was_last && !holders_.isEmpty() && !was)
		index = holders_.size();
	for (int i = 0; i < holders_.size(); ++i)
		if (holders_.at(i) == was)
			index = i;
	if (holders_.isEmpty())
		index = 0;
	holder_->setCurrentIndex(index);
	holder_->blockSignals(false);
}

DzNode *DzAniAttachPane::chosen_holder() const
{
	const int i = holder_->currentIndex();
	if (i >= 0 && i < holders_.size())
		return holders_.at(i).data();
	return (scene_usable() && i == holders_.size()) ? dzScene->getPrimarySelection() : nullptr;
}

void DzAniAttachPane::pick_up()
{
	if (!alive())
		return;
	DzNode *holder = chosen_holder();
	const QList<DzNode*> props = AniAttach::selected_props(holder);
	if (!holder)
	{
		status(tr("choose a holder, or select the props and then Ctrl+click the holder."));
		return;
	}
	if (props.isEmpty())
	{
		status(tr("select the prop(s) to pick up first."));
		return;
	}
	int n = 0;
	for (DzNode *prop : props)
		if (core()->attach(prop, holder, dzScene->getTime()))
			++n;
	status(tr("%1 prop(s) held by %2 from this frame on.").arg(n).arg(AniAttach::describe(holder)));
}

void DzAniAttachPane::update_scrollbar()
{
	const int visible = view_->visible_rows();
	scroll_->setRange(0, qMax(0, view_->row_count() - visible));
	scroll_->setPageStep(visible);
	scroll_->setVisible(view_->row_count() > visible);
}
