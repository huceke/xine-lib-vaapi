/*
 * Copyright (C) 2012 the xine project
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
 */

/* Torsten Jager <t.jager@gmx.de>
   The idea is: present a virtual directory filled with images.
   Create on demand in memory what the user actually tries to access. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define LOG_MODULE "input_test"
#define LOG_VERBOSE
/*
#define LOG
*/

#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/compat.h>
#include <xine/input_plugin.h>
#include <xine/video_out.h>

#define TEST_MAX_NAMES 10

typedef struct {
  input_class_t     input_class;
  xine_t           *xine;
  xine_mrl_t       *mrls[TEST_MAX_NAMES + 1], m[TEST_MAX_NAMES];
} test_input_class_t;

typedef struct {
  input_plugin_t    input_plugin;
  xine_stream_t    *stream;

  unsigned char    *buf, *bmp_head, *y4m_head, *y4m_frame;
  off_t             filesize, filepos, headsize, framesize;
  int               width, height, type;
} test_input_plugin_t;

static const char * const test_names[TEST_MAX_NAMES + 1 + 1] = {
  "test://",
  "test://color_circle.bmp",
  "test://rgb_levels.bmp",
  "test://saturation_levels.bmp",
  "test://uv_square.bmp",
  "test://y_resolution.bmp",
  "test://color_circle.y4m",
  "test://rgb_levels.y4m",
  "test://saturation_levels.y4m",
  "test://uv_square.y4m",
  "test://y_resolution.y4m",
  NULL
};

const char * const test_titles[TEST_MAX_NAMES/2] = {
  N_("Color Circle"),
  N_("RGB Levels"),
  N_("Saturation Levels"),
  N_("UV Square"),
  N_("Luminance Resolution"),
};

static const char * const test_cm[] = {
  " ITU-R 470 BG / SDTV",
  " ITU-R 709 / HDTV",
};

/* TJ. the generator code - actually a cut down version of my "testvideo" project */

static void put32le (unsigned int v, unsigned char *p) {
  p[0] = v;
  p[1] = v >> 8;
  p[2] = v >> 16;
  p[3] = v >> 24;
}

/* square root */
static unsigned int isqr (unsigned int v) {
  unsigned int a, b, c, e = 0;
  if (v == 0) return (0);
  c = 0;
  b = v;
  while (b) {b >>= 2; c++;}
  a = 1 << (c - 1);
  c = 1 << c;
  while (a + 1 < c) {
    b = (a + c) >> 1;
    e = b * b;
    if (e <= v) a = b; else c = b;
  }
  return (a + (c * c - v < v - e ? 1 : 0));
}

/* arcus tangens 0..24*255 */
static int iatan (int x, int y) {
  int a, b, v = 0;
  if ((x == 0) && (y == 0)) return (0);
  /* mirror to first half quadrant */
  if (y < 0) {v = -24 * 255; y = -y;}
  if (x < 0) {v = -12 * 255 - v; x = -x;}
  if (x < y) {v = -6 * 255 - v; a = x; b = y;} else {a = y; b = x;}
  /* cubic interpolation within 32 bit */
  v += (1027072 * a / b - 718 * a * a / b * 255 / b
    - 237 * a * a / b * a / b * 255 / b) >> 10;
  return (v < 0 ? -v : v);
}

/* absolute angle difference 0..180*255 */
static int adiff (int a, int b) {
  int d = a > b ? a - b : b - a;
  return (d < 12 * 255 ? d : 24 * 255 - d);
}

