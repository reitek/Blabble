#ifndef __PJMEDIA_CODEC_G729_H__
#define __PJMEDIA_CODEC_G729_H__

/*
    VoiceAge G.729
*/

/**
 * @file pjmedia-codec/g729.h
 */

#include <pjmedia-codec/types.h>

PJ_BEGIN_DECL


/**
 * Initialize and register G.729 codec factory to pjmedia endpoint.
 *
 * @param endpt	    The pjmedia endpoint.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_g729_init( pjmedia_endpt *endpt );



/**
 * Unregister GSM codec factory from pjmedia endpoint and deinitialize
 * the GSM codec library.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_g729_deinit(void);


PJ_END_DECL


/**
 * @}
 */

#endif	/* __PJMEDIA_CODEC_G729_H__ */
