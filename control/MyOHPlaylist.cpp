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
#include <PltService.h>
#include <PltUtilities.h>
#include <PltDidl.h>
/* local includes */
#include "MyOHPlaylist.h"
#include "UPnPUtils.h"
#include "Renderer.h"
#include "MyLogger.h"
#include "MyMessages.h"

NPT_SET_LOCAL_LOGGER("platinum.oh.myplaylist")

/**
 *
 */
MyOHPlaylist::MyOHPlaylist(std::shared_ptr<IRenderer> renderer, void* ctx,const char* friendly_name,
		  	  	  	  	   bool show_ip, const char* uuid,
						   unsigned int port)
	:
	IMyPLTController(renderer),
	PLT_OHPlaylist(friendly_name, show_ip, uuid, port),
	m_context(ctx),
	m_room(friendly_name),
	m_id(0),
	m_trackCount(0),
	m_token(1),
	m_idArray("")
{
	ML_ENTRY_EXIT();

	m_renderer->registerNotifier(this);
}

/**
 *
 */
MyOHPlaylist::~MyOHPlaylist()
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
void MyOHPlaylist::UpdateState()
{
	ML_ENTRY_EXIT();

	/* UpdateState() will be called only by this class. So no lock required, because it's already held by the caller */

	PLT_Service* service = NULL;

	if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Playlist:1", service))) {
		service->PauseEventing(true);

		if (m_index != -1) {
			NPT_String tmp =  NPT_String::FromInteger(m_mediaItems[m_index]->ohPltID);
			service->SetStateVariable("Id", tmp);
		}
		else {
			service->SetStateVariable("Id", "0");
		}

		service->SetStateVariable("IdArray", m_idArray);

		service->SetStateVariable("IdArrayToken", NPT_String::FromInteger(m_token));

		switch (m_renderer->getState()) {
			case RendererState::Stopped:
				service->SetStateVariable("TransportState", "Stopped");
				break;
			case RendererState::Playing:
				service->SetStateVariable("TransportState", "Playing");
				break;
			case RendererState::Paused:
				service->SetStateVariable("TransportState", "Paused");
				break;
			case RendererState::Buffering:
				service->SetStateVariable("TransportState", "Buffering");
				break;
			default:
				break;
		}

		service->PauseEventing(false);
	}

	if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Info:1", service))) {
		service->PauseEventing(true);

		if (m_index != -1) {
			service->SetStateVariable("Uri", m_mediaItems[m_index]->ohPltURI.c_str());
			service->SetStateVariable("Metadata", m_mediaItems[m_index]->ohPltMetadata.c_str());

			/* KAZOO issue (stop icon instead pause on start), only set duration here if valid */
			if (m_mediaItems[m_index]->duration != 0) {
				service->SetStateVariable("Duration", NPT_String::FromInteger(m_mediaItems[m_index]->duration));
			}

			service->SetStateVariable("TrackCount", NPT_String::FromInteger(m_trackCount));

			std::shared_ptr<MetaData> metaData = m_mediaItems[m_index]->getMetaData();

			if (metaData && metaData->resources.size()) {
				/* taking always the first entry */
				service->SetStateVariable("BitRate", NPT_String::FromInteger(metaData->resources[0].bitrate));
				service->SetStateVariable("BitDepth", NPT_String::FromInteger(metaData->resources[0].bitsPerSample));
				service->SetStateVariable("SampleRate", NPT_String::FromInteger(metaData->resources[0].sampleFrequency));
			}
		}
		else {
	        NPT_String meta =
	                "<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
	                "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" "
	                "xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" "
	                "xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0/\">"
	        		"<item restricted=\"1\">"
	        		"<orig>snk</orig>"
	        		"</item></DIDL-Lite>";

	        service->SetStateVariable("TrackCount", "0");
	        service->SetStateVariable("DetailsCount", "0");
	        service->SetStateVariable("MetatextCount", "0");
	        service->SetStateVariable("Uri", "");
	        service->SetStateVariable("Metadata", meta);
	        service->SetStateVariable("Duration", "");
	        service->SetStateVariable("BitRate", "0");
	        service->SetStateVariable("BitDepth", "0");
	        service->SetStateVariable("SampleRate", "0");
	        service->SetStateVariable("Lossless", "0");
	        service->SetStateVariable("CodecName", "");
	        service->SetStateVariable("Metatext", "");
		}

		service->PauseEventing(false);
	}

	if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Time:1", service))) {
		service->PauseEventing(true);

		if (m_index != -1) {
			service->SetStateVariable("TrackCount", NPT_String::FromInteger(m_trackCount));

			/* KAZOO issue (stop icon instead pause on start), only set duration here if valid */
			if (m_mediaItems[m_index]->duration != 0) {
				service->SetStateVariable("Duration", NPT_String::FromInteger(m_mediaItems[m_index]->duration));
			}

			service->SetStateVariable("Seconds", NPT_String::FromInteger(m_testTime));
		}
		else {
			service->SetStateVariable("Duration", "");
			service->SetStateVariable("Seconds", "0");
		}

		service->PauseEventing(false);
	}

}