static int test_make (test_input_plugin_t * this) {
  int width, height, x, y, cx, cy, d, r, dx, dy, a, red, green, blue, angle = 0;
  int mpeg = 0, hdtv = 0, yuv = 0, gray = 0;
  unsigned char *p, *buf;
  int type = this->type;

  if (this->buf) free (this->buf);
  this->buf = NULL;

  width = 320;
  if (this->stream && this->stream->video_out) {
    x = this->stream->video_out->get_property (this->stream->video_out,
      VO_PROP_WINDOW_WIDTH);
    if (x > width) width = x;
  }
  if (width > 1920) width = 1920;
  width &= ~1;
  height = width * 9 / 16;
  height &= ~1;

  this->width = width;
  this->height = height;

  a = 54 + width * height * 3;
  if (type > TEST_MAX_NAMES / 2) {
    type -= TEST_MAX_NAMES / 2;
    yuv = 1;
    mpeg = 1;
    if (height >= 720) hdtv = 1;
    a += 80 + width * height * 3 / 2;
  }

  buf = malloc (a);
  if (!buf) return (1);

  this->buf = p = buf;
  this->bmp_head = p;
  this->filesize = 54 + width * height * 3;
  if (yuv) {
    p += 54 + width * height * 3;
    this->y4m_head = p;
    this->headsize = sprintf (p,
      "YUV4MPEG2 W%d H%d F25:1 Ip A0:0 C420mpeg2 XYSCSS=420MPEG2\n", width, height);
    p += 74;
    this->y4m_frame = p;
    memcpy (p, "FRAME\n", 6);
    this->framesize = 6 + width * height * 3 / 2;
    this->filesize = this->headsize + 10 * 25 * this->framesize;
  }
  this->filepos = 0;

  p = this->bmp_head;
  memset (p, 0, 54);
  p[0] = 'B';
  p[1] = 'M';
  put32le (54 + width * height * 3, p + 2); /* file size */
  put32le (54, p + 10); /* header size */
  put32le (40, p + 14); /* ?? */
  put32le (width, p + 18);
  put32le (height, p + 22);
  p[26] = 1; /* ?? */
  p[28] = 24; /* depth */
  put32le (width * height * 3, p + 34); /* bitmap size */
  put32le (2835, p + 38); /* ?? */
  put32le (2835, p + 42); /* ?? */
  p += 54;

  switch (type) {

    case 1:
      /* color circle test */
      cx = width >> 1;
      cy = height >> 1;
      r = width < height ? (width * 98) / 100 : (height * 98) / 100;
      for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
          dx = ((x - cx) << 1) + 1;
          dy = ((y - cy) << 1) + 1;
          d = isqr (dx * dx + dy * dy);
          if (d > r) red = green = blue = 128; else {
            a = (iatan (dx, dy) + angle) % (24 * 255);
            red = 8 * 255 - adiff (a, 0);
            red = red < 0 ? 0 : (red > 4 * 255 ? 4 * 255 : red);
            red = red * d / (4 * r);
            green = 8 * 255 - adiff (a, 8 * 255);
            green = green < 0 ? 0 : (green > 4 * 255 ? 4 * 255 : green);
            green = green * d / (4 * r);
            blue = 8 * 255 - adiff (a, 16 * 255);
            blue = blue < 0 ? 0 : (blue > 4 * 255 ? 4 * 255 : blue);
            blue = blue * d / (4 * r);
          }
          *p++ = blue;
          *p++ = green;
          *p++ = red;
        }
      }
    break;

    case 2:
      /* sweep bars */
      dx = (((width + 9) / 18) + 1) & ~1;
      dy = (((height + 10) / 20) + 1) & ~1;
      cx = (width / 2 - 8 * dx) & ~1;
      cy = (height / 2 - 8 * dy) & ~1;
      /* bottom gray */
      d = cy * width * 3;
      memset (p, 127, d);
      p += d;
      /* color bars */
      for (y = 0; y < 16; y++) {
        /* make 1 line */
        unsigned char *q = p;
        for (x = 0; x < width; x++) {
          d = x - cx;
          if ((d < 0) || (d >= 16 * dx)) red = green = blue = 127;
          else {
            a = (y + 1) & 2 ? 17 * (15 - d / dx) : 255 - 16 * d / dx;
            red = y & 4 ? a : 0;
            green = y & 8 ? a : 0;
            blue = y & 2 ? a : 0;
          }
          *p++ = blue;
          *p++ = green;
          *p++ = red;
        }
        /* duplicate it further */
        for (d = 1; d < dy; d++) {
          memcpy (p, q, width * 3);
          p += width * 3;
        }
      }
      /* top gray */
      memset (p, 127, (height - cy - 16 * dy) * width * 3);
    break;

    case 3: {
      /* sweep bars, saturation */
      int g[] = {0, 29, 76, 105, 150, 179, 226, 255, 0, 18, 54, 73, 182, 201, 237, 255};
      dx = (((width + 9) / 18) + 1) & ~1;
      dy = (((height + 10) / 20) + 1) & ~1;
      cx = (width / 2 - 8 * dx) & ~1;
      cy = (height / 2 - 8 * dy) & ~1;
      /* bottom gray */
      d = cy * width * 3;
      memset (p, 127, d);
      p += d;
      /* color bars */
      for (y = 0; y < 16; y++) {
        /* make 1 line */
        unsigned char *q = p;
        for (x = 0; x < width; x++) {
          d = x - cx;
          if ((d < 0) || (d >= 16 * dx)) red = green = blue = 127;
          else {
            a = (y + 1) & 2 ? 17 * (15 - d / dx) : 255 - 16 * d / dx;
            r = (255 - a) * g[y / 2 + 8 * hdtv];
            red = ((y & 4 ? 255 : 0) * a + r) / 255;
            green = ((y & 8 ? 255 : 0) * a + r) / 255;
            blue = ((y & 2 ? 255 : 0) * a + r) / 255;
          }
          *p++ = blue;
          *p++ = green;
          *p++ = red;
        }
        /* duplicate it further */
        for (d = 1; d < dy; d++) {
          memcpy (p, q, width * 3);
          p += width * 3;
        }
      }
      /* top gray */
      memset (p, 127, (height - cy - 16 * dy) * width * 3);
    } break;

    case 4: {
      /* UV square */
      int m1 = hdtv ?  51603 :  45941;
      int m2 = hdtv ?  -6138 : -11277;
      int m3 = hdtv ? -15339 : -23401;
      int m4 = hdtv ?  60804 :  58065;
      r = width < height ? width : height;
      r = (49 << 9) * r / 100;
      for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
          int min, max, u, v;
          u = (x << 1) - width + 1;
          v = (y << 1) - height + 1;
          min = max = red = m1 * v;
          green = m2 * u + m3 * v;
          if (green < min) min = green; else if (green > max) max = green;
          blue = m4 * u;
          if (blue < min) min = blue; else if (blue > max) max = blue;
          d = (256 * r + (r >> 1)) + min - max - 1;
          if (d < 0) red = green = blue = 127;
          else {
            if (gray == 255) min -= d - (r >> 1);
            else min -= d / 255 * gray - (r >> 1);
            red = (red - min) / r;
            green = (green - min) / r;
            blue = (blue - min) / r;
          }
          *p++ = blue;
          *p++ = green;
          *p++ = red;
        }
      }
    } break;

    case 5:
      /* resolution pattern */
      dx = (width / 10);
      dy = (height / 7);
      for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
          if ((x < dx) || (x >= 9 * dx)) red = 127;
          else {
            d = x / dx;
            switch (y / dy) {
              case 1: red = ((x / d) ^ (y / d)) & 1 ? 0 : 255; break;
              case 3: red = (x / d) & 1 ? 0 : 255; break;
              case 5: red = (y / d) & 1 ? 0 : 255; break;
              default: red = 127;
            }
          }
          *p++ = red;
          *p++ = red;
          *p++ = red;
        }
      }
    break;
  }

  if (yuv) {
    int fb, fr, yb, yr, yg, yo, ubvr, vb, ur, ug, vg;
    int i, _yb[256], _yr[256], _yg[256];
    int _ubvr[1024], _vb[1024], _ur[1024], _ug[1024], _vg[1024];
    unsigned char *p2, *q, *q2;
    #define SSHIFT 17
    #define SFACTOR (1 << SSHIFT)
    fb = hdtv ? 722 : 1140;
    fr = hdtv ? 2126 : 2990;
    if (mpeg) {
      yg = (SFACTOR * 219 + 127) / 255;
      yo = SFACTOR * 16 + SFACTOR / 2;
      ubvr = (SFACTOR * 112 + 127) / 255;
    } else {
      yg = SFACTOR;
      yo = SFACTOR / 2;
      ubvr = (SFACTOR * 127 + 127) / 255;
    }
    yb = (yg * fb + 5000) / 10000;
    yr = (yg * fr + 5000) / 10000;
    yg -= yb + yr;
    for (i = 0; i < 256; i++) {
      _yb[i] = yb * i;
      _yr[i] = yr * i;
      _yg[i] = yg * i + yo;
    }
    ur = (ubvr * fr + fb / 2 - 5000) / (fb - 10000);
    ug = -ur - ubvr;
    vb = (ubvr * fb + fr / 2 - 5000) / (fr - 10000);
    vg = -vb - ubvr;
    for (i = 0; i < 1024; i++) {
      _ubvr[i] = ubvr * i + 4 * (SFACTOR * 128 + SFACTOR / 2);
      _ur[i] = ur * i;
      _ug[i] = ug * i;
      _vb[i] = vb * i;
      _vg[i] = vg * i;
    }
    p = this->bmp_head + 54 + width * height * 3;
    q = this->y4m_frame + 6;
    for (y = height - 1; y >= 0; y--) {
      p = buf + 54 + y * width * 3;
      for (x = width; x; x--) {
        *q++ = (_yb[p[0]] + _yg[p[1]] + _yr[p[2]]) >> SSHIFT;
        p += 3;
      }
    }
    q2 = q + width * height / 4;
    for (y = height - 2; y >= 0; y -= 2) {
      p = this->bmp_head + 54 + 3 * y * width;
      p2 = p + 3 * width;
      for (x = width / 2; x; x--) {
        blue = (unsigned int)*p++ + *p2++;
        green = (unsigned int)*p++ + *p2++;
        red = (unsigned int)*p++ + *p2++;
        blue += (unsigned int)*p++ + *p2++;
        green += (unsigned int)*p++ + *p2++;
        red += (unsigned int)*p++ + *p2++;
        a = (_ubvr[blue] + _ug[green] + _ur[red]) >> (SSHIFT + 2);
        *q++ = a > 255 ? 255 : a;
        a = (_ubvr[red] + _vg[green] + _vb[blue]) >> (SSHIFT + 2);
        *q2++ = a > 255 ? 255 : a;
      }
    }
  }

  /* human-friendly title */
  if (type > 0 && type <= TEST_MAX_NAMES / 2) {
    char *title = _x_asprintf("%s (%s)%s",
                              _(test_titles[type-1]),
                              yuv ? "YUV" : "RGB",
                              yuv ? test_cm[!!hdtv] : "");
    _x_meta_info_set(this->stream, XINE_META_INFO_TITLE, title);
    free(title);
  }

  return (1);
}

