/**********************************************************************
	AniAttach for Daz Studio 6 - Edit > AniAttach menu.

	This source code and software provided under the terms of the
	Mozilla Public License Version 2.0 (MPL) a copy of which is
	provided with it.
**********************************************************************/
#pragma once

#include "dzaction.h"

// Base for the AniAttach menu items: Edit > AniAttach and the Scene pane's
// right-click Edit menu.
class DzAniAttachBaseAction : public DzEditAction {
	Q_OBJECT
public:
	DzAniAttachBaseAction(const QString &text, const QString &desc);
protected:
	QString		getDefaultMenuPath() const override;
	QStringList	getDefaultMenuPaths() const override;
};

// Pick Up Here: the selected props are picked up by the item selected last
// (e.g. a hand) at the current frame.
class DzAniAttachPickUpAction : public DzAniAttachBaseAction {
	Q_OBJECT
public:
	DzAniAttachPickUpAction();
protected:
	void executeAction() override;
};

// Put Down Here: the selected props are put down at the current frame.
class DzAniAttachPutDownAction : public DzAniAttachBaseAction {
	Q_OBJECT
public:
	DzAniAttachPutDownAction();
protected:
	void executeAction() override;
};

// Clear Pick-Ups: delete the Held By keys of the selected props.
class DzAniAttachClearAction : public DzAniAttachBaseAction {
	Q_OBJECT
public:
	DzAniAttachClearAction();
protected:
	void executeAction() override;
};

// Rebake All: recompute every carrier (e.g. after changing the animation
// with the plugin off).
class DzAniAttachBakeAction : public DzAniAttachBaseAction {
	Q_OBJECT
public:
	DzAniAttachBakeAction();
protected:
	void executeAction() override;
};

// Remove AniAttach from the selected props.
class DzAniAttachRemoveAction : public DzAniAttachBaseAction {
	Q_OBJECT
public:
	DzAniAttachRemoveAction();
protected:
	void executeAction() override;
};
