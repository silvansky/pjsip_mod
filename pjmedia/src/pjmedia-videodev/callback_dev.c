/* $Id: callback_dev.c 3950 2012-02-06 08:27:28Z ming $ */
/*
* Based on callback_dev.c
* Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
* Modifyed by V.Gorshkov @ Rambler (http://rambler.ru)
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <pjmedia-videodev/videodev_imp.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>

#define PJMEDIA_VIDEO_DEV_HAS_CALLBACK 1

#if defined(PJMEDIA_VIDEO_DEV_HAS_CALLBACK) && PJMEDIA_VIDEO_DEV_HAS_CALLBACK != 0

#include <stdint.h>

// SDL-like typedefs
typedef uint32_t Uint32;

typedef uint16_t Uint16;

typedef uint8_t Uint8;

typedef int16_t Sint16;

typedef struct {
	Sint16 x, y;
	Uint16 w, h;
} CB_Rect;

// SDL-like defines

#define CALLBACK_YV12_OVERLAY  0x32315659  /* Planar mode: Y + V + U */
#define CALLBACK_IYUV_OVERLAY  0x56555949  /* Planar mode: Y + U + V */
#define CALLBACK_YUY2_OVERLAY  0x32595559  /* Packed mode: Y0+U0+Y1+V0 */
#define CALLBACK_UYVY_OVERLAY  0x59565955  /* Packed mode: U0+Y0+V0+Y1 */
#define CALLBACK_YVYU_OVERLAY  0x55595659  /* Packed mode: Y0+V0+Y1+U0 */

enum
{
    CALLBACK_PIXELTYPE_UNKNOWN,
    CALLBACK_PIXELTYPE_INDEX1,
    CALLBACK_PIXELTYPE_INDEX4,
    CALLBACK_PIXELTYPE_INDEX8,
    CALLBACK_PIXELTYPE_PACKED8,
    CALLBACK_PIXELTYPE_PACKED16,
    CALLBACK_PIXELTYPE_PACKED32,
    CALLBACK_PIXELTYPE_ARRAYU8,
    CALLBACK_PIXELTYPE_ARRAYU16,
    CALLBACK_PIXELTYPE_ARRAYU32,
    CALLBACK_PIXELTYPE_ARRAYF16,
    CALLBACK_PIXELTYPE_ARRAYF32
};

enum
{
    CALLBACK_BITMAPORDER_NONE,
    CALLBACK_BITMAPORDER_4321,
    CALLBACK_BITMAPORDER_1234
};

/** Packed component order, high bit -> low bit. */
enum
{
    CALLBACK_PACKEDORDER_NONE,
    CALLBACK_PACKEDORDER_XRGB,
    CALLBACK_PACKEDORDER_RGBX,
    CALLBACK_PACKEDORDER_ARGB,
    CALLBACK_PACKEDORDER_RGBA,
    CALLBACK_PACKEDORDER_XBGR,
    CALLBACK_PACKEDORDER_BGRX,
    CALLBACK_PACKEDORDER_ABGR,
    CALLBACK_PACKEDORDER_BGRA
};

/** Array component order, low byte -> high byte. */
enum
{
    CALLBACK_ARRAYORDER_NONE,
    CALLBACK_ARRAYORDER_RGB,
    CALLBACK_ARRAYORDER_RGBA,
    CALLBACK_ARRAYORDER_ARGB,
    CALLBACK_ARRAYORDER_BGR,
    CALLBACK_ARRAYORDER_BGRA,
    CALLBACK_ARRAYORDER_ABGR
};

/** Packed component layout. */
enum
{
    CALLBACK_PACKEDLAYOUT_NONE,
    CALLBACK_PACKEDLAYOUT_332,
    CALLBACK_PACKEDLAYOUT_4444,
    CALLBACK_PACKEDLAYOUT_1555,
    CALLBACK_PACKEDLAYOUT_5551,
    CALLBACK_PACKEDLAYOUT_565,
    CALLBACK_PACKEDLAYOUT_8888,
    CALLBACK_PACKEDLAYOUT_2101010,
    CALLBACK_PACKEDLAYOUT_1010102
};

#define CALLBACK_FOURCC(A, B, C, D) \
	(((Uint32)((Uint8)(A)) << 0 ) | \
	 ((Uint32)((Uint8)(B)) << 8 ) | \
	 ((Uint32)((Uint8)(C)) << 16) | \
	 ((Uint32)((Uint8)(D)) << 24))

#define CALLBACK_DEFINE_PIXELFOURCC(A, B, C, D) CALLBACK_FOURCC(A, B, C, D)

#define CALLBACK_DEFINE_PIXELFORMAT(type, order, layout, bits, bytes) \
    ((1 << 31) | ((type) << 24) | ((order) << 20) | ((layout) << 16) | \
     ((bits) << 8) | ((bytes) << 0))

#define CALLBACK_PIXELTYPE(X)	(((X) >> 24) & 0x0F)
#define CALLBACK_PIXELORDER(X)	(((X) >> 20) & 0x0F)
#define CALLBACK_PIXELLAYOUT(X)	(((X) >> 16) & 0x0F)
#define CALLBACK_BITSPERPIXEL(X)	(((X) >> 8) & 0xFF)
#define CALLBACK_BYTESPERPIXEL(X) \
    (CALLBACK_ISPIXELFORMAT_FOURCC(X) ? \
	((((X) == CALLBACK_PIXELFORMAT_YUY2) || \
	  ((X) == CALLBACK_PIXELFORMAT_UYVY) || \
	  ((X) == CALLBACK_PIXELFORMAT_YVYU)) ? 2 : 1) : (((X) >> 0) & 0xFF))

#define CALLBACK_ISPIXELFORMAT_INDEXED(format)   \
    (!CALLBACK_ISPIXELFORMAT_FOURCC(format) && \
     ((CALLBACK_PIXELTYPE(format) == CALLBACK_PIXELTYPE_INDEX1) || \
      (CALLBACK_PIXELTYPE(format) == CALLBACK_PIXELTYPE_INDEX4) || \
      (CALLBACK_PIXELTYPE(format) == CALLBACK_PIXELTYPE_INDEX8)))

