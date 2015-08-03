// REITEK

/*
    VoiceAge G.729
*/

#include <pjmedia-codec/g729.h>
#include <pjmedia/codec.h>
#include <pjmedia/errno.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/plc.h>
#include <pjmedia/port.h>
#include <pjmedia/silencedet.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/os.h>

#if defined(PJMEDIA_HAS_G729_CODEC) && (PJMEDIA_HAS_G729_CODEC != 0)


#include "../../../VoiceAgeG729/G729full/bas_type.h"
#include "../../../VoiceAgeG729/G729full/codecLib_if.h"

#if defined(_MSC_VER)

//#pragma comment(lib, "CodecLib.lib")
//#pragma comment(lib, "g729a.lib")

#pragma comment(linker, "/NODEFAULTLIB:libcmt.lib")

#endif

#define THIS_FILE   "g729.c"

/* Tracing */
#ifndef PJ_TRACE
#   define PJ_TRACE	0	
#endif

#if PJ_TRACE 
#   define TRACE_(expr)	PJ_LOG(5,expr)
#else
#   define TRACE_(expr)
#endif


/* Prototypes for G729 factory */
static pj_status_t g729_test_alloc( pjmedia_codec_factory *factory, 
				   const pjmedia_codec_info *id );
static pj_status_t g729_default_attr( pjmedia_codec_factory *factory, 
				     const pjmedia_codec_info *id, 
				     pjmedia_codec_param *attr );
static pj_status_t g729_enum_codecs( pjmedia_codec_factory *factory, 
				    unsigned *count, 
				    pjmedia_codec_info codecs[]);
static pj_status_t g729_alloc_codec( pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id, 
				    pjmedia_codec **p_codec);
static pj_status_t g729_dealloc_codec( pjmedia_codec_factory *factory, 
				      pjmedia_codec *codec );

/* Prototypes for G729 implementation. */
static pj_status_t  g729_codec_init( pjmedia_codec *codec, 
				    pj_pool_t *pool );
static pj_status_t  g729_codec_open( pjmedia_codec *codec, 
				    pjmedia_codec_param *attr );
static pj_status_t  g729_codec_close( pjmedia_codec *codec );
static pj_status_t  g729_codec_modify(pjmedia_codec *codec, 
				     const pjmedia_codec_param *attr );
static pj_status_t  g729_codec_parse( pjmedia_codec *codec,
				     void *pkt,
				     pj_size_t pkt_size,
				     const pj_timestamp *ts,
				     unsigned *frame_cnt,
				     pjmedia_frame frames[]);
static pj_status_t  g729_codec_encode( pjmedia_codec *codec, 
				      const struct pjmedia_frame *input,
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);
static pj_status_t  g729_codec_decode( pjmedia_codec *codec, 
				      const struct pjmedia_frame *input,
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);


/* Definition for G729 codec operations. */
static pjmedia_codec_op g729_op = 
{
    &g729_codec_init,
    &g729_codec_open,
    &g729_codec_close,
    &g729_codec_modify,
    &g729_codec_parse,
    &g729_codec_encode,
    &g729_codec_decode,
    NULL
};

/* Definition for G729 codec factory operations. */
static pjmedia_codec_factory_op g729_factory_op =
{
    &g729_test_alloc,
    &g729_default_attr,
    &g729_enum_codecs,
    &g729_alloc_codec,
    &g729_dealloc_codec,
    &pjmedia_codec_g729_deinit
};

/* G729 factory */
static struct g729_codec_factory
{
    pjmedia_codec_factory    base;
    pjmedia_endpt	    *endpt;
    pj_pool_t		    *pool;
    pj_mutex_t		    *mutex;
    pjmedia_codec	     codec_list;
} g729_codec_factory;


/* G729 codec private data. */
struct g729_data
{
    void	*encoder;
    void	*decoder;
    pj_bool_t		 vad_enabled;
    pjmedia_silence_det	*vad;
    pj_timestamp	 last_tx;
    pj_uint8_t       frm_per_pkt;
};



