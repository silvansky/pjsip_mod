// Uncomment to get minimum footprint (suitable for 1-2 concurrent calls only)
//#define PJ_CONFIG_MINIMAL_SIZE

// Uncomment to get maximum performance
//#define PJ_CONFIG_MAXIMUM_SPEED

#include <pj/config_site_sample.h> 

#define PJMEDIA_HAS_VIDEO	    1
#define PJMEDIA_HAS_FFMPEG	    1
#define PJMEDIA_HAS_X264_CODEC  1
//#define PJMEDIA_HAS_FFMPEG_CODEC 1
//PJMEDIA_HAS_FFMPEG_CODEC
#define PJMEDIA_HAS_FFMPEG_CODEC_H264 1
#define PJMEDIA_VIDEO_DEV_HAS_SDL   1
#define PJMEDIA_VIDEO_DEV_HAS_DSHOW 1
#define PJMEDIA_VIDEO_DEV_HAS_QT 0
#define PJMEDIA_AUDIO_DEV_HAS_NULL_AUDIO 0
#define PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO 1

#define PJMEDIA_HAS_FFMPEG_CODEC_H263P	0
//#   define PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMU	0
//#   define PJMEDIA_HAS_PASSTHROUGH_CODEC_ILBC	0
//#   define PJMEDIA_HAS_PASSTHROUGH_CODEC_G729	0
//#   define PJMEDIA_HAS_PASSTHROUGH_CODEC_AMR	0
//#   define PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMU	0
//#   define PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMA	0

//#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G722_1	0
//#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G728	0
//#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G726	0
//#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G723_1	0
//#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G729	0
//#   define PJMEDIA_HAS_INTEL_IPP_CODEC_AMRWB	0
//#   define PJMEDIA_HAS_INTEL_IPP_CODEC_AMR	0

#   define PJMEDIA_HAS_ILBC_CODEC    0

#   define PJMEDIA_HAS_G722_CODEC    0
#   define PJMEDIA_HAS_L16_CODEC    0

#   define PJMEDIA_HAS_SPEEX_CODEC    0
//#   define PJMEDIA_HAS_GSM_CODEC    1

#   define PJSIP_ENCODE_SHORT_HNAME 1

#define PJMEDIA_VID_STREAM_SKIP_PACKETS_TO_REDUCE_LATENCY 1

#define PJMEDIA_MAX_MTU 1400