/* instance functions */

static uint32_t test_plugin_get_capabilities (input_plugin_t *this_gen) {
  return INPUT_CAP_SEEKABLE;
}

static off_t test_plugin_read (input_plugin_t *this_gen, void *buf, off_t len) {
  test_input_plugin_t *this = (test_input_plugin_t *) this_gen;

  if (!this->buf || (len < 0) || !buf) return -1;
  if (len > this->filesize - this->filepos) len = this->filesize - this->filepos;

  if (this->type > TEST_MAX_NAMES / 2) {
    char *p = this->y4m_frame, *q = buf;
    off_t l = len, d;
    d = this->headsize - this->filepos;
    if (d > 0) {
      xine_fast_memcpy (q, this->y4m_head + this->filepos, d);
      q += d;
      this->filepos += d;
      l -= d;
      d = this->framesize;
    } else {
      d = (this->filepos - this->headsize) % this->framesize;
      p += d;
      d = this->framesize - d;
    }
    while (l > 0) {
      if (d > l) d = l;
      xine_fast_memcpy (q, p, d);
      p = this->y4m_frame;
      q += d;
      this->filepos += d;
      l -= d;
      d = this->framesize;
    }
  } else {
    xine_fast_memcpy (buf, this->bmp_head + this->filepos, len);
    this->filepos += len;
  }
  return len;
}