/*
 * Initialize and register G729 codec factory to pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_g729_init( pjmedia_endpt *endpt )
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (g729_codec_factory.pool != NULL)
	    return PJ_SUCCESS;

    /* Create G729 codec factory. */
    g729_codec_factory.base.op = &g729_factory_op;
    g729_codec_factory.base.factory_data = NULL;
    g729_codec_factory.endpt = endpt;

    g729_codec_factory.pool = pjmedia_endpt_create_pool(endpt, "G729", 8000, 
						       8000);
    if (!g729_codec_factory.pool)
	    return PJ_ENOMEM;

    pj_list_init(&g729_codec_factory.codec_list);

    /* Create mutex. */
    status = pj_mutex_create_simple(g729_codec_factory.pool, "G729", 
				    &g729_codec_factory.mutex);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
    if (!codec_mgr)
    {
	    status = PJ_EINVALIDOP;
	    goto on_error;
    }

    /* Register codec factory to endpoint. */
    status = pjmedia_codec_mgr_register_factory(codec_mgr, 
						&g729_codec_factory.base);

    if (status != PJ_SUCCESS)
	    goto on_error;

	PJ_LOG(3, (THIS_FILE, "pjmedia_codec_g729_init: codec factory initialized"));

    /* Done. */
    return PJ_SUCCESS;

on_error:
    pj_pool_release(g729_codec_factory.pool);
    g729_codec_factory.pool = NULL;
    return status;
}



/*
 * Unregister G729 codec factory from pjmedia endpoint and deinitialize
 * the G729 codec library.
 */
PJ_DEF(pj_status_t) pjmedia_codec_g729_deinit(void)
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (g729_codec_factory.pool == NULL)
    	return PJ_SUCCESS;

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(g729_codec_factory.endpt);
    if (!codec_mgr)
    {
	    pj_pool_release(g729_codec_factory.pool);
	    g729_codec_factory.pool = NULL;
	    return PJ_EINVALIDOP;
    }

    /* Unregister G729 codec factory. */
    status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
						  &g729_codec_factory.base);
    
    /* Destroy mutex. */
    pj_mutex_destroy(g729_codec_factory.mutex);

    /* Destroy pool. */
    pj_pool_release(g729_codec_factory.pool);
    g729_codec_factory.pool = NULL;

	PJ_LOG(3, (THIS_FILE, "pjmedia_codec_g729_deinit: codec factory shutdown"));

    return status;
}

/* 
 * Check if factory can allocate the specified codec. 
 */
static pj_status_t g729_test_alloc( pjmedia_codec_factory *factory, 
				   const pjmedia_codec_info *info )
{
    PJ_UNUSED_ARG(factory);

    /* Check payload type. */
    if (info->pt != PJMEDIA_RTP_PT_G729)
	    return PJMEDIA_CODEC_EUNSUP;

    /* Ignore the rest, since it's static payload type. */

    return PJ_SUCCESS;
}

/*
 * Generate default attribute.
 */
static pj_status_t g729_default_attr (pjmedia_codec_factory *factory, 
				      const pjmedia_codec_info *id, 
				      pjmedia_codec_param *attr )
{
    PJ_UNUSED_ARG(factory);
    PJ_UNUSED_ARG(id);

    pj_bzero(attr, sizeof(pjmedia_codec_param));
    attr->info.clock_rate = 8000;
    attr->info.channel_cnt = 1;
    attr->info.avg_bps = 8000;
    attr->info.max_bps = 11800;
    attr->info.pcm_bits_per_sample = 16;
    attr->info.frm_ptime = 10;
    attr->info.pt = PJMEDIA_RTP_PT_G729;

    attr->setting.frm_per_pkt = 2;
    attr->setting.vad = 1;

    /* Default all other flag bits disabled. */

    return PJ_SUCCESS;
}

