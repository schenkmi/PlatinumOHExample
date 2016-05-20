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

#include <memory>
#include <mutex>
#include <MediaItem.h>
#include <MyMessages.h>

#define UPNP_MEDIARENDERER_STRING_LEN		20

/* M.Schenk 2014.01.29 updated from XBMC */
#define RESOURCE_PROTOCOL_INFO_VALUES 					\
        "http-get:*:*:*"								\
        ",xbmc-get:*:*:*"								\
        ",http-get:*:audio/mkv:*"						\
        ",http-get:*:audio/mpegurl:*"					\
        ",http-get:*:audio/mpeg:*"						\
        ",http-get:*:audio/mpeg3:*"						\
        ",http-get:*:audio/mp3:*"						\
        ",http-get:*:audio/mp4:*"						\
        ",http-get:*:audio/basic:*"						\
        ",http-get:*:audio/midi:*"						\
        ",http-get:*:audio/ulaw:*"						\
        ",http-get:*:audio/ogg:*"						\
        ",http-get:*:audio/DVI4:*"						\
        ",http-get:*:audio/G722:*"						\
        ",http-get:*:audio/G723:*"						\
        ",http-get:*:audio/G726-16:*"					\
        ",http-get:*:audio/G726-24:*"					\
        ",http-get:*:audio/G726-32:*"					\
        ",http-get:*:audio/G726-40:*"					\
        ",http-get:*:audio/G728:*"						\
        ",http-get:*:audio/G729:*"						\
        ",http-get:*:audio/G729D:*"						\
        ",http-get:*:audio/G729E:*"						\
        ",http-get:*:audio/GSM:*"						\
        ",http-get:*:audio/GSM-EFR:*"					\
        ",http-get:*:audio/L8:*"						\
        ",http-get:*:audio/L16:*"						\
        ",http-get:*:audio/LPC:*"						\
        ",http-get:*:audio/MPA:*"						\
        ",http-get:*:audio/PCMA:*"						\
        ",http-get:*:audio/PCMU:*"						\
        ",http-get:*:audio/QCELP:*"						\
        ",http-get:*:audio/RED:*"						\
        ",http-get:*:audio/VDVI:*"						\
        ",http-get:*:audio/ac3:*"						\
        ",http-get:*:audio/vorbis:*"					\
        ",http-get:*:audio/speex:*"						\
        ",http-get:*:audio/flac:*"						\
        ",http-get:*:audio/x-flac:*"					\
        ",http-get:*:audio/x-aiff:*"					\
        ",http-get:*:audio/x-pn-realaudio:*"			\
        ",http-get:*:audio/x-realaudio:*"				\
        ",http-get:*:audio/x-wav:*"						\
        ",http-get:*:audio/x-matroska:*"				\
        ",http-get:*:audio/x-ms-wma:*"					\
        ",http-get:*:audio/x-mpegurl:*"					\
        ",http-get:*:application/x-shockwave-flash:*"	\
        ",http-get:*:application/ogg:*"					\
        ",http-get:*:application/sdp:*"					\
        ",http-get:*:image/gif:*"						\
        ",http-get:*:image/jpeg:*"						\
        ",http-get:*:image/ief:*"						\
        ",http-get:*:image/png:*"						\
        ",http-get:*:image/tiff:*"						\
        ",http-get:*:video/avi:*"						\
        ",http-get:*:video/divx:*"						\
        ",http-get:*:video/mpeg:*"						\
        ",http-get:*:video/fli:*"						\
        ",http-get:*:video/flv:*"						\
        ",http-get:*:video/quicktime:*"					\
        ",http-get:*:video/vnd.vivo:*"					\
        ",http-get:*:video/vc1:*"						\
        ",http-get:*:video/ogg:*"						\
        ",http-get:*:video/mp4:*"						\
        ",http-get:*:video/mkv:*"						\
        ",http-get:*:video/BT656:*"						\
        ",http-get:*:video/CelB:*"						\
        ",http-get:*:video/JPEG:*"						\
        ",http-get:*:video/H261:*"						\
        ",http-get:*:video/H263:*"						\
        ",http-get:*:video/H263-1998:*"					\
        ",http-get:*:video/H263-2000:*"					\
        ",http-get:*:video/MPV:*"						\
        ",http-get:*:video/MP2T:*"						\
        ",http-get:*:video/MP1S:*"						\
        ",http-get:*:video/MP2P:*"						\
        ",http-get:*:video/BMPEG:*"						\
        ",http-get:*:video/xvid:*"						\
        ",http-get:*:video/x-divx:*"					\
        ",http-get:*:video/x-matroska:*"				\
        ",http-get:*:video/x-ms-wmv:*"					\
        ",http-get:*:video/x-ms-avi:*"					\
        ",http-get:*:video/x-flv:*"						\
        ",http-get:*:video/x-fli:*"						\
        ",http-get:*:video/x-ms-asf:*"					\
        ",http-get:*:video/x-ms-asx:*"					\
        ",http-get:*:video/x-ms-wmx:*"					\
        ",http-get:*:video/x-ms-wvx:*"					\
        ",http-get:*:video/x-msvideo:*"					\
        ",http-get:*:video/x-xvid:*"

class IRenderer;
class SynchronizedStatus;

class IMyPLTController : public MyMessageDispatcher
{
	public:
		IMyPLTController(std::shared_ptr<IRenderer> renderer)
			:
			m_index(-1),
			m_testTime(0),
			m_renderer(renderer)
		{

		}

		virtual ~IMyPLTController() {}

		virtual const char* getName() = 0;

		/**
		 * Called by the renderer (decoder) if synchronized
		 * stuff has changed (mute, volume, shuffle, repeat).
		 */
		virtual void RendererChanges(SynchronizedStatus* status) = 0;

		/**
		 * Message listener, can be override by derived class.
		 */
		virtual void MessageListener(MyMessage* arg)
		{
			if (arg) {
				delete arg;
			}
		}

	private:
		virtual int messageListener(MyMessage* arg)
		{
			MessageListener(arg);
			return 0;
		}

	protected:
		int m_index; /* track index in m_mediaItems */
		MediaItems m_mediaItems;
		int m_testTime;
		std::shared_ptr<IRenderer> m_renderer;
		std::mutex m_mutex;
};