static buf_element_t *test_plugin_read_block (input_plugin_t *this_gen, fifo_buffer_t *fifo,
  off_t todo) {
  test_input_plugin_t *this = (test_input_plugin_t *) this_gen;
  buf_element_t *buf;

  if (!this->buf || (todo < 0)) return NULL;

  buf = fifo->buffer_pool_alloc (fifo);
  if (todo > buf->max_size) todo = buf->max_size;
  buf->type = BUF_DEMUX_BLOCK;
  test_plugin_read (this_gen, buf->content, todo);

  return buf;
}

static off_t test_plugin_seek (input_plugin_t *this_gen, off_t offset, int origin) {
  test_input_plugin_t *this = (test_input_plugin_t *) this_gen;
  off_t newpos = offset;

  switch (origin) {
    case SEEK_SET: break;
    case SEEK_CUR: newpos += this->filepos; break;
    case SEEK_END: newpos += this->filesize; break;
    default: newpos = -1;
  }

  if ((newpos < 0) || (newpos > this->filesize)) {
    errno = EINVAL;
    return (off_t)-1;
  }

  this->filepos = newpos;
  return newpos;
}

static off_t test_plugin_get_current_pos (input_plugin_t *this_gen) {
  test_input_plugin_t *this = (test_input_plugin_t *) this_gen;

  return this->filepos;
}

