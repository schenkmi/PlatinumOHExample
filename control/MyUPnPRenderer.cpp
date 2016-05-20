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
#include <assert.h>
#include <algorithm>

/* Platinum/Neptune UPnP SDK includes */
#include "PltService.h"
#include "PltUtilities.h"
#include "PltDidl.h"
/* local includes */
#include "MyUPnPRenderer.h"
#include "UPnPUtils.h"
#include "Renderer.h"
#include "MyLogger.h"
#include "MyMessages.h"

NPT_SET_LOCAL_LOGGER("platinum.upnp.myplaylist")

/**
 * Class constructor.
 */
MyUPnPRenderer::MyUPnPRenderer(std::shared_ptr<IRenderer> renderer, void* ctx, const char*  friendly_name,
							   bool show_ip,const char* uuid, unsigned int port)
	:
	IMyPLTController(renderer),
    PLT_MediaRenderer(friendly_name, show_ip, uuid, port),
	m_context(ctx)
{
	ML_ENTRY_EXIT();

	m_renderer->registerNotifier(this);
}

/**
 * 
 */
MyUPnPRenderer::~MyUPnPRenderer()
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);

/* this are equale functions */
#if 0
	for (auto i : m_mediaItems) {
		i.reset();
	}
#else
	std::for_each(m_mediaItems.begin(), m_mediaItems.end(), [](std::shared_ptr<MediaItem> n){ n.reset();});
#endif

	m_mediaItems.clear();

	m_index = -1;
	m_testTime = 0;

	m_renderer.reset();
}

/**
 *
 */
NPT_Result MyUPnPRenderer::SetupServices()
{
	ML_ENTRY_EXIT();

    PLT_Service* rct = NULL;
    PLT_Service* service = NULL;

    NPT_CHECK(PLT_MediaRenderer::SetupServices());

    /* update what we can play */
    NPT_CHECK_FATAL(FindServiceByType("urn:schemas-upnp-org:service:ConnectionManager:1", service));
    service->SetStateVariable("SinkProtocolInfo" , RESOURCE_PROTOCOL_INFO_VALUES);

    /* setup mute and value with current values so that any CP sees the current values */
    if (NPT_SUCCEEDED(FindServiceByType("urn:schemas-upnp-org:service:RenderingControl:1", rct))) {
		/* pause automatic eventing, we change multiple state vars */
		rct->PauseEventing(true);

/**
 * WA for Kinsky Volume
 * Kinsky seams to want "channel" and not "Channel", but according
 * UPnP A/V "Channel" would be correct.
 * Tested with other CPs too.
 */
#if 1
		rct->SetStateVariable("Mute", "2");
		rct->SetStateVariableExtraAttribute("Mute", "channel", "Master");
		rct->SetStateVariable("Volume", "999");
		rct->SetStateVariableExtraAttribute("Volume", "channel", "Master");
#endif

		rct->SetStateVariable("Volume", NPT_String::FromInteger(m_renderer->getVolume()));
		rct->SetStateVariable("Mute", NPT_String::FromInteger(m_renderer->getMute()));

		/* resume automatic eventing */
		rct->PauseEventing(false);
    }

    return NPT_SUCCESS;
}

/**
 * 
 */
void MyUPnPRenderer::UpdateState()
{
	ML_ENTRY_EXIT();

	/* UpdateState() will be called only by this class. So no lock required, because it's already held by the caller */

	PLT_Service* service = NULL;
    NPT_String timeString;
    NPT_String currentTrackURI;
    NPT_String transportState;

    /* update A/V transport stuff */
    if (NPT_SUCCEEDED(FindServiceByType("urn:schemas-upnp-org:service:AVTransport:1", service))) {
    	/* pause automatic eventing, we change multiple state vars */
    	service->PauseEventing(true);

/* WA for kinsky tracks not advanced */
#if 1
		/**
		 * this will force to re-send the CurrentTrackURI together
		 * with the changed TransportState. Kinsky rely on this :-(.
		 */
    	service->GetStateVariableValue("CurrentTrackURI", currentTrackURI);

		/* clear and set to activate changed flag */
    	service->SetStateVariable("CurrentTrackURI", !currentTrackURI.IsEmpty() ? "" : "Gugus");
    	service->SetStateVariable("CurrentTrackURI", currentTrackURI);
    	service->SetStateVariable("TransportState", "");
#endif

		switch (m_renderer->getState()) {
			case RendererState::Stopped:
				service->SetStateVariable("TransportState", (m_index != -1) ? "STOPPED" : "NO_MEDIA_PRESENT");

				service->SetStateVariable("TransportStatus", "OK");

				timeString = PLT_Didl::FormatTimeStamp(0);

				/* clear time values */
				service->SetStateVariable("RelativeTimePosition", timeString);
				service->SetStateVariable("AbsoluteTimePosition", timeString);
				service->SetStateVariable("CurrentTrackDuration", timeString);
				service->SetStateVariable("CurrentMediaDuration", timeString);

				break;
			case RendererState::Playing:
				service->SetStateVariable("TransportState", "PLAYING");
				service->SetStateVariable("TransportStatus", "OK");
				service->SetStateVariable("TransportPlaySpeed", "1");

				break;
			case RendererState::Paused:
				service->SetStateVariable("TransportState", "PAUSED_PLAYBACK");
				service->SetStateVariable("TransportStatus", "OK");

				break;
			case RendererState::Buffering:
				break;
			default:
				break;
		}

    	/* resume automatic eventing */
		service->PauseEventing(false);
    }

	return;
}