/*
 * Enum codecs supported by this factory (i.e. only G729!).
 */
static pj_status_t g729_enum_codecs(pjmedia_codec_factory *factory, 
				    unsigned *count, 
				    pjmedia_codec_info codecs[])
{
    PJ_UNUSED_ARG(factory);
    PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);

    pj_bzero(&codecs[0], sizeof(pjmedia_codec_info));
    codecs[0].encoding_name = pj_str("G729");
    codecs[0].pt = PJMEDIA_RTP_PT_G729;
    codecs[0].type = PJMEDIA_TYPE_AUDIO;
    codecs[0].clock_rate = 8000;
    codecs[0].channel_cnt = 1;

    *count = 1;

    return PJ_SUCCESS;
}

/*
 * Allocate a new G729 codec instance.
 */
static pj_status_t g729_alloc_codec( pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id,
				    pjmedia_codec **p_codec)
{
    pjmedia_codec *codec;
    struct g729_data *g729_data;

    PJ_ASSERT_RETURN(factory && id && p_codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &g729_codec_factory.base, PJ_EINVAL);


    pj_mutex_lock(g729_codec_factory.mutex);

    /* Get free nodes, if any. */
    if (!pj_list_empty(&g729_codec_factory.codec_list))
    {
	    codec = g729_codec_factory.codec_list.next;
	    pj_list_erase(codec);
    }
    else
    {
	    pj_status_t status;

	    codec = PJ_POOL_ZALLOC_T(g729_codec_factory.pool, pjmedia_codec);
	    PJ_ASSERT_RETURN(codec != NULL, PJ_ENOMEM);
	    codec->op = &g729_op;
	    codec->factory = factory;

	    g729_data = PJ_POOL_ZALLOC_T(g729_codec_factory.pool, struct g729_data);
	    codec->codec_data = g729_data;

	    /* Create silence detector */
	    status = pjmedia_silence_det_create(g729_codec_factory.pool,
					        8000, 160,
					        &g729_data->vad);
	    if (status != PJ_SUCCESS)
        {
	        pj_mutex_unlock(g729_codec_factory.mutex);

			PJ_LOG(1, (THIS_FILE, "g729_alloc_codec: create silence detector failed (status = %d)", status));

	        return status;
	    }
    }

    pj_mutex_unlock(g729_codec_factory.mutex);

    *p_codec = codec;
    return PJ_SUCCESS;
}

/*
 * Free codec.
 */
static pj_status_t g729_dealloc_codec( pjmedia_codec_factory *factory, 
				      pjmedia_codec *codec )
{
    struct g729_data *g729_data;

    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &g729_codec_factory.base, PJ_EINVAL);

    g729_data = (struct g729_data*) codec->codec_data;

    /* Close codec, if it's not closed. */
    g729_codec_close(codec);

    /* Re-init silence_period */
    pj_set_timestamp32(&g729_data->last_tx, 0, 0);

    /* Put in the free list. */
    pj_mutex_lock(g729_codec_factory.mutex);
    pj_list_push_front(&g729_codec_factory.codec_list, codec);
    pj_mutex_unlock(g729_codec_factory.mutex);

    return PJ_SUCCESS;
}

/*
 * Init codec.
 */
static pj_status_t g729_codec_init( pjmedia_codec *codec, 
				   pj_pool_t *pool )
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}

/*
 * Open codec.
 */