/**
 *
 */
NPT_Result MyOHPlaylist::SetupServices()
{
	ML_ENTRY_EXIT();

    PLT_Service* service = NULL;

    NPT_CHECK(PLT_OHPlaylist::SetupServices());

    if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Playlist:1", service))) {
		/* pause automatic eventing, we change multiple state vars */
    	service->PauseEventing(true);

    	service->SetStateVariable("ProtocolInfo", RESOURCE_PROTOCOL_INFO_VALUES);

    	service->SetStateVariable("Shuffle", NPT_String::FromInteger(m_renderer->getShuffle()));
    	service->SetStateVariable("Repeat", NPT_String::FromInteger(m_renderer->getRepeat()));

		/* resume automatic eventing */
    	service->PauseEventing(false);
    }

    if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Product:1", service))) {
		/* pause automatic eventing, we change multiple state vars */
    	service->PauseEventing(true);

    	service->SetStateVariable("ProductRoom", m_room.c_str() /*"MichaelTest"*/);

		/* resume automatic eventing */
    	service->PauseEventing(false);
    }

    if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Volume:1", service))) {
		/* pause automatic eventing, we change multiple state vars */
		service->PauseEventing(true);

		service->SetStateVariable("Volume", NPT_String::FromInteger(m_renderer->getVolume()));
		service->SetStateVariable("Mute", NPT_String::FromInteger(m_renderer->getMute()));

		/* resume automatic eventing */
		service->PauseEventing(false);
    }

    return NPT_SUCCESS;
}

/**
 *
 */