#define CALLBACK_ISPIXELFORMAT_ALPHA(format)   \
    (!CALLBACK_ISPIXELFORMAT_FOURCC(format) && \
     ((CALLBACK_PIXELORDER(format) == CALLBACK_PACKEDORDER_ARGB) || \
      (CALLBACK_PIXELORDER(format) == CALLBACK_PACKEDORDER_RGBA) || \
      (CALLBACK_PIXELORDER(format) == CALLBACK_PACKEDORDER_ABGR) || \
      (CALLBACK_PIXELORDER(format) == CALLBACK_PACKEDORDER_BGRA)))

#define CALLBACK_ISPIXELFORMAT_FOURCC(format)    \
    ((format) && !((format) & 0x80000000))

enum
{
    CALLBACK_PIXELFORMAT_UNKNOWN,
    CALLBACK_PIXELFORMAT_INDEX1LSB =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_INDEX1, CALLBACK_BITMAPORDER_4321, 0,
			       1, 0),
    CALLBACK_PIXELFORMAT_INDEX1MSB =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_INDEX1, CALLBACK_BITMAPORDER_1234, 0,
			       1, 0),
    CALLBACK_PIXELFORMAT_INDEX4LSB =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_INDEX4, CALLBACK_BITMAPORDER_4321, 0,
			       4, 0),
    CALLBACK_PIXELFORMAT_INDEX4MSB =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_INDEX4, CALLBACK_BITMAPORDER_1234, 0,
			       4, 0),
    CALLBACK_PIXELFORMAT_INDEX8 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_INDEX8, 0, 0, 8, 1),
    CALLBACK_PIXELFORMAT_RGB332 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED8, CALLBACK_PACKEDORDER_XRGB,
			       CALLBACK_PACKEDLAYOUT_332, 8, 1),
    CALLBACK_PIXELFORMAT_RGB444 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED16, CALLBACK_PACKEDORDER_XRGB,
			       CALLBACK_PACKEDLAYOUT_4444, 12, 2),
    CALLBACK_PIXELFORMAT_RGB555 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED16, CALLBACK_PACKEDORDER_XRGB,
			       CALLBACK_PACKEDLAYOUT_1555, 15, 2),
    CALLBACK_PIXELFORMAT_BGR555 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED16, CALLBACK_PACKEDORDER_XBGR,
			       CALLBACK_PACKEDLAYOUT_1555, 15, 2),
    CALLBACK_PIXELFORMAT_ARGB4444 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED16, CALLBACK_PACKEDORDER_ARGB,
			       CALLBACK_PACKEDLAYOUT_4444, 16, 2),
    CALLBACK_PIXELFORMAT_RGBA4444 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED16, CALLBACK_PACKEDORDER_RGBA,
			       CALLBACK_PACKEDLAYOUT_4444, 16, 2),
    CALLBACK_PIXELFORMAT_ABGR4444 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED16, CALLBACK_PACKEDORDER_ABGR,
			       CALLBACK_PACKEDLAYOUT_4444, 16, 2),
    CALLBACK_PIXELFORMAT_BGRA4444 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED16, CALLBACK_PACKEDORDER_BGRA,
			       CALLBACK_PACKEDLAYOUT_4444, 16, 2),
    CALLBACK_PIXELFORMAT_ARGB1555 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED16, CALLBACK_PACKEDORDER_ARGB,
			       CALLBACK_PACKEDLAYOUT_1555, 16, 2),
    CALLBACK_PIXELFORMAT_RGBA5551 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED16, CALLBACK_PACKEDORDER_RGBA,
			       CALLBACK_PACKEDLAYOUT_5551, 16, 2),
    CALLBACK_PIXELFORMAT_ABGR1555 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED16, CALLBACK_PACKEDORDER_ABGR,
			       CALLBACK_PACKEDLAYOUT_1555, 16, 2),
    CALLBACK_PIXELFORMAT_BGRA5551 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED16, CALLBACK_PACKEDORDER_BGRA,
			       CALLBACK_PACKEDLAYOUT_5551, 16, 2),
    CALLBACK_PIXELFORMAT_RGB565 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED16, CALLBACK_PACKEDORDER_XRGB,
			       CALLBACK_PACKEDLAYOUT_565, 16, 2),
    CALLBACK_PIXELFORMAT_BGR565 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED16, CALLBACK_PACKEDORDER_XBGR,
			       CALLBACK_PACKEDLAYOUT_565, 16, 2),
    CALLBACK_PIXELFORMAT_RGB24 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_ARRAYU8, CALLBACK_ARRAYORDER_RGB, 0,
			       24, 3),
    CALLBACK_PIXELFORMAT_BGR24 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_ARRAYU8, CALLBACK_ARRAYORDER_BGR, 0,
			       24, 3),
    CALLBACK_PIXELFORMAT_RGB888 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED32, CALLBACK_PACKEDORDER_XRGB,
			       CALLBACK_PACKEDLAYOUT_8888, 24, 4),
    CALLBACK_PIXELFORMAT_RGBX8888 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED32, CALLBACK_PACKEDORDER_RGBX,
			       CALLBACK_PACKEDLAYOUT_8888, 24, 4),
    CALLBACK_PIXELFORMAT_BGR888 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED32, CALLBACK_PACKEDORDER_XBGR,
			       CALLBACK_PACKEDLAYOUT_8888, 24, 4),
    CALLBACK_PIXELFORMAT_BGRX8888 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED32, CALLBACK_PACKEDORDER_BGRX,
			       CALLBACK_PACKEDLAYOUT_8888, 24, 4),
    CALLBACK_PIXELFORMAT_ARGB8888 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED32, CALLBACK_PACKEDORDER_ARGB,
			       CALLBACK_PACKEDLAYOUT_8888, 32, 4),
    CALLBACK_PIXELFORMAT_RGBA8888 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED32, CALLBACK_PACKEDORDER_RGBA,
			       CALLBACK_PACKEDLAYOUT_8888, 32, 4),
    CALLBACK_PIXELFORMAT_ABGR8888 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED32, CALLBACK_PACKEDORDER_ABGR,
			       CALLBACK_PACKEDLAYOUT_8888, 32, 4),
    CALLBACK_PIXELFORMAT_BGRA8888 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED32, CALLBACK_PACKEDORDER_BGRA,
			       CALLBACK_PACKEDLAYOUT_8888, 32, 4),
    CALLBACK_PIXELFORMAT_ARGB2101010 =
	CALLBACK_DEFINE_PIXELFORMAT(CALLBACK_PIXELTYPE_PACKED32, CALLBACK_PACKEDORDER_ARGB,
			       CALLBACK_PACKEDLAYOUT_2101010, 32, 4),

    CALLBACK_PIXELFORMAT_YV12 =      /**< Planar mode: Y + V + U  (3 planes) */
	CALLBACK_DEFINE_PIXELFOURCC('Y', 'V', '1', '2'),
    CALLBACK_PIXELFORMAT_IYUV =      /**< Planar mode: Y + U + V  (3 planes) */
	CALLBACK_DEFINE_PIXELFOURCC('I', 'Y', 'U', 'V'),
    CALLBACK_PIXELFORMAT_YUY2 =      /**< Packed mode: Y0+U0+Y1+V0 (1 plane) */
	CALLBACK_DEFINE_PIXELFOURCC('Y', 'U', 'Y', '2'),
    CALLBACK_PIXELFORMAT_UYVY =      /**< Packed mode: U0+Y0+V0+Y1 (1 plane) */
	CALLBACK_DEFINE_PIXELFOURCC('U', 'Y', 'V', 'Y'),
    CALLBACK_PIXELFORMAT_YVYU =      /**< Packed mode: Y0+V0+Y1+U0 (1 plane) */
	CALLBACK_DEFINE_PIXELFOURCC('Y', 'V', 'Y', 'U')
};

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
#   include "TargetConditionals.h"
#   include <Foundation/Foundation.h>
#endif