static pj_status_t g729_codec_open( pjmedia_codec *codec, 
				   pjmedia_codec_param *attr )
{
    struct g729_data *g729_data = (struct g729_data*) codec->codec_data;

    pj_assert(g729_data != NULL);
    pj_assert(g729_data->encoder == NULL && g729_data->decoder == NULL);

    codecLibOpen(&g729_data->encoder, G729, ENC, 0);

	if (!g729_data->encoder) {
		PJ_LOG(1, (THIS_FILE, "g729_codec_open: error opening encoder"));

		return PJMEDIA_CODEC_EFAILED;
	}

    codecLibOpen(&g729_data->decoder, G729, DEC, 0);

	if (!g729_data->decoder) {
		PJ_LOG(1, (THIS_FILE, "g729_codec_open: error opening decoder"));

		return PJMEDIA_CODEC_EFAILED;
	}

	g729_data->vad_enabled = (attr->setting.vad != 0);
    g729_data->frm_per_pkt = attr->setting.frm_per_pkt;

	PJ_LOG(3, (THIS_FILE, "g729_codec_open: codec opened: vad=%d, frm_per_pkt=%i", g729_data->vad_enabled, g729_data->frm_per_pkt));

    return PJ_SUCCESS;
}

/*
 * Close codec.
 */
static pj_status_t g729_codec_close( pjmedia_codec *codec )
{
    struct g729_data *g729_data = (struct g729_data*) codec->codec_data;

    pj_assert(g729_data != NULL);

    if (g729_data->encoder) {
        codecLibClose(&g729_data->encoder, 0);
	    g729_data->encoder = NULL;
    }

	if (g729_data->decoder) {
        codecLibClose(&g729_data->decoder, 0);
	    g729_data->decoder = NULL;
    }

	PJ_LOG(3, (THIS_FILE, "g729_codec_close: codec closed"));

    return PJ_SUCCESS;
}


/*
 * Modify codec settings.
 */
static pj_status_t  g729_codec_modify(pjmedia_codec *codec, 
				     const pjmedia_codec_param *attr )
{
    struct g729_data *g729_data = (struct g729_data*) codec->codec_data;

    pj_assert(g729_data != NULL);
    pj_assert(g729_data->encoder != NULL && g729_data->decoder != NULL);

    g729_data->vad_enabled = (attr->setting.vad != 0);
    g729_data->frm_per_pkt = attr->setting.frm_per_pkt;

	PJ_LOG(3, (THIS_FILE, "g729_codec_modify: codec modified: vad=%d, frm_per_pkt=%i", g729_data->vad_enabled, g729_data->frm_per_pkt));

    return PJ_SUCCESS;
}


/*
 * Get frames in the packet.
 */
static pj_status_t  g729_codec_parse( pjmedia_codec *codec,
				     void *pkt,
				     pj_size_t pkt_size,
				     const pj_timestamp *ts,
				     unsigned *frame_cnt,
				     pjmedia_frame frames[])
{
    unsigned count = 0;

    PJ_UNUSED_ARG(codec);

    PJ_ASSERT_RETURN(frame_cnt, PJ_EINVAL);

	TRACE_((THIS_FILE, "g729_codec_parse: input len=%d", pkt_size));

    while (pkt_size >= 10  && count < *frame_cnt) 
    {
	    frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
	    frames[count].buf = pkt;
	    frames[count].size = 10;
	    frames[count].timestamp.u64 = ts->u64 + count * 80;

	    pkt = ((char*)pkt) + 10;
	    pkt_size -= 10;

	    ++count;
    }

	TRACE_((THIS_FILE, "g729_codec_parse: got %d frames", count));

    *frame_cnt = count;
    return PJ_SUCCESS;
}

/*
 * Encode frame.
 */
static pj_status_t g729_codec_encode( pjmedia_codec *codec, 
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len, 
				     struct pjmedia_frame *output)
{
    struct g729_data *g729_data = (struct g729_data*) codec->codec_data;
    pj_int16_t *pcm_in;
    unsigned in_size;

    pj_assert(g729_data && input && output);
    
    pcm_in = (pj_int16_t*)input->buf;
    in_size = input->size;

    PJ_ASSERT_RETURN(in_size % 160 == 0, PJMEDIA_CODEC_EPCMFRMINLEN);
    PJ_ASSERT_RETURN(output_buf_len >= 20 * in_size/160, 
		     PJMEDIA_CODEC_EFRMTOOSHORT);

