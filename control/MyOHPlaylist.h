/**
 * Copyright (C) Albis Technologies AG 2015-2016
 * All Rights Reserved
 * -OWNER-----------------------------------------------------------------------
 * Author      : Michael Schenk
 *
 * -HISTORY---------------------------------------------------------------------
 *
 * -ISSUES---------------------------------------------------------------------_
 */
#pragma once

/* Platinum/Neptune UPnP SDK includes */
#include <Neptune.h>
#include <PltOHPlaylist.h>
#include <PltService.h>
/* local includes */
#include <MyPLTController.h>

/**
 *
 */
class MyOHPlaylist : public IMyPLTController, public PLT_OHPlaylist
{
	public:
		MyOHPlaylist(std::shared_ptr<IRenderer> renderer, void* ctx ,const char* friendly_name,
					 bool show_ip = false, const char* uuid = NULL,
					 unsigned int port = 0);

		virtual ~MyOHPlaylist();

		/* render interface */

		virtual const char* getName() { return "MyOHPlaylist"; }

		/**
		 * Called by the renderer (decoder) if synchronized
		 * stuff has changed (mute, volume, shuffle, repeat).
		 */
		virtual void RendererChanges(SynchronizedStatus* status);

	private:
		/**
		 * inherent functions from PLT_MediaRenderer class
		 */
		virtual NPT_Result SetupServices();

		/* OH Playlist */
		virtual NPT_Result OnPlaylistInsert(PLT_ActionReference& action);
		virtual NPT_Result OnPlaylistDeleteAll(PLT_ActionReference& action);
		virtual NPT_Result OnPlaylistDeleteId(PLT_ActionReference& action);
		virtual NPT_Result OnPlaylistReadList(PLT_ActionReference& action);
		virtual NPT_Result OnPlaylistIdArray(PLT_ActionReference& action);
		virtual NPT_Result OnPlaylistSeekIndex(PLT_ActionReference& action);
		virtual NPT_Result OnPlaylistSeekId(PLT_ActionReference& action);

	    virtual NPT_Result OnPlaylistPlay(PLT_ActionReference& action);
	    virtual NPT_Result OnPlaylistPause(PLT_ActionReference& action);
	    virtual NPT_Result OnPlaylistStop(PLT_ActionReference& action);
	    virtual NPT_Result OnPlaylistNext(PLT_ActionReference& action);
	    virtual NPT_Result OnPlaylistPrevious(PLT_ActionReference& action);

	    virtual NPT_Result OnPlaylistSeekSecondAbsolute(PLT_ActionReference& action);
	    virtual NPT_Result OnPlaylistSeekSecondRelative(PLT_ActionReference& action);

	    virtual NPT_Result OnPlaylistSetRepeat(PLT_ActionReference& action);
	    virtual NPT_Result OnPlaylistRepeat(PLT_ActionReference& action);
	    virtual NPT_Result OnPlaylistSetShuffle(PLT_ActionReference& action);
	    virtual NPT_Result OnPlaylistShuffle(PLT_ActionReference& action);

	    /* OH Volume */
	    virtual NPT_Result OnPlaylistSetVolume(PLT_ActionReference& action);
	    virtual NPT_Result OnPlaylistVolume(PLT_ActionReference& action);
	    virtual NPT_Result OnPlaylistVolumeInc(PLT_ActionReference& action);
	    virtual NPT_Result OnPlaylistVolumeDec(PLT_ActionReference& action);
	    virtual NPT_Result OnPlaylistSetMute(PLT_ActionReference& action);
	    virtual NPT_Result OnPlaylistMute(PLT_ActionReference& action);

	    /* helper functions */
		void createIdArray(NPT_String& idArray);
		void UpdateState();

		void OnMsgPlayNext(MyMessage* arg);
		void OnMsgUpdatePlayTime(MyMessage* arg);

		/**
		 * Message listener, can be override by derived class.
		 */
		virtual void MessageListener(MyMessage* arg);

	private:
		void* m_context;
		std::string m_room;
		int m_id;
		int m_trackCount;
		int m_token;
		NPT_String m_idArray;
};
