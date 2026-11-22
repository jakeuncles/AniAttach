/**********************************************************************
	AniAttach for Daz Studio 6 - Edit > AniAttach menu.

	This source code and software provided under the terms of the
	Mozilla Public License Version 2.0 (MPL) a copy of which is
	provided with it.
**********************************************************************/

#include "aniattachaction.h"

#include "dzapp.h"
#include "dznode.h"
#include "dzscene.h"

#include "aniattachcore.h"

namespace {

void status(const QString &message)
{
	dzApp->statusLine("AniAttach: " + message, true);
}

AniAttach *core() { return AniAttach::instance(); }

QList<DzNode*> props_with_aniattach()
{
	QList<DzNode*> list;
	for (DzNode *prop : AniAttach::selected_props())
		if (AniAttach::uses_aniattach(prop))
			list.append(prop);
	if (list.isEmpty())
		status("select one or more props that use AniAttach first.");
	return list;
}

} // namespace

DzAniAttachBaseAction::DzAniAttachBaseAction(const QString &text, const QString &desc) :
	DzEditAction(text, desc)
{
	// Follow saved scenes from the start, also without using a menu item.
	core();
}

QString DzAniAttachBaseAction::getDefaultMenuPath() const
{
	return "&Edit/AniAttach";
}

QStringList DzAniAttachBaseAction::getDefaultMenuPaths() const
{
	return QStringList() << getDefaultMenuPath() << "DzHierarchyPane::Edit/AniAttach";
}

// ---------------------------------------------------------------------------

DzAniAttachPickUpAction::DzAniAttachPickUpAction() :
	DzAniAttachBaseAction("Pick Up Here (by Last Selected)",
		"Select the prop(s), then Ctrl+click the hand (or other item) that holds them: "
		"they are held from the current frame on, keeping their current grip")
{
}

void DzAniAttachPickUpAction::executeAction()
{
	DzNode *holder = dzScene->getPrimarySelection();
	const QList<DzNode*> props = AniAttach::selected_props(holder);
	if (!holder || props.isEmpty())
	{
		status("select the prop(s), then Ctrl+click the hand that picks them up.");
		return;
	}
	int n = 0;
	for (DzNode *prop : props)
		if (core()->attach(prop, holder, dzScene->getTime()))
			++n;
	status(QString("%1 prop(s) held by %2 from this frame on.").arg(n).arg(AniAttach::describe(holder)));
}

DzAniAttachPutDownAction::DzAniAttachPutDownAction() :
	DzAniAttachBaseAction("Put Down Here",
		"The selected props are let go at the current frame and stay where they are")
{
}

void DzAniAttachPutDownAction::executeAction()
{
	int n = 0;
	for (DzNode *prop : props_with_aniattach())
		if (core()->release(prop, dzScene->getTime()))
			++n;
	if (n)
		status(QString("%1 prop(s) put down at this frame.").arg(n));
}

DzAniAttachClearAction::DzAniAttachClearAction() :
	DzAniAttachBaseAction("Clear Pick-Ups",
		"Delete the pick-ups and put-downs of the selected props; they stay where they were first placed")
{
}

void DzAniAttachClearAction::executeAction()
{
	int n = 0;
	for (DzNode *prop : props_with_aniattach())
		if (core()->clear(prop))
			++n;
	if (n)
		status(QString("cleared %1 prop(s).").arg(n));
}

DzAniAttachBakeAction::DzAniAttachBakeAction() :
	DzAniAttachBaseAction("Rebake All",
		"Recompute how every AniAttach prop follows its holders (done automatically when you edit)")
{
}

void DzAniAttachBakeAction::executeAction()
{
	status(QString("rebaked %1 prop(s).").arg(core()->bake_all()));
}

DzAniAttachRemoveAction::DzAniAttachRemoveAction() :
	DzAniAttachBaseAction("Remove AniAttach",
		"Take AniAttach off the selected props: they go back to where they were first placed")
{
}

void DzAniAttachRemoveAction::executeAction()
{
	int n = 0;
	for (DzNode *prop : props_with_aniattach())
		if (core()->remove(prop))
			++n;
	if (n)
		status(QString("removed from %1 prop(s).").arg(n));
}