void MyOHPlaylist::createIdArray(NPT_String& idArray)
{
	ML_ENTRY_EXIT();

	idArray = "";

	if (m_mediaItems.size() > 0) {
		NPT_Byte* idArrayByte = new NPT_Byte[m_mediaItems.size() * 4];
		NPT_Byte* ptr = idArrayByte;

/* auto is a lot more cooler */
#if 0
		for (MediaItemsIt it = m_mediaItems.begin(); it != m_mediaItems.end(); ++it) {
			//printf("-> (*it)->ohPltID %d\n", (*it)->ohPltID);
			*ptr++ = (NPT_Byte)((*it)->ohPltID >> 24);
			*ptr++ = (NPT_Byte)((*it)->ohPltID >> 16);
			*ptr++ = (NPT_Byte)((*it)->ohPltID >> 8);
			*ptr++ = (NPT_Byte)((*it)->ohPltID);
		}
#else
		for (auto i : m_mediaItems) {
			//printf("-> (*it)->ohPltID %d\n", (*it)->ohPltID);
			*ptr++ = (NPT_Byte)(i->ohPltID >> 24);
			*ptr++ = (NPT_Byte)(i->ohPltID >> 16);
			*ptr++ = (NPT_Byte)(i->ohPltID >> 8);
			*ptr++ = (NPT_Byte)(i->ohPltID);
		}
#endif

		NPT_Base64::Encode((const NPT_Byte *)idArrayByte, m_mediaItems.size() * 4, idArray);

		delete[] idArrayByte;
	}
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistInsert(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	NPT_Int32 afterId;
	NPT_String afterIdString;

	NPT_String tmp;
	NPT_String uri;
	NPT_String meta;

	NPT_CHECK_SEVERE(action->GetArgumentValue("AfterId", afterIdString));
	ML_LOG_DEBUG("OnPlaylistInsert AfterId  %s\n", afterIdString.GetChars());
	NPT_CHECK_SEVERE(afterIdString.ToInteger32(afterId));

	NPT_CHECK_SEVERE(action->GetArgumentValue("Uri", uri));
	ML_LOG_DEBUG("OnPlaylistInsert Uri  %s\n", uri.GetChars());

	NPT_CHECK_SEVERE(action->GetArgumentValue("Metadata", meta));
	ML_LOG_DEBUG("OnPlaylistInsert Metadata  %s\n", meta.GetChars());

	if (!meta.IsEmpty()) {
	    PLT_MediaObjectListReference list;
	    PLT_MediaObject* object = NULL;

	    if (NPT_SUCCEEDED(PLT_Didl::FromDidl(meta, list))) {
	    	ML_LOG_DEBUG("object list count [%d]\n", list->GetItemCount());

			/* get the first object of the list */
			list->Get(0, object);

			if (object) {
				std::shared_ptr<MetaData> metaData = create_metadata_from_media_object(object);

				if (metaData) {
					std::shared_ptr<MediaItem> mediaItem = std::make_shared<MediaItem>();

					upnp_update_playlist_from_metadata(mediaItem, metaData);

					if ((_mylogger_log_level_ && ML_LOG_MASK) > 1) {
						MediaItem::dump(mediaItem);
					}

					MediaItemsIt it;
				    if (afterId == 0) {
				    	it = m_mediaItems.begin();
				    }
				    else {
				    	for (it = m_mediaItems.begin(); it != m_mediaItems.end(); ++it) {
				    		if ((*it)->ohPltID == afterId) {
				    			break;
				    		}
				    	}

				        if(it == m_mediaItems.end()) {
				        	ML_LOG_DEBUG("not found\n");

				        	mediaItem.reset();

				        	return NPT_ERROR_NOT_IMPLEMENTED;
				        }
				        
				        ++it;  /* we insert after the id, not before */
				    }

				    m_id++;

				    mediaItem->origin = kMediaItemOriginOpenHome;
				    mediaItem->ohPltID = m_id;
				    mediaItem->ohPltURI = uri.GetChars();
				    mediaItem->ohPltMetadata = meta.GetChars();

				    m_mediaItems.insert(it, mediaItem);

				    ML_LOG_DEBUG("-> ID %d\n", m_id);

					/* need to update ID after every insert */
					{
						PLT_Service* playlistService = NULL;

						if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Playlist:1", playlistService))) {
							playlistService->PauseEventing(true);

							/**
							 * Id: The id of the current track (the track currently playing or that would
							 * be played if the Play action was invoked). Or 0 if the playlist is empty.
							 */
							createIdArray(m_idArray);

							playlistService->SetStateVariable("IdArray", m_idArray);

							m_token++;
							playlistService->SetStateVariable("IdArrayToken", NPT_String::FromInteger(m_token));

							playlistService->PauseEventing(false);
						}
					}
				}

				action->SetArgumentValue("NewId", NPT_String::FromInteger(m_id));
			}
	    }
	}

	return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistIdArray(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);

	createIdArray(m_idArray);

	action->SetArgumentValue("Array", m_idArray);

	m_token++;

	action->SetArgumentValue("Token", NPT_String::FromInteger(m_token));

	return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistDeleteAll(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);

	m_renderer->stop(this);

/* this are equale functions */
#if 0
	for (auto i : m_mediaItems) {
		i.reset();
	}
#else
	std::for_each(m_mediaItems.begin(), m_mediaItems.end(), [](std::shared_ptr<MediaItem> n){ n.reset();});
#endif

	m_mediaItems.clear();

	/* SNK, m_id must be always increase !!!!!*/
#if 0
	m_id = 0;
#endif

	m_token++;

	createIdArray(m_idArray);

	m_index = -1;

	m_testTime = 0;

	UpdateState();

	NPT_CHECK_SEVERE(action->SetArgumentsOutFromStateVariable());

	return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistDeleteId(PLT_ActionReference& action)
{
	NPT_String id;
	int idValue = -1;
	int index = 0;

	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);

	NPT_CHECK_SEVERE(action->GetArgumentValue("Value", id));

	ML_LOG_DEBUG( "OnPlaylistDeleteId Id = %s\n", id.GetChars());

	NPT_CHECK_SEVERE(id.ToInteger32(idValue));
#if 0
	int f = 0;
	for (MediaItemsIt it = m_mediaItems.begin(); it != m_mediaItems.end(); ++it) {
		printf("item[%d] use_count() %d\n",f, (*it).use_count());
		f++;
	}
#endif

	int currentPltID = (m_index != -1) ? m_mediaItems[m_index]->ohPltID : -1;

	/* TODO: Check if it playing ... */
	for (MediaItemsIt it = m_mediaItems.begin(); it != m_mediaItems.end(); ++it) {

		if ((*it)->ohPltID == idValue) {
			(*it).reset();
			m_mediaItems.erase(it);

			if (index == m_index) {
				/* we delete the current active element */
				if (!m_mediaItems.empty()) {
					if ((m_renderer->getState() == RendererState::Playing) || (m_renderer->getState() == RendererState::Paused)) {
						std::shared_ptr<MediaItem> item = m_mediaItems[m_index];

						/* does stop/play */
						m_renderer->play(this, item);

						/* update the currentPltID with the new played !! */
						currentPltID = item->ohPltID;
					}

					m_trackCount++;
					m_testTime = 0;
				}
				else {
					/* was the last in playlist */
					m_renderer->stop(this);

					m_testTime = 0;
				}
			}

			break;
		}

		index++;
	}

	m_index = -1;

	/* need to re-adjust the index */
	if (currentPltID != -1) {
		int newIndex = 0;

		for (auto item : m_mediaItems) {
			if (item->ohPltID == currentPltID) {
				m_index = newIndex;
				break;
			}

			newIndex++;
		}
	}

	m_token++;
	
	createIdArray(m_idArray);

	UpdateState();

	NPT_CHECK_SEVERE(action->SetArgumentsOutFromStateVariable());

	return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistReadList(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	NPT_List<NPT_String> ids;
	NPT_List<NPT_String>::Iterator idsIt;
	NPT_String idList;

	NPT_CHECK_SEVERE(action->GetArgumentValue("IdList", idList));
	ML_LOG_DEBUG( "OnPlaylistReadList IdList  %s\n", idList.GetChars());

	ids = idList.Split(" ");

	NPT_String csxml = "<TrackList>";
	NPT_Int32 idInteger;

    for (idsIt = ids.GetFirstItem(); idsIt; idsIt++) {
    	ML_LOG_DEBUG( "ID %s\n", (*idsIt).GetChars());

    	(*idsIt).ToInteger32(idInteger);

    	for (MediaItemsIt it = m_mediaItems.begin(); it != m_mediaItems.end(); ++it) {
    		if ((*it)->ohPltID == idInteger) {
    			csxml+="<Entry>";

    			csxml+="<Id>";
    			csxml+= NPT_String::FromInteger((*it)->ohPltID);
    			csxml+="</Id>";

    			csxml+="<Uri>";
    			PLT_Didl::AppendXmlEscape(csxml, (*it)->ohPltURI.c_str());
    			csxml+="</Uri>";

    			csxml+="<Metadata>";
    			PLT_Didl::AppendXmlEscape(csxml, (*it)->ohPltMetadata.c_str());
    			csxml+="</Metadata>";

    			csxml+="</Entry>";

    			break;
    		}
    	}
    }

    csxml+="</TrackList>";

    ML_LOG_DEBUG( "TrackList %s\n", csxml.GetChars());

	NPT_CHECK_SEVERE(action->SetArgumentValue("TrackList", csxml));

	return NPT_SUCCESS;

}

/**
 * Index is from start of current playlist, not id dependend
 */
NPT_Result MyOHPlaylist::OnPlaylistSeekIndex(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	int index;
	NPT_String value;

	NPT_CHECK_SEVERE(action->GetArgumentValue("Value", value));
	ML_LOG_DEBUG( "OnPlaylistSeekIndex SeekIndex  value %s\n", value.GetChars());

	value.ToInteger32(index);

	if (index < (int)m_mediaItems.size()) {
		m_index = index;

		m_trackCount++;

		std::shared_ptr<MediaItem> item = m_mediaItems[m_index];
		m_renderer->play(this, item);
	}
	else {
		m_index = -1;
	}

	UpdateState();

	NPT_CHECK_SEVERE(action->SetArgumentsOutFromStateVariable());

	return NPT_SUCCESS;
}

/**
 * Index is from start of current playlist, not id dependend
 */
NPT_Result MyOHPlaylist::OnPlaylistSeekId(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	int index;
	MediaItemsIt it;
	NPT_String value;
	int id;

	NPT_CHECK_SEVERE(action->GetArgumentValue("Value", value));
	ML_LOG_DEBUG( "OnPlaylistSeekId ID = %s\n", value.GetChars());

	NPT_CHECK_SEVERE(value.ToInteger32(id));

	index = 0;

	for (it = m_mediaItems.begin(); it != m_mediaItems.end(); ++it) {
		if ((*it)->ohPltID == id) {
			break;
		}

		index++;
	}

	if (it !=  m_mediaItems.end()) {
		m_index = index;

		m_trackCount++;

		std::shared_ptr<MediaItem> item = m_mediaItems[m_index];
		m_renderer->play(this, item);
	}
	else {
		/* error ? */
	    action->SetError(401,"Id not found");
	    return NPT_FAILURE;
	}

	UpdateState();

	return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistPlay(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_index != -1) {
		if (m_renderer->getState() == RendererState::Paused) {
			m_renderer->unpause(this);
		}
		else if (m_renderer->getState() == RendererState::Stopped) {
			std::shared_ptr<MediaItem> item = m_mediaItems[m_index];
			m_renderer->play(this, item);
		}

		UpdateState();

		NPT_CHECK_SEVERE(action->SetArgumentsOutFromStateVariable());

		return NPT_SUCCESS;
	}
	else {
	    action->SetError(401,"Id not found");
	    return NPT_FAILURE;
	}
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistPause(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_renderer->getState() == RendererState::Playing) {
		m_renderer->pause(this);
	}
	else if (m_renderer->getState() == RendererState::Paused) {
		m_renderer->unpause(this);
	}

	UpdateState();

	NPT_CHECK_SEVERE(action->SetArgumentsOutFromStateVariable());

	return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistStop(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);

	/* if playint/pause -> stop */
	m_renderer->stop(this);

	m_testTime = 0;

	UpdateState();

	NPT_CHECK_SEVERE(action->SetArgumentsOutFromStateVariable());

	return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistNext(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_renderer->getShuffle() && (m_index != -1)) {
		/* create random between 0 ... (m_mediaItems.size() - 1) */
		m_index = rand() % m_mediaItems.size();
	}

	if ((m_index != -1) && (m_renderer->getRepeat() || (m_index < ((int)m_mediaItems.size() - 1)))) {
		m_index++;

		m_index = (m_index % m_mediaItems.size());

		/**
		 * m_trackCount++ is essential everytime we play a new track, otherwise
		 * Linn Kinsky will not update information on left top corner
		 */
		m_trackCount++;
		m_testTime = 0;

		std::shared_ptr<MediaItem> item = m_mediaItems[m_index];
		m_renderer->play(this, item);

		UpdateState();

		NPT_CHECK_SEVERE(action->SetArgumentsOutFromStateVariable());

		return NPT_SUCCESS;
	}
	else {
	    action->SetError(401,"Id not found");
	    return NPT_FAILURE;
	}

}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistPrevious(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);

	if ((m_index != -1) && (m_renderer->getRepeat() || (m_index > 0))) {
		m_index--;

		m_index = (m_index % m_mediaItems.size());

		/**
		 * m_trackCount++ is essential everytime we play a new track, otherwise
		 * Linn Kinsky will not update information on left top corner
		 */
		m_trackCount++;
		m_testTime = 0;

		std::shared_ptr<MediaItem> item = m_mediaItems[m_index];
		m_renderer->play(this, item);

		UpdateState();

		NPT_CHECK_SEVERE(action->SetArgumentsOutFromStateVariable());


		return NPT_SUCCESS;
	}
	else {
	    action->SetError(401,"Id not found");
	    return NPT_FAILURE;
	}
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistSeekSecondAbsolute(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	NPT_String value;
	int time;

	NPT_CHECK_SEVERE(action->GetArgumentValue("Value", value));
	ML_LOG_DEBUG( "OnPlaylistSeekSecondAbsolute Value = %s\n", value.GetChars());

	NPT_CHECK_SEVERE(value.ToInteger32(time));

	m_renderer->seek(this, 0, time);

	return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistSeekSecondRelative(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	NPT_String value;
	int time;

	NPT_CHECK_SEVERE(action->GetArgumentValue("Value", value));
	ML_LOG_DEBUG( "OnPlaylistSeekSecondRelative Value = %s\n", value.GetChars());

	NPT_CHECK_SEVERE(value.ToInteger32(time));

	m_renderer->seek(this, 1, time);

	return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistSetRepeat(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	NPT_String value;
	int repeate;

	NPT_CHECK_SEVERE(action->GetArgumentValue("Value", value));
	ML_LOG_DEBUG( "OnPlaylistSetRepeat Value = %s\n", value.GetChars());

	NPT_CHECK_SEVERE(value.ToInteger32(repeate));

	m_renderer->setRepeat(this, repeate);

/* not needed, updated via RendererChanges */
#if 0
	PLT_Service* service = NULL;
	if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Playlist:1", service))) {
		service->PauseEventing(true);

		service->SetStateVariable("Repeat", NPT_String::FromInteger(m_renderer->getRepeat()));

		service->PauseEventing(false);
	}
#endif

	return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistRepeat(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	action->SetArgumentValue("Value", NPT_String::FromInteger(m_renderer->getRepeat()));

	return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistSetShuffle(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	NPT_String value;
	int shuffle;

	NPT_CHECK_SEVERE(action->GetArgumentValue("Value", value));
	ML_LOG_DEBUG( "OnPlaylistSetShuffle Value = %s\n", value.GetChars());

	NPT_CHECK_SEVERE(value.ToInteger32(shuffle));

	m_renderer->setShuffle(this, shuffle);

/* not needed, updated via RendererChanges */
#if 0
	PLT_Service* service = NULL;
	if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Playlist:1", service))) {
		service->PauseEventing(true);

		service->SetStateVariable("Shuffle", NPT_String::FromInteger(m_renderer->getShuffle()));

		service->PauseEventing(false);
	}
#endif

	return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistShuffle(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	action->SetArgumentValue("Value", NPT_String::FromInteger(m_renderer->getShuffle()));

	return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistSetVolume(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	NPT_String value;
	int volume;

	NPT_CHECK_SEVERE(action->GetArgumentValue("Value", value));
	ML_LOG_DEBUG( "OnPlaylistSetVolume Value = %s\n", value.GetChars());

	NPT_CHECK_SEVERE(value.ToInteger32(volume));

	m_renderer->setVolume(this, volume);

/* not needed, updated via RendererChanges */
#if 0
	PLT_Service* service = NULL;
	if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Volume:1", service))) {
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
NPT_Result MyOHPlaylist::OnPlaylistVolume(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	action->SetArgumentValue("Value", NPT_String::FromInteger(m_renderer->getVolume()));

	return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistVolumeInc(PLT_ActionReference& action __attribute__((unused)))
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	int volume = m_renderer->getVolume();

	if (volume < 100) {
		volume++;
		m_renderer->setVolume(this, volume);
	}

/* not needed, updated via RendererChanges */
#if 0
	PLT_Service* service = NULL;
	if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Volume:1", service))) {
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
NPT_Result MyOHPlaylist::OnPlaylistVolumeDec(PLT_ActionReference& action __attribute__((unused)))
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	int volume = m_renderer->getVolume();

	if (volume > 0) {
		volume--;
		m_renderer->setVolume(this, volume);
	}

/* not needed, updated via RendererChanges */
#if 0
	PLT_Service* service = NULL;
	if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Volume:1", service))) {
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
NPT_Result MyOHPlaylist::OnPlaylistSetMute(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	NPT_String value;
	int mute;

	NPT_CHECK_SEVERE(action->GetArgumentValue("Value", value));
	ML_LOG_DEBUG( "OnPlaylistSetMute Value = %s\n", value.GetChars());

	NPT_CHECK_SEVERE(value.ToInteger32(mute));

	m_renderer->setMute(this, mute);

/* not needed, updated via RendererChanges */
#if 0
	PLT_Service* service = NULL;
	if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Volume:1", service))) {
		service->PauseEventing(true);

		service->SetStateVariable("Mute", NPT_String::FromInteger(mute));

		service->PauseEventing(false);
	}
#endif

	return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistMute(PLT_ActionReference& action)
{
	ML_ENTRY_EXIT();

	action->SetArgumentValue("Value", NPT_String::FromInteger(m_renderer->getMute()));

	return NPT_SUCCESS;
}

/**
 * 
 */
void MyOHPlaylist::RendererChanges(SynchronizedStatus* status)
{
	ML_ENTRY_EXIT();

	PLT_Service* service = NULL;

	if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Volume:1", service))) {
		service->PauseEventing(true);

		service->SetStateVariable("Volume", NPT_String::FromInteger(status->volume));
		service->SetStateVariable("Mute", NPT_String::FromInteger(status->mute));

		service->PauseEventing(false);
	}

	if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Playlist:1", service))) {
		service->PauseEventing(true);

		service->SetStateVariable("Repeat", NPT_String::FromInteger(status->repeat));
		service->SetStateVariable("Shuffle", NPT_String::FromInteger(status->shuffle));

		service->PauseEventing(false);
	}
}

/**
 * 
 */
void MyOHPlaylist::OnMsgPlayNext(MyMessage* arg)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	arg = arg;

	if (m_renderer->getShuffle() && (m_index != -1)) {
		/* create random between 0 ... (m_mediaItems.size() - 1) */
		m_index = rand() % m_mediaItems.size();
	}

	if ((m_index != -1) && (m_renderer->getRepeat() || (m_index < ((int)m_mediaItems.size() - 1)))) {
		m_index++;

		m_index = (m_index % m_mediaItems.size());

		ML_LOG_DEBUG("next index to play [%d]\n", m_index);

		std::shared_ptr<MediaItem> item = m_mediaItems[m_index];
		m_renderer->play(this, item);

		m_trackCount++;
		m_testTime = 0;

		UpdateState();
	}
	else {
		/* end of list */
		m_renderer->stop(this);

		m_index = -1;
		m_testTime = 0;

		UpdateState();
	}
}

/**
 * 
 */
void MyOHPlaylist::OnMsgUpdatePlayTime(MyMessage* arg)
{
	ML_ENTRY_EXIT();

	std::lock_guard<std::mutex> lock(m_mutex);
	UpdatePlayTimeMessage* msg = (UpdatePlayTimeMessage*)arg;
	PLT_Service* serviceTime = NULL;
	PLT_Service* serviceInfo = NULL;

	m_testTime = msg->getTime();

	if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Time:1", serviceTime))) {
		serviceTime->PauseEventing(true);

		if (m_index != -1) {
			serviceTime->SetStateVariable("Seconds", NPT_String::FromInteger(m_testTime));

			/* we do not get always the duration from metadata (KAZOO issue) */
			if (m_mediaItems[m_index]->duration == 0) {
				m_mediaItems[m_index]->duration = msg->getDuration();
				serviceTime->SetStateVariable("Duration", NPT_String::FromInteger(m_mediaItems[m_index]->duration));

				if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Info:1", serviceInfo))) {
					serviceInfo->SetStateVariable("Duration", NPT_String::FromInteger(m_mediaItems[m_index]->duration));
				}
			}
		}

		serviceTime->PauseEventing(false);
	}
}

/**
 * 
 */
void MyOHPlaylist::MessageListener(MyMessage * arg)
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
