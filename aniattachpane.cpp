/**********************************************************************
	AniAttach for Daz Studio 6 - the AniAttach Timeline pane.

	This source code and software provided under the terms of the
	Mozilla Public License Version 2.0 (MPL) a copy of which is
	provided with it.
**********************************************************************/

#include "aniattachpane.h"

#include <functional>

#include <QtCore/QTimer>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QVBoxLayout>

#include "dzapp.h"
#include "dzbone.h"
#include "dzenumproperty.h"
#include "dznode.h"
#include "dzscene.h"
#include "dztimerange.h"

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

void anim_range(DzTime &start, DzTime &end, DzTime &step)
{
	const DzTimeRange range = dzScene->getAnimRange();
	step = qMax<DzTime>(1, dzScene->getTimeStep());
	start = range.getStart();
	end = qMax(range.getEnd(), start);
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
	setFocusPolicy(Qt::NoFocus);
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
	DzTime start, end, step;
	anim_range(start, end, step);
	const QRect bars = bars_rect();
	const int frames = int((end - start) / step) + 1;
	const double f = qBound(0.0, double(x - bars.left()) / bars.width(), 0.999999);
	const DzTime t = start + DzTime(int(f * frames)) * step;
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
	anim_range(start, end, step);
	const QRect bars = bars_rect();
	const DzTime span = end + step - start;
	auto x_at = [&](DzTime t) {
		const double f = double(qBound<DzTime>(start, t, end + step) - start) / double(span);
		return bars.left() + f * bars.width();
	};
	const QColor text = pal.color(QPalette::Text);
	QColor dim = text;
	dim.setAlpha(130);

	// Ruler.
	p.fillRect(QRect(0, 0, width(), c_ruler_h), pal.color(QPalette::Window));
	p.setPen(dim);
	p.drawText(QRect(c_margin, 0, name_width_ - c_margin, c_ruler_h), Qt::AlignVCenter | Qt::AlignLeft, tr("Prop"));
	p.drawText(QRect(name_width_, 0, c_now_w, c_ruler_h), Qt::AlignVCenter | Qt::AlignLeft, tr("Now held by"));
	const int frames = int((end - start) / step) + 1;
	const double px_per_frame = double(bars.width()) / frames;
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
		const double x = bars.left() + f * px_per_frame;
		const bool major = frame % label_every == 0;
		if (!major && px_per_frame < 4)
			continue;
		p.drawLine(QPointF(x, c_ruler_h - (major ? 8 : 4)), QPointF(x, c_ruler_h - 1));
		if (major)
			p.drawText(QRectF(x + 2, 0, 40, c_ruler_h - 6), Qt::AlignLeft | Qt::AlignVCenter, QString::number(frame));
	}

	if (rows_.isEmpty())
	{
		p.setPen(dim);
		p.drawText(rect().adjusted(c_margin, c_ruler_h, -c_margin, 0), Qt::AlignCenter | Qt::TextWordWrap,
			tr("No props use AniAttach yet.\nSelect a prop, choose the hand above, go to the frame "
			   "where the hand touches it and click Pick Up Here."));
	}

	const DzTime now = dzScene->getTime();
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

		// Bar: one stretch per holder.
		const QRect strip(bars.left(), y + 4, bars.width(), c_row_h - 8);
		QPainterPath clip;
		clip.addRoundedRect(strip, 3, 3);
		p.save();
		p.setClipPath(clip);
		const QList<AniAttachSegment> segs = AniAttach::segments(node);
		for (const AniAttachSegment &s : segs)
		{
			const double x0 = x_at(s.start), x1 = x_at(s.end);
			const QRectF r(x0, strip.top(), x1 - x0, strip.height());
			QColor c = slot_color(s.slot);
			if (s.slot <= 0)
				c.setAlpha(120);
			p.fillRect(r, c);
			if (s.slot > 0 && r.width() > 30)
			{
				p.setPen(QColor(255, 255, 255));
				p.drawText(r.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft,
					p.fontMetrics().elidedText(short_name(s.target), Qt::ElideRight, int(r.width()) - 8));
			}
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
				const DzTime t = held->getKeyTime(k);
				if (t < start || t > end)
					continue;
				// The first "Free" key at the start is not a real event.
				if (k == 0 && held->getKeyValue(k) == 0 && t == start)
					continue;
				const double cx = x_at(t) + 1;
				QPolygonF diamond;
				diamond << QPointF(cx, cy - 5) << QPointF(cx + 4, cy) << QPointF(cx, cy + 5) << QPointF(cx - 4, cy);
				p.drawPolygon(diamond);
			}
		}
		p.setRenderHint(QPainter::Antialiasing, false);
	}

	// Current frame.
	const double nx = x_at(now) + 0.5 * px_per_frame;
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
	if (scrubbing_)
		scrub_to(e->position().toPoint().x());
}

void DzAniAttachTimelineView::mouseReleaseEvent(QMouseEvent *)
{
	scrubbing_ = false;
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
			"Click or drag the bars to move the timeline.").arg(rows.size()).arg(held));
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
