/*
 * Copyright (C) 2008 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 * Common acceleration definitions for vdpau
 *
 *
 */

#ifndef HAVE_XINE_ACCEL_VAAPI_H
#define HAVE_XINE_ACCEL_VAAPI_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <va/va_x11.h>
#include <pthread.h>
#ifdef HAVE_FFMPEG_AVUTIL_H
#  include <avcodec.h>
#else
#  include <libavcodec/avcodec.h>
#endif

#include <libswscale/swscale.h>

#if LIBAVCODEC_VERSION_MAJOR >= 53 || (LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR >= 32)
#  define AVVIDEO 2
#else
#  define AVVIDEO 1
#  define pp_context	pp_context_t
#  define pp_mode	pp_mode_t
#endif

#define NUM_OUTPUT_SURFACES 22

#define SURFACE_FREE            0
#define SURFACE_ALOC            1
#define SURFACE_RELEASE         2
#define SURFACE_RENDER          3
#define SURFACE_RENDER_RELEASE  5

struct vaapi_equalizer {
  VADisplayAttribute brightness;
  VADisplayAttribute contrast;
  VADisplayAttribute hue;
  VADisplayAttribute saturation;
};

typedef struct ff_vaapi_context_s ff_vaapi_context_t;

struct ff_vaapi_context_s {
  VADisplay         va_display;
  VAContextID       va_context_id;
  VAConfigID        va_config_id;
  int               width;
  int               height;
  int               sw_width;
  int               sw_height;
  int               va_profile;
  unsigned int      va_colorspace;
  VAImage           va_subpic_image;
  VASubpictureID    va_subpic_id;
  int               va_subpic_width;
  int               va_subpic_height;
  int               is_bound;
  unsigned int      soft_head;
  unsigned int      valid_context;
  vo_driver_t       *driver;
  unsigned int      last_sub_image_fmt;
  struct SwsContext *convert_ctx;
  struct vaapi_equalizer va_equalizer;
};

typedef struct {
  vo_frame_t                *vo_frame;
  VASurfaceID               va_surface_id;
  VASurfaceID               va_soft_surface_id;
  VAImage                   *va_soft_image;

  VAStatus (*vaapi_init)(vo_frame_t *frame_gen, int va_profile, int width, int height, int softrender);
  int (*profile_from_imgfmt)(vo_frame_t *frame_gen, enum PixelFormat pix_fmt, int codec_id, int vaapi_mpeg_sofdec);
  ff_vaapi_context_t *(*get_context)(vo_frame_t *frame_gen);
} vaapi_accel_t;

#ifdef __cplusplus
}
#endif

#endif

