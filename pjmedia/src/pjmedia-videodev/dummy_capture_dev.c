/* $Id: dummy_capture_dev.c 0001 2012-09-06 14:18:00Z popov $ */
/*
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
#include <pj/rand.h>


#if defined(PJMEDIA_VIDEO_DEV_HAS_DCDEV_SRC) &&  PJMEDIA_VIDEO_DEV_HAS_DCDEV_SRC != 0


#define THIS_FILE		"dummy_capture_dev.c"
#define DEFAULT_CLOCK_RATE	90000
#define DEFAULT_WIDTH		352 //640
#define DEFAULT_HEIGHT		288 //480
#define DEFAULT_FPS		25

/* dcdev_ device info */
struct dcdev_dev_info
{
	pjmedia_vid_dev_info	 info;
};

/* dcdev_ factory */
struct dcdev_factory
{
	pjmedia_vid_dev_factory	 base;
	pj_pool_t			*pool;
	pj_pool_factory		*pf;

	unsigned			 dev_count;
	struct dcdev_dev_info	*dev_info;
};

struct dcdev_fmt_info {
	pjmedia_format_id            fmt_id;        /* Format ID                */

	/* Info for packed formats. */
	unsigned                     c_offset[3];   /* Color component offset, 
																							in bytes                 */
	unsigned                     c_stride[3];   /* Color component stride,
																							or distance between two 
																							consecutive same color
																							components, in bytes     */
};

/* Dummy capture video source supports */
static struct dcdev_fmt_info dcdev_fmts[] =
{
	/* Packed formats */
	{ PJMEDIA_FORMAT_YUY2,      {0, 1, 3}, {2, 4, 4} },
	{ PJMEDIA_FORMAT_UYVY,      {1, 0, 2}, {2, 4, 4} },
	{ PJMEDIA_FORMAT_YVYU,      {0, 3, 1}, {2, 4, 4} },
	{ PJMEDIA_FORMAT_RGBA,      {0, 1, 2}, {4, 4, 4} },
	{ PJMEDIA_FORMAT_RGB24,     {0, 1, 2}, {3, 3, 3} },
	{ PJMEDIA_FORMAT_BGRA,      {2, 1, 0}, {4, 4, 4} },

	/* Planar formats */
	{ PJMEDIA_FORMAT_YV12 },
	{ PJMEDIA_FORMAT_I420 },
	{ PJMEDIA_FORMAT_I420JPEG },
	{ PJMEDIA_FORMAT_I422JPEG },
};

/* Video stream. */
struct dcdev_stream
{
	pjmedia_vid_dev_stream	     base;	    /**< Base stream	    */
	pjmedia_vid_dev_param	     param;	    /**< Settings	    */
	pj_pool_t			    *pool;          /**< Memory pool.       */

	pjmedia_vid_dev_cb		     vid_cb;	    /**< Stream callback.   */
	void			    *user_data;	    /**< Application data.  */

	const struct dcdev_fmt_info      *cbfi;
	const pjmedia_video_format_info *vfi;
	pjmedia_video_apply_fmt_param    vafp;
	pj_uint8_t                      *first_line[PJMEDIA_MAX_VIDEO_PLANES];
	pj_timestamp		     ts;
	unsigned			     ts_inc;
};


/* Prototypes */
static pj_status_t dcdev_factory_init(pjmedia_vid_dev_factory *f);
static pj_status_t dcdev_factory_destroy(pjmedia_vid_dev_factory *f);
static pj_status_t dcdev_factory_refresh(pjmedia_vid_dev_factory *f); 
static unsigned    dcdev_factory_get_dev_count(pjmedia_vid_dev_factory *f);
static pj_status_t dcdev_factory_get_dev_info(pjmedia_vid_dev_factory *f,
																						 unsigned index,
																						 pjmedia_vid_dev_info *info);
static pj_status_t dcdev_factory_default_param(pj_pool_t *pool,
																							pjmedia_vid_dev_factory *f,
																							unsigned index,
																							pjmedia_vid_dev_param *param);
