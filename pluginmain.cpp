/**********************************************************************
	AniAttach for Daz Studio 6 - plugin definition.

	This source code and software provided under the terms of the
	Mozilla Public License Version 2.0 (MPL) a copy of which is
	provided with it.
**********************************************************************/

#include "dzplugin.h"
#include "dzapp.h"

#include "version.h"
#include "aniattachaction.h"
#include "aniattachguids.h"
#include "aniattachpane.h"

DZ_PLUGIN_DEFINITION( "AniAttach - Pick Up and Put Down Props (Daz Studio 6)" );

DZ_PLUGIN_AUTHOR( "AniAttach contributors" );

DZ_PLUGIN_VERSION( ANIATTACH_MAJOR, ANIATTACH_MINOR, ANIATTACH_REV, ANIATTACH_BUILD );

DZ_PLUGIN_DESCRIPTION( QString(
	"Lets a hand (or any item) pick a prop up and put it down again over the timeline, "
	"for example a cup taken from a table and put back. "
	"See Window &gt; Panes (Tabs) &gt; AniAttach Timeline and Edit &gt; AniAttach. "
	"The result is ordinary keys, so scenes also open, render and export without the plugin." ) );

DZ_PLUGIN_CLASS_GUID( DzAniAttachPickUpAction, F4203EAD-AE74-4368-9DB8-3A45AD635F12 );
DZ_PLUGIN_CLASS_GUID( DzAniAttachPutDownAction, 68032786-0F2D-4684-943A-8B12A3E90A75 );
DZ_PLUGIN_CLASS_GUID( DzAniAttachClearAction, CDC13706-C22C-4C6A-B02C-57CD552AF3B6 );
DZ_PLUGIN_CLASS_GUID( DzAniAttachBakeAction, 47F28277-C9EC-47E9-BAD2-A1E885CD5B52 );
DZ_PLUGIN_CLASS_GUID( DzAniAttachRemoveAction, D3363B40-E350-4FED-A4BF-911A2A980D3C );
DZ_PLUGIN_CLASS_GUID( DzAniAttachPane, ANIATTACH_PANE_GUID );
DZ_PLUGIN_CLASS_GUID( DzAniAttachPaneAction, ANIATTACH_PANE_ACTION_GUID );
