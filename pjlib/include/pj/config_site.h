// Uncomment to get minimum footprint (suitable for 1-2 concurrent calls only)
//#define PJ_CONFIG_MINIMAL_SIZE

// Uncomment to get maximum performance
//#define PJ_CONFIG_MAXIMUM_SPEED

#include <pj/config_site_sample.h> 

#define PJMEDIA_HAS_VIDEO					1
#define PJMEDIA_HAS_FFMPEG					1
#define PJMEDIA_HAS_X264_CODEC				1
#define PJMEDIA_HAS_SRTP					0
//#define PJMEDIA_HAS_FFMPEG_CODEC			1
#define PJMEDIA_HAS_FFMPEG_CODEC_H264		1
#define PJMEDIA_VIDEO_DEV_HAS_SDL			0
#define PJMEDIA_VIDEO_DEV_HAS_CALLBACK		1
#define PJMEDIA_AUDIO_DEV_HAS_NULL_AUDIO	0
#define PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO		1

#define PJMEDIA_HAS_FFMPEG_CODEC_H263P		0

#define PJMEDIA_HAS_ILBC_CODEC				0

#define PJMEDIA_HAS_G722_CODEC				0
#define PJMEDIA_HAS_L16_CODEC				0

#define PJMEDIA_HAS_SPEEX_CODEC				1
#define PJMEDIA_HAS_GSM_CODEC				0

#define PJSIP_ENCODE_SHORT_HNAME			1

//#define PJMEDIA_VID_STREAM_SKIP_PACKETS_TO_REDUCE_LATENCY 1

#define PJMEDIA_MAX_MTU 1500

#if defined(PJ_WIN32) && PJ_WIN32!=0
# define PJMEDIA_VIDEO_DEV_HAS_DSHOW		1
# define PJMEDIA_VIDEO_DEV_HAS_QT			0
#endif // PJ_WIN32_WINCE


#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
# ifdef PJMEDIA_VIDEO_DEV_HAS_DSHOW
#  undef PJMEDIA_VIDEO_DEV_HAS_DSHOW
# endif
# define PJMEDIA_VIDEO_DEV_HAS_DSHOW		0

# ifdef PJMEDIA_VIDEO_DEV_HAS_QT
#  undef PJMEDIA_VIDEO_DEV_HAS_QT
# endif
# define PJMEDIA_VIDEO_DEV_HAS_QT			1

# define PJMEDIA_HAS_SRTP					0

# define PJ_HAS_THREADS						1
#endif // PJ_DARWINOS

#define PJ_DEBUG							1
#define PJ_LOG_MAX_LEVEL					6

