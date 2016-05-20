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
#include <PltMediaItem.h>
#include <PltMediaRenderer.h>
/* local includes */
#include <MyPLTController.h>

/**
 *
 */
class MyUPnPRenderer : public IMyPLTController, public PLT_MediaRenderer
{
	public:
        MyUPnPRenderer(std::shared_ptr<IRenderer> renderer, void* ctx, const char* friendly_name,
					   bool show_ip = false, const char* uuid = NULL,
					   unsigned int port = 0);

		virtual ~MyUPnPRenderer();

		/* render interface */

		virtual const char* getName() { return "MyUPnPRenderer"; }

		/**
		 * Called by the renderer (decoder) if synchronized
		 * stuff has changed (mute, volume, shuffle, repeat).
		 */
		virtual void RendererChanges(SynchronizedStatus* status);

	private:
		/**
		 * inherent functions from PLT_MediaRenderer class
		 */	
	
		/* AVTransport methods */
		virtual NPT_Result OnNext(PLT_ActionReference& action);
		virtual NPT_Result OnPause(PLT_ActionReference& action);
		virtual NPT_Result OnPlay(PLT_ActionReference& action);
		virtual NPT_Result OnPrevious(PLT_ActionReference& action);
		virtual NPT_Result OnStop(PLT_ActionReference& action);
		virtual NPT_Result OnSeek(PLT_ActionReference& action);
		virtual NPT_Result OnSetAVTransportURI(PLT_ActionReference& action);
	
		/* RenderingControl methods */
		virtual NPT_Result OnSetVolume(PLT_ActionReference& action);
		virtual NPT_Result OnSetMute(PLT_ActionReference& action);
		virtual NPT_Result OnGetVolumeDBRange(PLT_ActionReference& action);
	
		/* helper functions */
		NPT_Result SetupServices();

		void UpdateState();

		void OnMsgPlayNext(MyMessage* arg);
		void OnMsgUpdatePlayTime(MyMessage* arg);

		/**
		 * Message listener, can be override by derived class.
		 */
		virtual void MessageListener(MyMessage* arg);

	private:
		void* m_context;
};