#define THIS_FILE		"callback_dev.c"
#define DEFAULT_CLOCK_RATE	90000
#define DEFAULT_WIDTH		640
#define DEFAULT_HEIGHT		480
#define DEFAULT_FPS		25

typedef struct callback_fmt_info
{
	pjmedia_format_id   fmt_id;
	Uint32              callback_format;
	Uint32              Rmask;
	Uint32              Gmask;
	Uint32              Bmask;
	Uint32              Amask;
} callback_fmt_info;

static callback_fmt_info callback_fmts[] =
{
	#if PJ_IS_BIG_ENDIAN
	{PJMEDIA_FORMAT_RGBA,  (Uint32)CALLBACK_PIXELFORMAT_RGBA8888,
	 0xFF000000, 0xFF0000, 0xFF00, 0xFF} ,
	{PJMEDIA_FORMAT_RGB24, (Uint32)CALLBACK_PIXELFORMAT_RGB24,
	 0xFF0000, 0xFF00, 0xFF, 0} ,
	{PJMEDIA_FORMAT_BGRA,  (Uint32)CALLBACK_PIXELFORMAT_BGRA8888,
	 0xFF00, 0xFF0000, 0xFF000000, 0xFF} ,
	#else /* PJ_IS_BIG_ENDIAN */
	{PJMEDIA_FORMAT_RGBA,  (Uint32)CALLBACK_PIXELFORMAT_ABGR8888,
	 0xFF, 0xFF00, 0xFF0000, 0xFF000000} ,
	{PJMEDIA_FORMAT_RGB24, (Uint32)CALLBACK_PIXELFORMAT_BGR24,
	 0xFF, 0xFF00, 0xFF0000, 0} ,
	{PJMEDIA_FORMAT_BGRA,  (Uint32)CALLBACK_PIXELFORMAT_ARGB8888,
	 0xFF0000, 0xFF00, 0xFF, 0xFF000000} ,
	#endif /* PJ_IS_BIG_ENDIAN */

	{PJMEDIA_FORMAT_DIB , (Uint32)CALLBACK_PIXELFORMAT_RGB24,
	 0xFF0000, 0xFF00, 0xFF, 0} ,

	{PJMEDIA_FORMAT_YUY2, CALLBACK_YUY2_OVERLAY, 0, 0, 0, 0} ,
	{PJMEDIA_FORMAT_UYVY, CALLBACK_UYVY_OVERLAY, 0, 0, 0, 0} ,
	{PJMEDIA_FORMAT_YVYU, CALLBACK_YVYU_OVERLAY, 0, 0, 0, 0} ,
	{PJMEDIA_FORMAT_I420, CALLBACK_IYUV_OVERLAY, 0, 0, 0, 0} ,
	{PJMEDIA_FORMAT_YV12, CALLBACK_YV12_OVERLAY, 0, 0, 0, 0} ,
	{PJMEDIA_FORMAT_I420JPEG, CALLBACK_IYUV_OVERLAY, 0, 0, 0, 0} ,
	{PJMEDIA_FORMAT_I422JPEG, CALLBACK_YV12_OVERLAY, 0, 0, 0, 0} ,
};

/* callback_ device info */
struct callback_dev_info
{
	pjmedia_vid_dev_info	 info;
};

/* Linked list of streams */
struct stream_list
{
	PJ_DECL_LIST_MEMBER(struct stream_list);
	struct callback_stream	*stream;
};

#define INITIAL_MAX_JOBS 64
#define JOB_QUEUE_INC_FACTOR 2

typedef pj_status_t (*job_func_ptr)(void *data);

typedef struct job {
	job_func_ptr    func;
	void           *data;
	unsigned        flags;
	pj_status_t     retval;
} job;

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
@interface JQDelegate: NSObject
{
	@public
	job *pjob;
}

- (void)run_job;
@end

@implementation JQDelegate
- (void)run_job
{
	pjob->retval = (*pjob->func)(pjob->data);
}
@end
#endif /* PJ_DARWINOS */

typedef struct job_queue {
	pj_pool_t      *pool;
	job           **jobs;
	pj_sem_t      **job_sem;
	pj_sem_t      **old_sem;
	pj_mutex_t     *mutex;
	pj_thread_t    *thread;
	pj_sem_t       *sem;

	unsigned        size;
	unsigned        head, tail;
	pj_bool_t	    is_full;
	pj_bool_t       is_quitting;
} job_queue;

/* callback_ factory */
struct callback_factory
{
	pjmedia_vid_dev_factory	 base;
	pj_pool_t			*pool;
	pj_pool_factory		*pf;

	unsigned			 dev_count;
	struct callback_dev_info	        *dev_info;
	job_queue                   *jq;

	pj_thread_t			*callback_thread;        /**< Callback thread.        */
	pj_sem_t                    *sem;
	pj_mutex_t			*mutex;
	struct stream_list		 streams;
	pj_bool_t                    is_quitting;
	pj_thread_desc 		 thread_desc;
	pj_thread_t 		*ev_thread;
};