static pj_status_t dcdev_factory_create_stream(
	pjmedia_vid_dev_factory *f,
	pjmedia_vid_dev_param *param,
	const pjmedia_vid_dev_cb *cb,
	void *user_data,
	pjmedia_vid_dev_stream **p_vid_strm);

static pj_status_t dcdev_stream_get_param(pjmedia_vid_dev_stream *strm,
																				 pjmedia_vid_dev_param *param);
static pj_status_t dcdev_stream_get_cap(pjmedia_vid_dev_stream *strm,
																			 pjmedia_vid_dev_cap cap,
																			 void *value);
static pj_status_t dcdev_stream_set_cap(pjmedia_vid_dev_stream *strm,
																			 pjmedia_vid_dev_cap cap,
																			 const void *value);
static pj_status_t dcdev_stream_get_frame(pjmedia_vid_dev_stream *strm,
																				 pjmedia_frame *frame);
static pj_status_t dcdev_stream_start(pjmedia_vid_dev_stream *strm);
static pj_status_t dcdev_stream_stop(pjmedia_vid_dev_stream *strm);
static pj_status_t dcdev_stream_destroy(pjmedia_vid_dev_stream *strm);

/* Operations */
static pjmedia_vid_dev_factory_op factory_op =
{
	&dcdev_factory_init,
	&dcdev_factory_destroy,
	&dcdev_factory_get_dev_count,
	&dcdev_factory_get_dev_info,
	&dcdev_factory_default_param,
	&dcdev_factory_create_stream,
	&dcdev_factory_refresh
};

static pjmedia_vid_dev_stream_op stream_op =
{
	&dcdev_stream_get_param,
	&dcdev_stream_get_cap,
	&dcdev_stream_set_cap,
	&dcdev_stream_start,
	&dcdev_stream_get_frame,
	NULL,
	&dcdev_stream_stop,
	&dcdev_stream_destroy,
	&dcdev_stream_stop
};


/****************************************************************************
* Factory operations
*/
/*
* Init dcdev_ video driver.
*/
pjmedia_vid_dev_factory* pjmedia_dcdev_factory(pj_pool_factory *pf)
{
	struct dcdev_factory *f;
	pj_pool_t *pool;

	pool = pj_pool_create(pf, "dcdev video", 512, 512, NULL);
	f = PJ_POOL_ZALLOC_T(pool, struct dcdev_factory);
	f->pf = pf;
	f->pool = pool;
	f->base.op = &factory_op;

	return &f->base;
}


/* API: init factory */
static pj_status_t dcdev_factory_init(pjmedia_vid_dev_factory *f)
{
	struct dcdev_factory *cf = (struct dcdev_factory*)f;
	struct dcdev_dev_info *ddi;
	unsigned i;

	cf->dev_count = 1;
	cf->dev_info = (struct dcdev_dev_info*)
		pj_pool_calloc(cf->pool, cf->dev_count,
		sizeof(struct dcdev_dev_info));

	ddi = &cf->dev_info[0];
	pj_bzero(ddi, sizeof(*ddi));
	pj_ansi_strncpy(ddi->info.name, "Dummy capture stub", sizeof(ddi->info.name));
	ddi->info.driver[sizeof(ddi->info.driver)-1] = '\0';
	pj_ansi_strncpy(ddi->info.driver, "DummyCapture", sizeof(ddi->info.driver));
	ddi->info.driver[sizeof(ddi->info.driver)-1] = '\0';
	ddi->info.dir = PJMEDIA_DIR_CAPTURE;
	ddi->info.has_callback = PJ_FALSE;

	ddi->info.caps = PJMEDIA_VID_DEV_CAP_FORMAT;
	ddi->info.fmt_cnt = sizeof(dcdev_fmts)/sizeof(dcdev_fmts[0]);
	for (i = 0; i < ddi->info.fmt_cnt; i++) {
		pjmedia_format *fmt = &ddi->info.fmt[i];
		pjmedia_format_init_video(fmt, dcdev_fmts[i].fmt_id,
			DEFAULT_WIDTH, DEFAULT_HEIGHT,
			DEFAULT_FPS, 1);
	}

	PJ_LOG(4, (THIS_FILE, "Dummy capture video src initialized with %d device(s):", cf->dev_count));
	for (i = 0; i < cf->dev_count; i++) {
		PJ_LOG(4, (THIS_FILE, "%2d: %s", i, cf->dev_info[i].info.name));
	}

	return PJ_SUCCESS;
}