    /* Detect silence */
    if (g729_data->vad_enabled)
    {
	    pj_bool_t is_silence;
	    pj_int32_t silence_duration;

	    silence_duration = pj_timestamp_diff32(&g729_data->last_tx, 
					           &input->timestamp);

	    is_silence = pjmedia_silence_det_detect(g729_data->vad, 
					            (const pj_int16_t*) input->buf,
						    (input->size >> 1),
						    NULL);
	    if (is_silence &&
	        (PJMEDIA_CODEC_MAX_SILENCE_PERIOD == -1 ||
	         silence_duration < PJMEDIA_CODEC_MAX_SILENCE_PERIOD*8000/1000))
        {
	        output->type = PJMEDIA_FRAME_TYPE_NONE;
	        output->buf = NULL;
	        output->size = 0;
	        output->timestamp = input->timestamp;
	        return PJ_SUCCESS;
	    }
        else
        {
	        g729_data->last_tx = input->timestamp;
	    }
    }

    /* Encode */
    output->size = 0;
    while (in_size >= 80 * (unsigned)(g729_data->frm_per_pkt))
    {
        Word32 src_len = 40 * g729_data->frm_per_pkt;
        Word32 dst_len = 5 * g729_data->frm_per_pkt;

        codecOption option;
        Word32 res = codecLibQuery(G729, ENC, 0, &option, 0);
        if (res!=0) {
			PJ_LOG(1, (THIS_FILE, "g729_codec_encode: error getting encoder"));

			return PJMEDIA_CODEC_EFAILED;
        }

        codecLibEncode(&g729_data->encoder, (Word16*)pcm_in, &src_len, (Word8*)output->buf + output->size, &dst_len, &option, 0);

        pcm_in += 40 * g729_data->frm_per_pkt;
	    output->size += 5 * g729_data->frm_per_pkt;
	    in_size -= 80 * g729_data->frm_per_pkt;
    }

    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;

	TRACE_((THIS_FILE, "g729_codec_encode: size=%d", output->size));

    return PJ_SUCCESS;
}

/*
 * Decode frame.
 */
static pj_status_t g729_codec_decode( pjmedia_codec *codec, 
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len, 
				     struct pjmedia_frame *output)
{
    struct g729_data *g729_data = (struct g729_data*) codec->codec_data;
    codecOption option;
    Word32 res;

    Word32 src_len = 10;
    Word32 dst_len = 80;

    pj_assert(g729_data != NULL);
    PJ_ASSERT_RETURN(input && output, PJ_EINVAL);

	TRACE_((THIS_FILE, "g729_codec_decode: inbuf=%p, insize=%d, outbuf=%p, outsize=%d", input->buf, input->size, output->buf, output_buf_len));

	if (output_buf_len < 80) {
		PJ_LOG(1, (THIS_FILE, "g729_codec_decode: ERROR: PJMEDIA_CODEC_EPCMTOOSHORT"));

		return PJMEDIA_CODEC_EPCMTOOSHORT;
	}

	if (input->size < 10) {
		PJ_LOG(1, (THIS_FILE, "g729_codec_decode: ERROR: PJMEDIA_CODEC_EFRMTOOSHORT"));

		return PJMEDIA_CODEC_EFRMTOOSHORT;
	}

    res = codecLibQuery(G729, DEC, 0, &option, 0);
    if (res!=0) {
		PJ_LOG(1, (THIS_FILE, "g729_codec_decode: error getting decoder"));

        return PJMEDIA_CODEC_EFAILED;
    }

    codecLibDecode(&g729_data->decoder, (Word8*)input->buf, &src_len, (Word16*)output->buf, &dst_len, &option, 0);

    output->size = 80 * 2;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;

	TRACE_((THIS_FILE, "g729_codec_decode: done"));

    return PJ_SUCCESS;
}

#endif	// PJMEDIA_HAS_G729_CODEC