/* Video stream. */
struct callback_stream
{
	pjmedia_vid_dev_stream	 base;		    /**< Base stream	    */
	pjmedia_vid_dev_param	 param;		    /**< Settings	    */
	pj_pool_t			*pool;              /**< Memory pool.       */

	pjmedia_vid_dev_cb		 vid_cb;            /**< Stream callback.   */
	void			*user_data;         /**< Application data.  */

	struct callback_factory          *sf;
	const pjmedia_frame         *frame;
	pj_bool_t			 is_running;
	pj_timestamp		 last_ts;
	struct stream_list		 list_entry;

	//	CALLBACK_Window                  *window;            /**< Display window.    */
	Uint32							windowID;
	//	CALLBACK_Renderer                *renderer;          /**< Display renderer.  */
	//	CALLBACK_Texture                 *scr_tex;           /**< Screen texture.    */
	int                          pitch;             /**< Pitch value.       */
	CB_Rect			 rect;              /**< Frame rectangle.   */
	CB_Rect			 dstrect;           /**< Display rectangle. */

	pjmedia_video_apply_fmt_param vafp;
};

/* Prototypes */
static pj_status_t callback_factory_init(pjmedia_vid_dev_factory *f);
static pj_status_t callback_factory_destroy(pjmedia_vid_dev_factory *f);
static pj_status_t callback_factory_refresh(pjmedia_vid_dev_factory *f);
static unsigned    callback_factory_get_dev_count(pjmedia_vid_dev_factory *f);
static pj_status_t callback_factory_get_dev_info(pjmedia_vid_dev_factory *f,
						 unsigned index,
						 pjmedia_vid_dev_info *info);
static pj_status_t callback_factory_default_param(pj_pool_t *pool,
						  pjmedia_vid_dev_factory *f,
						  unsigned index,
						  pjmedia_vid_dev_param *param);
static pj_status_t callback_factory_create_stream(
		pjmedia_vid_dev_factory *f,
		pjmedia_vid_dev_param *param,
		const pjmedia_vid_dev_cb *cb,
		void *user_data,
		pjmedia_vid_dev_stream **p_vid_strm);

static pj_status_t callback_stream_get_param(pjmedia_vid_dev_stream *strm,
					     pjmedia_vid_dev_param *param);
static pj_status_t callback_stream_get_cap(pjmedia_vid_dev_stream *strm,
					   pjmedia_vid_dev_cap cap,
					   void *value);
static pj_status_t callback_stream_set_cap(pjmedia_vid_dev_stream *strm,
					   pjmedia_vid_dev_cap cap,
					   const void *value);
static pj_status_t callback_stream_put_frame(pjmedia_vid_dev_stream *strm,
					     const pjmedia_frame *frame);
static pj_status_t callback_stream_start(pjmedia_vid_dev_stream *strm);
static pj_status_t callback_stream_stop(pjmedia_vid_dev_stream *strm);
static pj_status_t callback_stream_destroy(pjmedia_vid_dev_stream *strm);

static pj_status_t resize_disp(struct callback_stream *strm,
			       pjmedia_rect_size *new_disp_size);
static pj_status_t callback_destroy_all(void *data);

/* Job queue prototypes */
static pj_status_t job_queue_create(pj_pool_t *pool, job_queue **pjq);
static pj_status_t job_queue_post_job(job_queue *jq, job_func_ptr func,
				      void *data, unsigned flags,
				      pj_status_t *retval);
static pj_status_t job_queue_destroy(job_queue *jq);

/* Operations */
static pjmedia_vid_dev_factory_op factory_op =
{
	&callback_factory_init,
	&callback_factory_destroy,
	&callback_factory_get_dev_count,
	&callback_factory_get_dev_info,
	&callback_factory_default_param,
	&callback_factory_create_stream,
	&callback_factory_refresh
};

static pjmedia_vid_dev_stream_op stream_op =
{
	&callback_stream_get_param,
	&callback_stream_get_cap,
	&callback_stream_set_cap,
	&callback_stream_start,
	NULL,
	&callback_stream_put_frame,
	&callback_stream_stop,
	&callback_stream_destroy
};


/****************************************************************************
* Factory operations
*/
/*
* Init callback_ video driver.
*/
pjmedia_vid_dev_factory* pjmedia_callback_factory(pj_pool_factory *pf)
{
	struct callback_factory *f;
	pj_pool_t *pool;

	pool = pj_pool_create(pf, "callback video", 1000, 1000, NULL);
	f = PJ_POOL_ZALLOC_T(pool, struct callback_factory);
	f->pf = pf;
	f->pool = pool;
	f->base.op = &factory_op;

	return &f->base;
}

static pj_status_t callback_init(void * data)
{
	PJ_UNUSED_ARG(data);

	return PJ_SUCCESS;
}

static struct callback_stream* find_stream(struct callback_factory *sf,
					   Uint32 windowID,
					   pjmedia_event *pevent)
{
	struct stream_list *it, *itBegin;
	struct callback_stream *strm = NULL;

	itBegin = &sf->streams;
	for (it = itBegin->next; it != itBegin; it = it->next) {
		if (it->stream->windowID == windowID)
		{
			strm = it->stream;
			break;
		}
	}

	if (strm)
		pjmedia_event_init(pevent, PJMEDIA_EVENT_NONE, &strm->last_ts,
				   strm);

	return strm;
}