/* API: destroy factory */
static pj_status_t dcdev_factory_destroy(pjmedia_vid_dev_factory *f)
{
	struct dcdev_factory *cf = (struct dcdev_factory*)f;
	pj_pool_t *pool = cf->pool;

	cf->pool = NULL;
	pj_pool_release(pool);

	return PJ_SUCCESS;
}

/* API: refresh the list of devices */
static pj_status_t dcdev_factory_refresh(pjmedia_vid_dev_factory *f)
{
	PJ_UNUSED_ARG(f);
	return PJ_SUCCESS;
}

/* API: get number of devices */
static unsigned dcdev_factory_get_dev_count(pjmedia_vid_dev_factory *f)
{
	struct dcdev_factory *cf = (struct dcdev_factory*)f;
	return cf->dev_count;
}

/* API: get device info */
static pj_status_t dcdev_factory_get_dev_info(pjmedia_vid_dev_factory *f,
																						 unsigned index,
																						 pjmedia_vid_dev_info *info)
{
	struct dcdev_factory *cf = (struct dcdev_factory*)f;

	PJ_ASSERT_RETURN(index < cf->dev_count, PJMEDIA_EVID_INVDEV);

	pj_memcpy(info, &cf->dev_info[index].info, sizeof(*info));

	return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t dcdev_factory_default_param(pj_pool_t *pool,
																							pjmedia_vid_dev_factory *f,
																							unsigned index,
																							pjmedia_vid_dev_param *param)
{
	struct dcdev_factory *cf = (struct dcdev_factory*)f;
	struct dcdev_dev_info *di = &cf->dev_info[index];

	PJ_ASSERT_RETURN(index < cf->dev_count, PJMEDIA_EVID_INVDEV);

	PJ_UNUSED_ARG(pool);

	pj_bzero(param, sizeof(*param));
	param->dir = PJMEDIA_DIR_CAPTURE;
	param->cap_id = index;
	param->rend_id = PJMEDIA_VID_INVALID_DEV;
	param->flags = PJMEDIA_VID_DEV_CAP_FORMAT;
	param->clock_rate = DEFAULT_CLOCK_RATE;
	pj_memcpy(&param->fmt, &di->info.fmt[0], sizeof(param->fmt));

	return PJ_SUCCESS;
}

static const struct dcdev_fmt_info* get_dcdev_fmt_info(pjmedia_format_id id)
{
	unsigned i;

	for (i = 0; i < sizeof(dcdev_fmts)/sizeof(dcdev_fmts[0]); i++) {
		if (dcdev_fmts[i].fmt_id == id)
			return &dcdev_fmts[i];
	}

	return NULL;
}



/* API: create stream */
static pj_status_t dcdev_factory_create_stream(
	pjmedia_vid_dev_factory *f,
	pjmedia_vid_dev_param *param,
	const pjmedia_vid_dev_cb *cb,
	void *user_data,
	pjmedia_vid_dev_stream **p_vid_strm)
{
	struct dcdev_factory *cf = (struct dcdev_factory*)f;
	pj_pool_t *pool;
	struct dcdev_stream *strm;
	const pjmedia_video_format_detail *vfd;
	const pjmedia_video_format_info *vfi;
	pjmedia_video_apply_fmt_param vafp;
	const struct dcdev_fmt_info *cbfi;
	unsigned i;

	PJ_ASSERT_RETURN(f && param && p_vid_strm, PJ_EINVAL);
	PJ_ASSERT_RETURN(param->fmt.type == PJMEDIA_TYPE_VIDEO &&
		param->fmt.detail_type == PJMEDIA_FORMAT_DETAIL_VIDEO &&
		param->dir == PJMEDIA_DIR_CAPTURE,
		PJ_EINVAL);

	pj_bzero(&vafp, sizeof(vafp));

	vfd = pjmedia_format_get_video_format_detail(&param->fmt, PJ_TRUE);
	vfi = pjmedia_get_video_format_info(NULL, param->fmt.id);
	cbfi = get_dcdev_fmt_info(param->fmt.id);
	if (!vfi || !cbfi)
		return PJMEDIA_EVID_BADFORMAT;

	vafp.size = param->fmt.det.vid.size;
	if (vfi->apply_fmt(vfi, &vafp) != PJ_SUCCESS)
		return PJMEDIA_EVID_BADFORMAT;

	/* Create and Initialize stream descriptor */
	pool = pj_pool_create(cf->pf, "dcap-dev", 512, 512, NULL);
	PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

	strm = PJ_POOL_ZALLOC_T(pool, struct dcdev_stream);
	pj_memcpy(&strm->param, param, sizeof(*param));
	strm->pool = pool;
	pj_memcpy(&strm->vid_cb, cb, sizeof(*cb));
	strm->user_data = user_data;
	strm->vfi = vfi;
	strm->cbfi = cbfi;
	pj_memcpy(&strm->vafp, &vafp, sizeof(vafp));
	strm->ts_inc = PJMEDIA_SPF2(param->clock_rate, &vfd->fps, 1);

	for (i = 0; i < vfi->plane_cnt; ++i) {
		strm->first_line[i] = pj_pool_alloc(pool, vafp.strides[i]);
		pj_memset(strm->first_line[i], 255, vafp.strides[i]);
	}

	//fill_first_line(strm->first_line, strm->cbfi, vfi, &strm->vafp);

	/* Apply the remaining settings */
	/*    if (param->flags & PJMEDIA_VID_DEV_CAP_INPUT_SCALE) {
	cbar_stream_set_cap(&strm->base,
	PJMEDIA_VID_DEV_CAP_INPUT_SCALE,
	&param->fmt);
	}
	*/
	/* Done */
	strm->base.op = &stream_op;
	*p_vid_strm = &strm->base;

	return PJ_SUCCESS;
}

/* API: Get stream info. */
static pj_status_t dcdev_stream_get_param(pjmedia_vid_dev_stream *s,
																				 pjmedia_vid_dev_param *pi)
{
	struct dcdev_stream *strm = (struct dcdev_stream*)s;

	PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

	pj_memcpy(pi, &strm->param, sizeof(*pi));

	/*    if (cbar_stream_get_cap(s, PJMEDIA_VID_DEV_CAP_INPUT_SCALE,
	&pi->fmt.info_size) == PJ_SUCCESS)
	{
	pi->flags |= PJMEDIA_VID_DEV_CAP_INPUT_SCALE;
	}
	*/
	return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t dcdev_stream_get_cap(pjmedia_vid_dev_stream *s,
																			 pjmedia_vid_dev_cap cap,
																			 void *pval)
{
	struct dcdev_stream *strm = (struct dcdev_stream*)s;

	PJ_UNUSED_ARG(strm);

	PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

	if (cap==PJMEDIA_VID_DEV_CAP_INPUT_SCALE)
	{
		return PJMEDIA_EVID_INVCAP;
		//	return PJ_SUCCESS;
	} else {
		return PJMEDIA_EVID_INVCAP;
	}
}

/* API: set capability */
static pj_status_t dcdev_stream_set_cap(pjmedia_vid_dev_stream *s,
																			 pjmedia_vid_dev_cap cap,
																			 const void *pval)
{
	struct dcdev_stream *strm = (struct dcdev_stream*)s;

	PJ_UNUSED_ARG(strm);

	PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

	if (cap==PJMEDIA_VID_DEV_CAP_INPUT_SCALE)
	{
		return PJ_SUCCESS;
	}

	return PJMEDIA_EVID_INVCAP;
}

//static pj_status_t spectrum_run(struct cbar_stream *d, pj_uint8_t *p,
//																pj_size_t size)
//{
//	unsigned i;
//	pj_uint8_t *ptr = p;
//	pj_time_val tv;
//
//	PJ_UNUSED_ARG(size);
//
//	/* Subsequent lines */
//	for (i=0; i<d->vfi->plane_cnt; ++i) {
//		pj_uint8_t *plane_end;
//
//		plane_end = ptr + d->vafp.plane_bytes[i];
//		while (ptr < plane_end) {
//			pj_memcpy(ptr, d->first_line[i], d->vafp.strides[i]);
//			ptr += d->vafp.strides[i]; 
//		}
//	}
//
//	/* blinking dot */
//	pj_gettimeofday(&tv);
//	if (tv.msec < 660) {
//		enum { DOT_SIZE = 8 };
//		pj_uint8_t dot_clr_rgb[3] = {255, 255, 255};
//		pj_uint8_t dot_clr_yuv[3] = {235, 128, 128};
//
//		if (d->vfi->plane_cnt == 1) {
//			for (i = 0; i < 3; ++i) {
//				pj_uint8_t *ptr;
//				unsigned j, k, inc_ptr;
//				pj_size_t dot_size = DOT_SIZE;
//
//				dot_size /= (d->cbfi->c_stride[i] * 8 / d->vfi->bpp);
//				inc_ptr = d->cbfi->c_stride[i];
//				for (j = 0; j < dot_size; ++j) {
//					ptr = p + d->vafp.strides[0]*(dot_size+j+1) - 
//						2*dot_size*inc_ptr + d->cbfi->c_offset[i];
//					for (k = 0; k < dot_size; ++k) {
//						if (d->vfi->color_model == PJMEDIA_COLOR_MODEL_RGB)
//							*ptr = dot_clr_rgb[i];
//						else
//							*ptr = dot_clr_yuv[i];
//						ptr += inc_ptr;
//					}
//				}
//			}
//		} else {
//			pj_size_t offset_p = 0;
//
//			for (i = 0; i < 3; ++i) {
//				pj_uint8_t *ptr, c;
//				unsigned j;
//				pj_size_t dot_size = DOT_SIZE;
//
//				if (d->vfi->color_model == PJMEDIA_COLOR_MODEL_RGB)
//					c = dot_clr_rgb[i];
//				else
//					c = dot_clr_yuv[i];
//
//				dot_size /= (d->vafp.size.w / d->vafp.strides[i]);
//				ptr = p + offset_p + d->vafp.strides[i]*(dot_size+1) - 
//					2*dot_size;
//				for (j = 0; j < dot_size; ++j) {
//					pj_memset(ptr, c, dot_size);
//					ptr += d->vafp.strides[i];
//				}
//				offset_p += d->vafp.plane_bytes[i];
//			}
//		}
//	}
//
//	return PJ_SUCCESS;
//}

/* API: Get frame from stream */
static pj_status_t dcdev_stream_get_frame(pjmedia_vid_dev_stream *strm,
																				 pjmedia_frame *frame)
{
	struct dcdev_stream *stream = (struct dcdev_stream*)strm;

	frame->type = PJMEDIA_FRAME_TYPE_VIDEO;
	frame->bit_info = 0;
	frame->timestamp = stream->ts;
	stream->ts.u64 += stream->ts_inc;
	frame->buf = NULL;
	frame->size = 0;
	
	return PJ_SUCCESS;
}

/* API: Start stream. */
static pj_status_t dcdev_stream_start(pjmedia_vid_dev_stream *strm)
{
	struct dcdev_stream *stream = (struct dcdev_stream*)strm;

	PJ_UNUSED_ARG(stream);

	PJ_LOG(4, (THIS_FILE, "Starting dcdev video stream"));

	return PJ_SUCCESS;
}

/* API: Stop stream. */
static pj_status_t dcdev_stream_stop(pjmedia_vid_dev_stream *strm)
{
	struct dcdev_stream *stream = (struct dcdev_stream*)strm;

	PJ_UNUSED_ARG(stream);

	PJ_LOG(4, (THIS_FILE, "Stopping dcdev video stream"));

	return PJ_SUCCESS;
}


/* API: Destroy stream. */
static pj_status_t dcdev_stream_destroy(pjmedia_vid_dev_stream *strm)
{
	struct dcdev_stream *stream = (struct dcdev_stream*)strm;

	PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

	dcdev_stream_stop(strm);

	pj_pool_release(stream->pool);

	return PJ_SUCCESS;
}

#endif	/* PJMEDIA_VIDEO_DEV_HAS_DCDEV_SRC */