static off_t test_plugin_get_length (input_plugin_t *this_gen) {
  test_input_plugin_t *this = (test_input_plugin_t *) this_gen;

  return this->filesize;
}

static uint32_t test_plugin_get_blocksize (input_plugin_t *this_gen) {
  return 0;
}

static const char *test_plugin_get_mrl (input_plugin_t *this_gen) {
  test_input_plugin_t *this = (test_input_plugin_t *) this_gen;

  return test_names[this->type];
}

static int test_plugin_get_optional_data (input_plugin_t *this_gen, void *data,
  int data_type) {

  return INPUT_OPTIONAL_UNSUPPORTED;
}

static void test_plugin_dispose (input_plugin_t *this_gen ) {
  test_input_plugin_t *this = (test_input_plugin_t *) this_gen;

  if (this->buf) free (this->buf);
  free (this);
}

static int test_plugin_open (input_plugin_t *this_gen ) {
  test_input_plugin_t *this = (test_input_plugin_t *) this_gen;

  return test_make (this);
}

static input_plugin_t *test_class_get_instance (input_class_t *cls_gen,
  xine_stream_t *stream, const char *data) {
  test_input_plugin_t *this;
  int i;

  for (i = 0; i < TEST_MAX_NAMES + 1; i++) {
    if (!strcasecmp (data, test_names[i])) break;
  }
  if (i == TEST_MAX_NAMES + 1) return NULL;
  if (i == 0) i = 2;

  this = (test_input_plugin_t *) calloc(1, sizeof (test_input_plugin_t));
  this->stream = stream;
  this->type = i;

  this->input_plugin.open               = test_plugin_open;
  this->input_plugin.get_capabilities   = test_plugin_get_capabilities;
  this->input_plugin.read               = test_plugin_read;
  this->input_plugin.read_block         = test_plugin_read_block;
  this->input_plugin.seek               = test_plugin_seek;
  this->input_plugin.get_current_pos    = test_plugin_get_current_pos;
  this->input_plugin.get_length         = test_plugin_get_length;
  this->input_plugin.get_blocksize      = test_plugin_get_blocksize;
  this->input_plugin.get_mrl            = test_plugin_get_mrl;
  this->input_plugin.get_optional_data  = test_plugin_get_optional_data;
  this->input_plugin.dispose            = test_plugin_dispose;
  this->input_plugin.input_class        = cls_gen;

  return &this->input_plugin;
}


/*
 * plugin class functions
 */

static const char * const *test_class_get_autoplay_list (input_class_t *this_gen, int *num_files)
{
  *num_files = sizeof(test_names) / sizeof(test_names[0]) - 2;

  return test_names + 1;
}

static xine_mrl_t **test_class_get_dir (input_class_t *this_gen, const char *filename,
  int *nFiles) {
  test_input_class_t *this = (test_input_class_t *) this_gen;
  int i;
  xine_mrl_t *m;

  for (i = 0; i < TEST_MAX_NAMES; i++) {
    m = &this->m[i];
    this->mrls[i] = m;

    m->origin = test_names[0];
    m->mrl = test_names[i + 1];
    m->link = NULL;
    m->type = mrl_file | mrl_file_normal;
    m->size = 54 + 1024 * 576 * 3;
  }

  *nFiles = i;
  this->mrls[i] = NULL;

  return this->mrls;
}

static void test_class_dispose (input_class_t *this_gen) {
  test_input_class_t *this = (test_input_class_t *) this_gen;

  free (this);
}

static void *init_plugin (xine_t *xine, void *data) {
  test_input_class_t *this;

  this = (test_input_class_t *) calloc(1, sizeof (test_input_class_t));

  this->xine   = xine;

  this->input_class.get_instance       = test_class_get_instance;
  this->input_class.identifier         = "test";
  this->input_class.description        = N_("test card input plugin");
  this->input_class.get_dir            = test_class_get_dir;
  this->input_class.get_autoplay_list  = test_class_get_autoplay_list;
  this->input_class.dispose            = test_class_dispose;
  this->input_class.eject_media        = NULL;

  return this;
}

/*
 * exported plugin catalog entry
 */

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_INPUT | PLUGIN_MUST_PRELOAD, 18, "test", XINE_VERSION_CODE, NULL, init_plugin },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