static pj_status_t handle_event(void *data)
{
//	struct callback_factory *sf = (struct callback_factory*)data;
//	CALLBACK_Event sevent;

//	if (!pj_thread_is_registered())
//		pj_thread_register("callback_ev", sf->thread_desc, &sf->ev_thread);

//	while (CALLBACK_PollEvent(&sevent)) {
//		struct callback_stream *strm = NULL;
//		pjmedia_event pevent;

//		pj_mutex_lock(sf->mutex);
//		pevent.type = PJMEDIA_EVENT_NONE;
//		switch(sevent.type) {
//		case CALLBACK_MOUSEBUTTONDOWN:
//			strm = find_stream(sf, sevent.button.windowID, &pevent);
//			pevent.type = PJMEDIA_EVENT_MOUSE_BTN_DOWN;
//			break;
//		case CALLBACK_WINDOWEVENT:
//			strm = find_stream(sf, sevent.window.windowID, &pevent);
//			switch (sevent.window.event) {
//			case CALLBACK_WINDOWEVENT_RESIZED:
//				pevent.type = PJMEDIA_EVENT_WND_RESIZED;
//				pevent.data.wnd_resized.new_size.w =
//						sevent.window.data1;
//				pevent.data.wnd_resized.new_size.h =
//						sevent.window.data2;
//				break;
//			case CALLBACK_WINDOWEVENT_CLOSE:
//				pevent.type = PJMEDIA_EVENT_WND_CLOSING;
//				break;
//			}
//			break;
//		default:
//			break;
//		}

//		if (strm && pevent.type != PJMEDIA_EVENT_NONE) {
//			pj_status_t status;

//			pjmedia_event_publish(NULL, strm, &pevent, 0);

//			switch (pevent.type) {
//			case PJMEDIA_EVENT_WND_RESIZED:
//				status = resize_disp(strm, &pevent.data.wnd_resized.new_size);
//				if (status != PJ_SUCCESS)
//					PJ_LOG(3, (THIS_FILE, "Failed resizing the display."));
//				break;
//			case PJMEDIA_EVENT_WND_CLOSING:
//				if (pevent.data.wnd_closing.cancel) {
//					/* Cancel the closing operation */
//					break;
//				}

//				/* Proceed to cleanup SDL. App must still call
//	* pjmedia_dev_stream_destroy() when getting WND_CLOSED
//	* event
//	*/
//				callback_stream_stop(&strm->base);
//				callback_destroy_all(strm);
//				pjmedia_event_init(&pevent, PJMEDIA_EVENT_WND_CLOSED,
//						   &strm->last_ts, strm);
//				pjmedia_event_publish(NULL, strm, &pevent, 0);

//				/*
//	* Note: don't access the stream after this point, it
//	* might have been destroyed
//	*/
//				break;
//			default:
//				/* Just to prevent gcc warning about unused enums */
//				break;
//			}
//		}

//		pj_mutex_unlock(sf->mutex);
//	}

	return PJ_SUCCESS;
}

static int callback_ev_thread(void *data)
{
	struct callback_factory *sf = (struct callback_factory*)data;

	while(1) {
		pj_status_t status;

		pj_mutex_lock(sf->mutex);
		if (pj_list_empty(&sf->streams)) {
			pj_mutex_unlock(sf->mutex);
			/* Wait until there is any stream. */
			pj_sem_wait(sf->sem);
		} else
			pj_mutex_unlock(sf->mutex);

		if (sf->is_quitting)
			break;

		job_queue_post_job(sf->jq, handle_event, sf, 0, &status);

		pj_thread_sleep(50);
	}

	return 0;
}

static pj_status_t callback_quit(void *data)
{
	PJ_UNUSED_ARG(data);
	//CALLBACK_Quit();
	return PJ_SUCCESS;
}

/* API: init factory */
static pj_status_t callback_factory_init(pjmedia_vid_dev_factory *f)
{
	struct callback_factory *sf = (struct callback_factory*)f;
	struct callback_dev_info *ddi;
	unsigned i, j;
	pj_status_t status;
	//CALLBACK_version version;
	uint ver_major = 0;
	uint ver_minor = 1;


	status = job_queue_create(sf->pool, &sf->jq);
	if (status != PJ_SUCCESS)
		return PJMEDIA_EVID_INIT;

	job_queue_post_job(sf->jq, callback_init, NULL, 0, &status);
	if (status != PJ_SUCCESS)
		return status;

	pj_list_init(&sf->streams);
	status = pj_mutex_create_recursive(sf->pool, "callback_factory",
					   &sf->mutex);
	if (status != PJ_SUCCESS)
		return status;

	status = pj_sem_create(sf->pool, NULL, 0, 1, &sf->sem);
	if (status != PJ_SUCCESS)
		return status;

	/* Create event handler thread. */
	status = pj_thread_create(sf->pool, "callback_thread", callback_ev_thread,
				  sf, 0, 0, &sf->callback_thread);
	if (status != PJ_SUCCESS)
		return status;

	sf->dev_count = 1;

	sf->dev_info = (struct callback_dev_info*)
			pj_pool_calloc(sf->pool, sf->dev_count,
				       sizeof(struct callback_dev_info));

	ddi = &sf->dev_info[0];
	pj_bzero(ddi, sizeof(*ddi));
	strncpy(ddi->info.name, "Callback renderer", sizeof(ddi->info.name));
	ddi->info.name[sizeof(ddi->info.name)-1] = '\0';
	ddi->info.fmt_cnt = PJ_ARRAY_SIZE(callback_fmts);

	for (i = 0; i < sf->dev_count; i++) {
		ddi = &sf->dev_info[i];
		strncpy(ddi->info.driver, "Callback", sizeof(ddi->info.driver));
		ddi->info.driver[sizeof(ddi->info.driver)-1] = '\0';
		ddi->info.dir = PJMEDIA_DIR_RENDER;
		ddi->info.has_callback = PJ_FALSE;
		ddi->info.caps = PJMEDIA_VID_DEV_CAP_FORMAT |
				PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE;
		ddi->info.caps |= PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW;
		ddi->info.caps |= PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW_FLAGS;

		for (j = 0; j < ddi->info.fmt_cnt; j++) {
			pjmedia_format *fmt = &ddi->info.fmt[j];
			pjmedia_format_init_video(fmt, callback_fmts[j].fmt_id,
						  DEFAULT_WIDTH, DEFAULT_HEIGHT,
						  DEFAULT_FPS, 1);
		}
	}

	PJ_LOG(4, (THIS_FILE, "Callback %d.%d initialized",
		   ver_major, ver_minor));

	return PJ_SUCCESS;
}

/* API: destroy factory */
static pj_status_t callback_factory_destroy(pjmedia_vid_dev_factory *f)
{
	struct callback_factory *sf = (struct callback_factory*)f;
	pj_pool_t *pool = sf->pool;
	pj_status_t status;

	pj_assert(pj_list_empty(&sf->streams));

	sf->is_quitting = PJ_TRUE;
	if (sf->callback_thread) {
		pj_sem_post(sf->sem);
#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
		/* To prevent pj_thread_join() of getting stuck if we are in
		* the main thread and we haven't finished processing the job
		* posted by callback_thread.
		*/
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
#endif
		pj_thread_join(sf->callback_thread);
	}

	if (sf->mutex) {
		pj_mutex_destroy(sf->mutex);
		sf->mutex = NULL;
	}

	if (sf->sem) {
		pj_sem_destroy(sf->sem);
		sf->sem = NULL;
	}

	job_queue_post_job(sf->jq, callback_quit, NULL, 0, &status);
	job_queue_destroy(sf->jq);

	sf->pool = NULL;
	pj_pool_release(pool);

	return PJ_SUCCESS;
}