/**
 * 
 */
NPT_Result MyUPnPRenderer::OnNext(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	/* TODO */
	action = action;

    return NPT_SUCCESS;
}

/**
 * 
 */
NPT_Result MyUPnPRenderer::OnPause(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	NPT_String instanceID;

	NPT_CHECK_SEVERE(action->GetArgumentValue("InstanceID", instanceID));
	ML_LOG_DEBUG("OnPause InstanceID  %s\n", instanceID.GetChars());

	if (m_renderer->getState() == RendererState::Playing) {
		m_renderer->pause(this);
	}
	else if (m_renderer->getState() == RendererState::Paused) {
		m_renderer->unpause(this);
	}

	UpdateState();

    return NPT_SUCCESS;
}

/**
 * 
 */
NPT_Result MyUPnPRenderer::OnPlay(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();
	
	std::lock_guard<std::mutex> lock(m_mutex);
	NPT_Result result = NPT_SUCCESS;
	NPT_String instanceID;
	NPT_String speed;

	NPT_CHECK_SEVERE(action->GetArgumentValue("InstanceID", instanceID));
	ML_LOG_DEBUG("OnPlay InstanceID  %s\n", instanceID.GetChars());

	NPT_CHECK_SEVERE(action->GetArgumentValue("Speed", speed));
	ML_LOG_DEBUG("OnPlay Speed  %s\n", speed.GetChars());

	ML_LOG_DEBUG("OnPlay m_index %d\n", m_index);

	if (m_index != -1) {
		if (m_renderer->getState() == RendererState::Paused) {
			m_renderer->unpause(this);
		}
		else {
			std::shared_ptr<MediaItem> item = m_mediaItems[m_index];
			m_renderer->play(this, item);
		}
	}
	else {
		/* NOP */
	}

	UpdateState();

	return result;
}

/**
 * 
 */
NPT_Result MyUPnPRenderer::OnPrevious(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

    /* TODO */
	action = action;

    return NPT_SUCCESS;
}

/**
 * 
 */
NPT_Result MyUPnPRenderer::OnStop(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	NPT_String instanceID;

	NPT_CHECK_SEVERE(action->GetArgumentValue("InstanceID", instanceID));
	ML_LOG_DEBUG("OnStop InstanceID  %s\n", instanceID.GetChars());

	/* if playint/pause -> stop */
	m_renderer->stop(this);

	m_testTime = 0;

	UpdateState();

	return NPT_SUCCESS;
}

/**
 * 
 */
