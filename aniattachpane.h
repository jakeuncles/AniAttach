/**********************************************************************
	AniAttach for Daz Studio 6 - the AniAttach Timeline pane.

	This source code and software provided under the terms of the
	Mozilla Public License Version 2.0 (MPL) a copy of which is
	provided with it.
**********************************************************************/
#pragma once

#include <QtCore/QList>
#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include "dzaction.h"
#include "dzpane.h"
#include "dztypes.h"

class QComboBox;
class QLabel;
class QScrollBar;
class DzNode;

// Draws a frame ruler and, per prop, who holds it over the animation:
// grey = free (on its own), colours = held, yellow diamonds = pick-up and
// put-down keys, white line = the current frame. Plain painting.
class DzAniAttachTimelineView : public QWidget {
	Q_OBJECT
public:
	explicit DzAniAttachTimelineView(QWidget *parent = nullptr);

	void set_rows(const QList<QPointer<DzNode>> &rows);
	void set_scroll(int first_row);
	int  row_count() const { return rows_.size(); }
	int  visible_rows() const;

signals:
	void scroll_requested(int delta_rows);

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void wheelEvent(QWheelEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;

private:
	QRect bars_rect() const;
	void layout_columns();
	void scrub_to(int x);
	int row_at(int y) const;

	QList<QPointer<DzNode>> rows_;
	int first_row_ = 0;
	int name_width_ = 160;
	bool scrubbing_ = false;
};

// Window > Panes (Tabs) > AniAttach Timeline. Dock it next to the Timeline.
class DzAniAttachPane : public DzPane {
	Q_OBJECT
public:
	DzAniAttachPane();
	~DzAniAttachPane() override;

	Q_SLOT void refresh() override;

protected:
	void showEvent(QShowEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;

private:
	void connect_scene();
	bool alive() const;
	void schedule_rebuild();
	void rebuild();
	void rebuild_holders();
	void repaint_view();
	void update_scrollbar();
	DzNode *chosen_holder() const;
	void pick_up();

	DzAniAttachTimelineView *view_ = nullptr;
	QScrollBar *scroll_ = nullptr;
	QComboBox *holder_ = nullptr;
	QLabel *summary_ = nullptr;
	QList<QPointer<DzNode>> holders_;   // combo index -> hand
	QList<QMetaObject::Connection> connections_;
	bool rebuild_scheduled_ = false;
	bool dirty_ = true;
	bool closing_ = false;
};

class DzAniAttachPaneAction : public DzPaneAction {
	Q_OBJECT
public:
	DzAniAttachPaneAction() : DzPaneAction(DzAniAttachPane::staticMetaObject.className()) {}
};