/* API: refresh the list of devices */
static pj_status_t callback_factory_refresh(pjmedia_vid_dev_factory *f)
{
	PJ_UNUSED_ARG(f);
	return PJ_SUCCESS;
}

/* API: get number of devices */
static unsigned callback_factory_get_dev_count(pjmedia_vid_dev_factory *f)
{
	struct callback_factory *sf = (struct callback_factory*)f;
	return sf->dev_count;
}

/* API: get device info */
static pj_status_t callback_factory_get_dev_info(pjmedia_vid_dev_factory *f,
						 unsigned index,
						 pjmedia_vid_dev_info *info)
{
	struct callback_factory *sf = (struct callback_factory*)f;

	PJ_ASSERT_RETURN(index < sf->dev_count, PJMEDIA_EVID_INVDEV);

	pj_memcpy(info, &sf->dev_info[index].info, sizeof(*info));

	return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t callback_factory_default_param(pj_pool_t *pool,
						  pjmedia_vid_dev_factory *f,
						  unsigned index,
						  pjmedia_vid_dev_param *param)
{
	struct callback_factory *sf = (struct callback_factory*)f;
	struct callback_dev_info *di = &sf->dev_info[index];

	PJ_ASSERT_RETURN(index < sf->dev_count, PJMEDIA_EVID_INVDEV);

	PJ_UNUSED_ARG(pool);

	pj_bzero(param, sizeof(*param));
	param->dir = PJMEDIA_DIR_RENDER;
	param->rend_id = index;
	param->cap_id = PJMEDIA_VID_INVALID_DEV;

	/* Set the device capabilities here */
	param->flags = PJMEDIA_VID_DEV_CAP_FORMAT;
	param->fmt.type = PJMEDIA_TYPE_VIDEO;
	param->clock_rate = DEFAULT_CLOCK_RATE;
	pj_memcpy(&param->fmt, &di->info.fmt[0], sizeof(param->fmt));

	return PJ_SUCCESS;
}

static callback_fmt_info* get_callback_format_info(pjmedia_format_id id)
{
	unsigned i;

	for (i = 0; i < sizeof(callback_fmts)/sizeof(callback_fmts[0]); i++) {
		if (callback_fmts[i].fmt_id == id)
			return &callback_fmts[i];
	}

	return NULL;
}

static pj_status_t callback_destroy(void *data)
{
	PJ_UNUSED_ARG(data);
	return PJ_SUCCESS;
}

static pj_status_t callback_destroy_all(void *data)
{
	struct callback_stream *strm = (struct callback_stream *)data;

	callback_destroy(data);
#if !defined(TARGET_OS_IPHONE) || TARGET_OS_IPHONE == 0
	strm->windowID = 0;
#endif /* !TARGET_OS_IPHONE */

	return PJ_SUCCESS;
}

static pj_status_t callback_create_rend(struct callback_stream * strm,
					pjmedia_format *fmt)
{
	callback_fmt_info *callback_info;
	const pjmedia_video_format_info *vfi;
	pjmedia_video_format_detail *vfd;

	callback_info = get_callback_format_info(fmt->id);
	vfi = pjmedia_get_video_format_info(pjmedia_video_format_mgr_instance(),
					    fmt->id);
	if (!vfi || !callback_info)
		return PJMEDIA_EVID_BADFORMAT;

	strm->vafp.size = fmt->det.vid.size;
	strm->vafp.buffer = NULL;
	if (vfi->apply_fmt(vfi, &strm->vafp) != PJ_SUCCESS)
		return PJMEDIA_EVID_BADFORMAT;

	vfd = pjmedia_format_get_video_format_detail(fmt, PJ_TRUE);
	strm->rect.x = strm->rect.y = 0;
	strm->rect.w = (Uint16)vfd->size.w;
	strm->rect.h = (Uint16)vfd->size.h;
	if (strm->param.disp_size.w == 0)
		strm->param.disp_size.w = strm->rect.w;
	if (strm->param.disp_size.h == 0)
		strm->param.disp_size.h = strm->rect.h;
	strm->dstrect.x = strm->dstrect.y = 0;
	strm->dstrect.w = (Uint16)strm->param.disp_size.w;
	strm->dstrect.h = (Uint16)strm->param.disp_size.h;

	callback_destroy(strm);

	strm->pitch = strm->rect.w * CALLBACK_BYTESPERPIXEL(callback_info->callback_format);

	return PJ_SUCCESS;
}

static pj_status_t callback_create(void *data)
{
	struct callback_stream *strm = (struct callback_stream *)data;
	return callback_create_rend(strm, &strm->param.fmt);
}

static pj_status_t resize_disp(struct callback_stream *strm,
			       pjmedia_rect_size *new_disp_size)
{
	pj_memcpy(&strm->param.disp_size, new_disp_size,
		  sizeof(strm->param.disp_size));

	return PJ_SUCCESS;
}

static pj_status_t change_format(struct callback_stream *strm,
				 pjmedia_format *new_fmt)
{
	pj_status_t status;

	/* Recreate Callback renderer */
	status = callback_create_rend(strm, (new_fmt? new_fmt :
						      &strm->param.fmt));
	if (status == PJ_SUCCESS && new_fmt)
		pjmedia_format_copy(&strm->param.fmt, new_fmt);

	return status;
}

static pj_status_t put_frame(void *data)
{
	struct callback_stream *stream = (struct callback_stream *)data;

	// working with callback functions in myframe
	if(myframe.put_frame_callback)
		myframe.put_frame_callback(stream->frame, stream->rect.w, stream->rect.h, stream->pitch);

	return PJ_SUCCESS;
}

/* API: Put frame from stream */
static pj_status_t callback_stream_put_frame(pjmedia_vid_dev_stream *strm, const pjmedia_frame *frame)
{
	struct callback_stream *stream = (struct callback_stream*)strm;
	pj_status_t status;

	stream->last_ts.u64 = frame->timestamp.u64;

	if (!stream->is_running)
		return PJ_EINVALIDOP;

	if (frame->size==0 || frame->buf==NULL ||
			frame->size < stream->vafp.framebytes)
		return PJ_SUCCESS;

	stream->frame = frame;
	job_queue_post_job(stream->sf->jq, put_frame, strm, 0, &status);


	//if(myframe.put_frame_callback)
	//	myframe.put_frame_callback(stream->frame, stream->rect.w, stream->rect.h);

	return status;
}

/* API: create stream */
static pj_status_t callback_factory_create_stream(
		pjmedia_vid_dev_factory *f,
		pjmedia_vid_dev_param *param,
		const pjmedia_vid_dev_cb *cb,
		void *user_data,
		pjmedia_vid_dev_stream **p_vid_strm)
{
	struct callback_factory *sf = (struct callback_factory*)f;
	pj_pool_t *pool;
	struct callback_stream *strm;
	pj_status_t status;

	PJ_ASSERT_RETURN(param->dir == PJMEDIA_DIR_RENDER, PJ_EINVAL);

	/* Create and Initialize stream descriptor */
	pool = pj_pool_create(sf->pf, "sdl-dev", 1000, 1000, NULL);
	PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

	strm = PJ_POOL_ZALLOC_T(pool, struct callback_stream);
	pj_memcpy(&strm->param, param, sizeof(*param));
	strm->pool = pool;
	strm->sf = sf;
	pj_memcpy(&strm->vid_cb, cb, sizeof(*cb));
	pj_list_init(&strm->list_entry);
	strm->list_entry.stream = strm;
	strm->user_data = user_data;

	/* Create render stream here */
	job_queue_post_job(sf->jq, callback_create, strm, 0, &status);
	if (status != PJ_SUCCESS) {
		goto on_error;
	}
	pj_mutex_lock(strm->sf->mutex);
	if (pj_list_empty(&strm->sf->streams))
		pj_sem_post(strm->sf->sem);
	pj_list_insert_after(&strm->sf->streams, &strm->list_entry);
	pj_mutex_unlock(strm->sf->mutex);

	/* Apply the remaining settings */
	if (param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION) {
		callback_stream_set_cap(&strm->base,
					PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION,
					&param->window_pos);
	}

	/* Done */
	strm->base.op = &stream_op;
	*p_vid_strm = &strm->base;

	return PJ_SUCCESS;

on_error:
	callback_stream_destroy(&strm->base);
	return status;
}

/* API: Get stream info. */
static pj_status_t callback_stream_get_param(pjmedia_vid_dev_stream *s,
					     pjmedia_vid_dev_param *pi)
{
	struct callback_stream *strm = (struct callback_stream*)s;

	PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

	pj_memcpy(pi, &strm->param, sizeof(*pi));

	if (callback_stream_get_cap(s, PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW,
				    &pi->window) == PJ_SUCCESS)
	{
		pi->flags |= PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW;
	}
	if (callback_stream_get_cap(s, PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION,
				    &pi->window_pos) == PJ_SUCCESS)
	{
		pi->flags |= PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION;
	}
	if (callback_stream_get_cap(s, PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE,
				    &pi->disp_size) == PJ_SUCCESS)
	{
		pi->flags |= PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE;
	}
	if (callback_stream_get_cap(s, PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE,
				    &pi->window_hide) == PJ_SUCCESS)
	{
		pi->flags |= PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE;
	}
	if (callback_stream_get_cap(s, PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW_FLAGS,
				    &pi->window_flags) == PJ_SUCCESS)
	{
		pi->flags |= PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW_FLAGS;
	}

	return PJ_SUCCESS;
}

struct strm_cap {
	struct callback_stream   *strm;
	pjmedia_vid_dev_cap  cap;
	union {
		void            *pval;
		const void      *cpval;
	} pval;
};

static pj_status_t get_cap(void *data)
{
	PJ_UNUSED_ARG(data);
	return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t callback_stream_get_cap(pjmedia_vid_dev_stream *s,
					   pjmedia_vid_dev_cap cap,
					   void *pval)
{
	struct callback_stream *strm = (struct callback_stream*)s;
	struct strm_cap scap;
	pj_status_t status;

	PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

	scap.strm = strm;
	scap.cap = cap;
	scap.pval.pval = pval;

	job_queue_post_job(strm->sf->jq, get_cap, &scap, 0, &status);

	return status;
}

static pj_status_t set_cap(void *data)
{
	struct strm_cap *scap = (struct strm_cap *)data;
	struct callback_stream *strm = scap->strm;
	pjmedia_vid_dev_cap cap = scap->cap;
	const void *pval = scap->pval.cpval;

	if (cap == PJMEDIA_VID_DEV_CAP_FORMAT) {
		pj_status_t status;

		status = change_format(strm, (pjmedia_format *)pval);
		if (status != PJ_SUCCESS) {
			pj_status_t status_;

			/**
			* Failed to change the output format. Try to revert
			* to its original format.
			*/
			status_ = change_format(strm, &strm->param.fmt);
			if (status_ != PJ_SUCCESS) {
				/**
				* This means that we failed to revert to our
				* original state!
				*/
				status = PJMEDIA_EVID_ERR;
			}
		}

		return status;
	}

	return PJMEDIA_EVID_INVCAP;
}

/* API: set capability */
static pj_status_t callback_stream_set_cap(pjmedia_vid_dev_stream *s,
					   pjmedia_vid_dev_cap cap,
					   const void *pval)
{
	struct callback_stream *strm = (struct callback_stream*)s;
	struct strm_cap scap;
	pj_status_t status;

	PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

	scap.strm = strm;
	scap.cap = cap;
	scap.pval.cpval = pval;

	job_queue_post_job(strm->sf->jq, set_cap, &scap, 0, &status);

	return status;
}

/* API: Start stream. */
static pj_status_t callback_stream_start(pjmedia_vid_dev_stream *strm)
{
	struct callback_stream *stream = (struct callback_stream*)strm;

	PJ_LOG(4, (THIS_FILE, "Starting callback video stream"));

	stream->is_running = PJ_TRUE;

	return PJ_SUCCESS;
}


/* API: Stop stream. */
static pj_status_t callback_stream_stop(pjmedia_vid_dev_stream *strm)
{
	struct callback_stream *stream = (struct callback_stream*)strm;

	PJ_LOG(4, (THIS_FILE, "Stopping callback video stream"));

	stream->is_running = PJ_FALSE;

	return PJ_SUCCESS;
}


/* API: Destroy stream. */
static pj_status_t callback_stream_destroy(pjmedia_vid_dev_stream *strm)
{
	struct callback_stream *stream = (struct callback_stream*)strm;
	pj_status_t status;

	PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

	callback_stream_stop(strm);

	job_queue_post_job(stream->sf->jq, callback_destroy_all, strm, 0, &status);
	if (status != PJ_SUCCESS)
		return status;

	pj_mutex_lock(stream->sf->mutex);
	if (!pj_list_empty(&stream->list_entry))
		pj_list_erase(&stream->list_entry);
	pj_mutex_unlock(stream->sf->mutex);

	pj_pool_release(stream->pool);

	return PJ_SUCCESS;
}

/****************************************************************************
* Job queue implementation
*/
static int job_thread(void * data)
{
	job_queue *jq = (job_queue *)data;

	while (1)
	{
		job *jb;

		/* Wait until there is a job. */
		pj_sem_wait(jq->sem);

		/* Make sure there is no pending jobs before we quit. */
		if (jq->is_quitting && jq->head == jq->tail && !jq->is_full)
			break;

		jb = jq->jobs[jq->head];
		jb->retval = (*jb->func)(jb->data);
		/* If job queue is full and we already finish all the pending
		* jobs, increase the size.
		*/
		if (jq->is_full && ((jq->head + 1) % jq->size == jq->tail))
		{
			unsigned i, head;

			if (jq->old_sem)
			{
				for (i = 0; i < jq->size / JOB_QUEUE_INC_FACTOR; i++)
				{
					pj_sem_destroy(jq->old_sem[i]);
				}
			}
			jq->old_sem = jq->job_sem;

			/* Double the job queue size. */
			jq->size *= JOB_QUEUE_INC_FACTOR;
			pj_sem_destroy(jq->sem);
			pj_sem_create(jq->pool, "thread_sem", 0, jq->size + 1, &jq->sem);
			jq->jobs = (job **)pj_pool_calloc(jq->pool, jq->size, sizeof(job *));
			jq->job_sem = (pj_sem_t **) pj_pool_calloc(jq->pool, jq->size, sizeof(pj_sem_t *));
			for (i = 0; i < jq->size; i++)
			{
				pj_sem_create(jq->pool, "job_sem", 0, 1, &jq->job_sem[i]);
			}
			jq->is_full = PJ_FALSE;
			head = jq->head;
			jq->head = jq->tail = 0;
			pj_sem_post(jq->old_sem[head]);
		}
		else
		{
			pj_sem_post(jq->job_sem[jq->head]);
			jq->head = (jq->head + 1) % jq->size;
		}
	}

	return 0;
}

static pj_status_t job_queue_create(pj_pool_t *pool, job_queue **pjq)
{
	unsigned i;
	pj_status_t status;

	job_queue *jq = PJ_POOL_ZALLOC_T(pool, job_queue);

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
	PJ_UNUSED_ARG(status);
#else
	status = pj_thread_create(pool, "job_th", job_thread, jq, 0, 0, &jq->thread);
	if (status != PJ_SUCCESS)
	{
		job_queue_destroy(jq);
		return status;
	}
#endif /* PJ_DARWINOS */

	jq->pool = pool;
	jq->size = INITIAL_MAX_JOBS;
	pj_sem_create(pool, "thread_sem", 0, jq->size + 1, &jq->sem);
	jq->jobs = (job **)pj_pool_calloc(pool, jq->size, sizeof(job *));
	jq->job_sem = (pj_sem_t **) pj_pool_calloc(pool, jq->size, sizeof(pj_sem_t *));
	for (i = 0; i < jq->size; i++)
	{
		pj_sem_create(pool, "job_sem", 0, 1, &jq->job_sem[i]);
	}
	pj_mutex_create_recursive(pool, "job_mutex", &jq->mutex);

	*pjq = jq;
	return PJ_SUCCESS;
}

static pj_status_t job_queue_post_job(job_queue *jq, job_func_ptr func,
				      void *data, unsigned flags,
				      pj_status_t *retval)
{
	job jb;
	int tail;

	if (jq->is_quitting)
		return PJ_EBUSY;

	jb.func = func;
	jb.data = data;
	jb.flags = flags;

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
	PJ_UNUSED_ARG(tail);
	NSAutoreleasePool *apool = [[NSAutoreleasePool alloc]init];
	JQDelegate *jqd = [[JQDelegate alloc]init];
	jqd->pjob = &jb;
	[jqd performSelectorOnMainThread:@selector(run_job)
	      withObject:nil waitUntilDone:YES];
	[jqd release];
	[apool release];
#else /* PJ_DARWINOS */
	pj_mutex_lock(jq->mutex);
	jq->jobs[jq->tail] = &jb;
	tail = jq->tail;
	jq->tail = (jq->tail + 1) % jq->size;
	if (jq->tail == jq->head) {
		jq->is_full = PJ_TRUE;
		PJ_LOG(4, (THIS_FILE, "SDL job queue is full, increasing "
			   "the queue size."));
		pj_sem_post(jq->sem);
		/* Wait until our posted job is completed. */
		pj_sem_wait(jq->job_sem[tail]);
		pj_mutex_unlock(jq->mutex);
	} else {
		pj_mutex_unlock(jq->mutex);
		pj_sem_post(jq->sem);
		/* Wait until our posted job is completed. */
		pj_sem_wait(jq->job_sem[tail]);
	}
#endif /* PJ_DARWINOS */

	*retval = jb.retval;

	return PJ_SUCCESS;
}

static pj_status_t job_queue_destroy(job_queue *jq)
{
	unsigned i;

	jq->is_quitting = PJ_TRUE;

	if (jq->thread) {
		pj_sem_post(jq->sem);
		pj_thread_join(jq->thread);
	}

	if (jq->sem) {
		pj_sem_destroy(jq->sem);
		jq->sem = NULL;
	}
	for (i = 0; i < jq->size; i++) {
		pj_sem_destroy(jq->job_sem[i]);
	}
	if (jq->old_sem) {
		for (i = 0; i < jq->size / JOB_QUEUE_INC_FACTOR; i++) {
			pj_sem_destroy(jq->old_sem[i]);
		}
	}
	pj_mutex_destroy(jq->mutex);

	return PJ_SUCCESS;
}

#endif	/* PJMEDIA_VIDEO_DEV_HAS_CALLBACK */