NPT_Result MyUPnPRenderer::OnSetAVTransportURI(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();
	
	std::lock_guard<std::mutex> lock(m_mutex);
    NPT_Result result = NPT_SUCCESS;    
    PLT_Service* avt = NULL;
    NPT_String timeString;
    NPT_String currentURI; 				/* DLNA URI (from CurrentURI)					*/
	NPT_String currentURIMetaData;		/* DLNA meta data (from CurrentURIMetaData)		*/
	NPT_String instanceID;

	NPT_CHECK_SEVERE(action->GetArgumentValue("InstanceID", instanceID));
	ML_LOG_DEBUG("OnSetAVTransportURI InstanceID  %s\n", instanceID.GetChars());

	NPT_CHECK_SEVERE(action->GetArgumentValue("CurrentURI", currentURI));
	ML_LOG_DEBUG("OnSetAVTransportURI CurrentURI  %s\n", currentURI.GetChars());

	NPT_CHECK_SEVERE(action->GetArgumentValue("CurrentURIMetaData", currentURIMetaData));
	ML_LOG_DEBUG("OnSetAVTransportURI CurrentURIMetaData  %s\n", currentURIMetaData.GetChars());

	/**
	 * if URI is empty, we have clear the playlist on CP
	 * so stop the renderer
	 */
	if (currentURI.IsEmpty()) {
		m_renderer->stop(this);
	}

	m_testTime = 0;

	/* clear all media items */
/* this are equale functions */
#if 0
	for (auto i : m_mediaItems) {
		i.reset();
	}
#else
	std::for_each(m_mediaItems.begin(), m_mediaItems.end(), [](std::shared_ptr<MediaItem> n){ n.reset();});
#endif

	m_mediaItems.clear();
	m_index = -1;
	m_testTime = 0;

	std::shared_ptr<MediaItem> mediaItem = nullptr;

	if (!currentURI.IsEmpty()) {
		mediaItem = std::make_shared<MediaItem>();

		mediaItem->origin = kMediaItemOriginUPnP;
		mediaItem->uri = currentURI.GetChars();
	}

	if (!currentURIMetaData.IsEmpty()) {
	    PLT_MediaObjectListReference list;
	    PLT_MediaObject* object = NULL;

	    if (NPT_SUCCEEDED(PLT_Didl::FromDidl(currentURIMetaData, list))) {
	    	ML_LOG_DEBUG("object list count [%d]\n", list->GetItemCount());

			/* get the first object of the list */
			list->Get(0, object);

			if (object) {
				std::shared_ptr<MetaData> metaData = create_metadata_from_media_object(object);

				if (metaData) {
					if (!mediaItem) {
						/* no URI ??? */
						mediaItem = std::make_shared<MediaItem>();
					}

					upnp_update_playlist_from_metadata(mediaItem, metaData);
				}
			}
	    }
	}


	if (NPT_SUCCEEDED(FindServiceByType("urn:schemas-upnp-org:service:AVTransport:1", avt))) {
		/* pause automatic eventing, we change multiple state vars */
		avt->PauseEventing(true);

		timeString = PLT_Didl::FormatTimeStamp(0);

		/* clear time values */
        avt->SetStateVariable("RelativeTimePosition", timeString);
		avt->SetStateVariable("AbsoluteTimePosition", timeString);
 		avt->SetStateVariable("CurrentTrackDuration", timeString);
 		avt->SetStateVariable("CurrentMediaDuration", timeString);

		if (mediaItem) {
			m_mediaItems.push_back(mediaItem);

			m_index = 0;

			avt->SetStateVariable("NumberOfTracks", "1");
			avt->SetStateVariable("CurrentTrack", "1");
			avt->SetStateVariable("AVTransportURI", currentURI);
			avt->SetStateVariable("CurrentTrackURI",currentURI);
			avt->SetStateVariable("AVTransportURIMetaData", currentURIMetaData);
			avt->SetStateVariable("CurrentTrackMetadata", currentURIMetaData);

			if (mediaItem->duration != 0) {
				timeString = PLT_Didl::FormatTimeStamp(mediaItem->duration);

				ML_LOG_DEBUG("DMR set CurrentTrackDuration/CurrentMediaDuration to [%s]\n", timeString.GetChars());

				avt->SetStateVariable("CurrentTrackDuration", timeString);
				avt->SetStateVariable("CurrentMediaDuration", timeString);
			}
		}
		else {
			avt->SetStateVariable("TransportState", "NO_MEDIA_PRESENT");
			avt->SetStateVariable("TransportStatus", "OK");

			avt->SetStateVariable("NumberOfTracks", "0");
			avt->SetStateVariable("CurrentTrack", "0");
			avt->SetStateVariable("AVTransportURI", "");
			avt->SetStateVariable("CurrentTrackURI","");
			avt->SetStateVariable("AVTransportURIMetaData", "");
			avt->SetStateVariable("CurrentTrackMetadata", "");
		}

		/* resume automatic eventing */
		avt->PauseEventing(false);
	}

	return result;
}

/**
 * 
 */
NPT_Result MyUPnPRenderer::OnSetVolume(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	NPT_String instanceID;
	NPT_String channel;
	NPT_String desiredVolume;
	int volume;

	NPT_CHECK_SEVERE(action->GetArgumentValue("InstanceID", instanceID));
	ML_LOG_DEBUG("OnSetVolume InstanceID  %s\n", instanceID.GetChars());

	NPT_CHECK_SEVERE(action->GetArgumentValue("Channel", channel));
	ML_LOG_DEBUG("OnSetVolume Channel  %s\n", channel.GetChars());

	NPT_CHECK_SEVERE(action->GetArgumentValue("DesiredVolume", desiredVolume));
	ML_LOG_DEBUG("OnSetVolume DesiredVolume  %s\n", desiredVolume.GetChars());

	if (channel.Compare("Master") != 0) {
		action->SetError(800, "Internal error");
		return NPT_FAILURE;
	}

	if (desiredVolume.ToInteger32(volume) != NPT_SUCCESS) {
		action->SetError(800, "Internal error");
		return NPT_FAILURE;
	}

	ML_LOG_DEBUG("OnSetVolume volume [%d]\n", volume);

    if (volume < 0 || volume > 100) {
    	action->SetError(800, "Internal error");
    	return NPT_FAILURE;
    }

	m_renderer->setVolume(this, volume);

/* not needed, updated via RendererChanges */
#if 0
	PLT_Service* service = NULL;
	if (NPT_SUCCEEDED(FindServiceByType("urn:schemas-upnp-org:service:RenderingControl:1", service))) {
		service->PauseEventing(true);

		service->SetStateVariable("Volume", NPT_String::FromInteger(volume));

		service->PauseEventing(false);
	}
#endif

	return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyUPnPRenderer::OnGetVolumeDBRange(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

    /* TODO */
	action = action;

    return NPT_SUCCESS;
}

/**
 * 
 */
NPT_Result MyUPnPRenderer::OnSetMute(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	NPT_String instanceID;
	NPT_String channel;
	NPT_String desiredMute;
	int mute;

	NPT_CHECK_SEVERE(action->GetArgumentValue("InstanceID", instanceID));
	ML_LOG_DEBUG("OnSetMute InstanceID  %s\n", instanceID.GetChars());

	NPT_CHECK_SEVERE(action->GetArgumentValue("Channel", channel));
	ML_LOG_DEBUG("OnSetMute Channel  %s\n", channel.GetChars());

	NPT_CHECK_SEVERE(action->GetArgumentValue("DesiredMute", desiredMute));
	ML_LOG_DEBUG("OnSetMute DesiredMute  %s\n", desiredMute.GetChars());


	if (channel.Compare("Master") != 0) {
		action->SetError(800, "Internal error");
		return NPT_FAILURE;
	}

	if (!desiredMute.IsEmpty()) {
		if (desiredMute[0] == 'F' || desiredMute[0] == '0') {
			mute = 0;
		}
		else if (desiredMute[0] == 'T' || desiredMute[0] == '1') {
			mute = 1;
		}
		else {
			action->SetError(800, "Internal error");
			return NPT_FAILURE;
		}

		m_renderer->setMute(this, mute);

/* not needed, updated via RendererChanges */
#if 0
		PLT_Service* service = NULL;
		if (NPT_SUCCEEDED(FindServiceByType("urn:schemas-upnp-org:service:RenderingControl:1", service))) {
			service->PauseEventing(true);

			service->SetStateVariable("Mute", NPT_String::FromInteger(mute));

			service->PauseEventing(false);
		}
#endif
	}

    return NPT_SUCCESS;
}

/**
 * 
 */
NPT_Result MyUPnPRenderer::OnSeek(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	NPT_String instanceID;
	NPT_String unit;
	NPT_String target;
	NPT_UInt32 time;

	NPT_CHECK_SEVERE(action->GetArgumentValue("InstanceID", instanceID));
	ML_LOG_DEBUG("OnSeek InstanceID  %s\n", instanceID.GetChars());

	NPT_CHECK_SEVERE(action->GetArgumentValue("Unit", unit));
	ML_LOG_DEBUG("OnSeek Unit  %s\n", unit.GetChars());

	NPT_CHECK_SEVERE(action->GetArgumentValue("Target", target));
	ML_LOG_DEBUG("OnSeek Target  %s\n", target.GetChars());

    /**
     * Note that ABS_TIME and REL_TIME don't mean what you'd think
     * they mean.  REL_TIME means relative to the current track,
     * ABS_TIME to the whole media (ie for a multitrack tape). So
     * take both ABS and REL as absolute position in the current song
     */

	if ((unit.Compare("REL_TIME") == 0) || (unit.Compare("ABS_TIME") == 0)) {
        /* converts target to seconds */
        NPT_CHECK_SEVERE(PLT_Didl::ParseTimeStamp(target, time));
    }
	else {
		action->SetError(800, "Internal error");
		return NPT_FAILURE;
    }

	ML_LOG_DEBUG("seek to [%u] seconds\n", time);

	m_renderer->seek(this, 0, time);

	return NPT_SUCCESS;
}

/**
 * 
 */
void MyUPnPRenderer::RendererChanges(SynchronizedStatus* status)
{
	ML_ENTRY_EXIT();

	PLT_Service* service = NULL;

    /* setup mute and value with current values so that any CP sees the current values */
    if (NPT_SUCCEEDED(FindServiceByType("urn:schemas-upnp-org:service:RenderingControl:1", service))) {
		/* pause automatic eventing, we change multiple state vars */
    	service->PauseEventing(true);

		service->SetStateVariable("Volume", NPT_String::FromInteger(status->volume));
		service->SetStateVariable("Mute", NPT_String::FromInteger(status->mute));

		/* resume automatic eventing */
		service->PauseEventing(false);
    }
}

/**
 * 
 */
void MyUPnPRenderer::OnMsgPlayNext(MyMessage* arg)
{
	ML_ENTRY_EXIT();

	arg = arg;

#if 0
	UpdateState();
#else
	/**
	 * for iterate to the next track we need to fake the TransportState to STOPPED
	 * but we do not like to call real playStop in that case.
	 */
	PLT_Service* service = NULL;
    NPT_String timeString;
    NPT_String currentTrackURI;
    NPT_String transportState;

    /* update A/V transport stuff */
    if (NPT_SUCCEEDED(FindServiceByType("urn:schemas-upnp-org:service:AVTransport:1", service))) {
    	/* pause automatic eventing, we change multiple state vars */
    	service->PauseEventing(true);

/* WA for kinsky tracks not advanced */
#if 1
		/**
		 * this will force to re-send the CurrentTrackURI together
		 * with the changed TransportState. Kinsky rely on this :-(.
		 */
    	service->GetStateVariableValue("CurrentTrackURI", currentTrackURI);

		/* clear and set to activate changed flag */
    	service->SetStateVariable("CurrentTrackURI", !currentTrackURI.IsEmpty() ? "" : "Gugus");
    	service->SetStateVariable("CurrentTrackURI", currentTrackURI);
    	service->SetStateVariable("TransportState", "");
#endif

    	service->SetStateVariable("TransportState", (m_index != -1) ? "STOPPED" : "NO_MEDIA_PRESENT");

		service->SetStateVariable("TransportStatus", "OK");

		timeString = PLT_Didl::FormatTimeStamp(0);

		/* clear time values */
		service->SetStateVariable("RelativeTimePosition", timeString);
		service->SetStateVariable("AbsoluteTimePosition", timeString);
		service->SetStateVariable("CurrentTrackDuration", timeString);
		service->SetStateVariable("CurrentMediaDuration", timeString);


    	/* resume automatic eventing */
		service->PauseEventing(false);
    }
#endif
}

/**
 * 
 */
void MyUPnPRenderer::OnMsgUpdatePlayTime(MyMessage* arg)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	NPT_String timeString;
	PLT_Service* avt = NULL;
	UpdatePlayTimeMessage* msg = (UpdatePlayTimeMessage*)arg;

	m_testTime = msg->getTime();

	if (NPT_SUCCEEDED(FindServiceByType("urn:schemas-upnp-org:service:AVTransport:1", avt))) {
		avt->PauseEventing(true);

		if (m_index != -1) {
			timeString = PLT_Didl::FormatTimeStamp(m_testTime);

			ML_LOG_DEBUG("RelativeTimePosition/AbsoluteTimePosition [%s]\n", timeString.GetChars());

	 		/* time since start of the current track */
			avt->SetStateVariable("RelativeTimePosition", timeString);

			/* time since start of the media */
			avt->SetStateVariable("AbsoluteTimePosition", timeString);

			/* we do not get always the duration from metadata (KAZOO issue) */
			if (m_mediaItems[m_index]->duration == 0) {
				m_mediaItems[m_index]->duration = msg->getDuration();

	 			timeString = PLT_Didl::FormatTimeStamp(m_mediaItems[m_index]->duration);

	 			ML_LOG_DEBUG("DMR set CurrentTrackDuration/CurrentMediaDuration to [%s]\n", timeString.GetChars());

		 		avt->SetStateVariable("CurrentTrackDuration", timeString);
		 		avt->SetStateVariable("CurrentMediaDuration", timeString);
			}
		}

		avt->PauseEventing(false);
	}
}

/**
 * 
 */
void MyUPnPRenderer::MessageListener(MyMessage * arg)
{
	ML_ENTRY_EXIT();

	if (arg) {
		ML_LOG_DEBUG(" got message %s\n", MessageIDs::toString(arg->getMessageID()));

		switch(arg->getMessageID()) {
			case MessageIDs::UpdateState:
				break;
			case MessageIDs::PlayNext:
				OnMsgPlayNext(arg);
				break;
			case MessageIDs::UpdatePlayTime:
				OnMsgUpdatePlayTime(arg);
				break;
			default:
				break;
		}

		delete arg;
	}
}
