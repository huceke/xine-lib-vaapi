/*
 * Copyright (C) 2000-2004, 2008 the xine project
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
 * video_out_vaapi.c, VAAPI video extension interface for xine
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include <sys/types.h>
#if defined(__FreeBSD__)
#include <machine/param.h>
#endif
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <time.h>
#include <unistd.h>
#include "yuv2rgb.h"

#define LOG_MODULE "video_out_vaapi"
#define LOG_VERBOSE
/*
#define LOG
*/
/*
#define DEBUG_SURFACE
*/
#include "xine.h"
#include <xine/video_out.h>
#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/vo_scale.h>

#include <GL/glu.h>
#include <GL/glx.h>
#include <GL/glext.h>
#include <GL/gl.h>
#include <dlfcn.h>

#include <va/va_x11.h>
#include <va/va_glx.h>

#include "accel_vaapi.h"

#define  RENDER_SURFACES  21
#define  OUTPUT_SURFACES  2

#define IMGFMT_VAAPI               0x56410000 /* 'VA'00 */
#define IMGFMT_VAAPI_MASK          0xFFFF0000
#define IMGFMT_IS_VAAPI(fmt)       (((fmt) & IMGFMT_VAAPI_MASK) == IMGFMT_VAAPI)
#define IMGFMT_VAAPI_CODEC_MASK    0x000000F0
#define IMGFMT_VAAPI_CODEC(fmt)    ((fmt) & IMGFMT_VAAPI_CODEC_MASK)
#define IMGFMT_VAAPI_CODEC_MPEG2   (0x10)
#define IMGFMT_VAAPI_CODEC_MPEG4   (0x20)
#define IMGFMT_VAAPI_CODEC_H264    (0x30)
#define IMGFMT_VAAPI_CODEC_VC1     (0x40)
#define IMGFMT_VAAPI_MPEG2         (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_MPEG2)
#define IMGFMT_VAAPI_MPEG2_IDCT    (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_MPEG2|1)
#define IMGFMT_VAAPI_MPEG2_MOCO    (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_MPEG2|2)
#define IMGFMT_VAAPI_MPEG4         (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_MPEG4)
#define IMGFMT_VAAPI_H263          (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_MPEG4|1)
#define IMGFMT_VAAPI_H264          (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_H264)
#define IMGFMT_VAAPI_VC1           (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_VC1)
#define IMGFMT_VAAPI_WMV3          (IMGFMT_VAAPI|IMGFMT_VAAPI_CODEC_VC1|1)

#define FOVY     60.0f
#define ASPECT   1.0f
#define Z_NEAR   0.1f
#define Z_FAR    100.0f
#define Z_CAMERA 0.869f

#ifndef GLAPIENTRY
#ifdef APIENTRY
#define GLAPIENTRY APIENTRY
#else
#define GLAPIENTRY
#endif
#endif

#define RECT_IS_EQ(a, b) ((a).x1 == (b).x1 && (a).y1 == (b).y1 && (a).x2 == (b).x2 && (a).y2 == (b).y2)

typedef struct vaapi_driver_s vaapi_driver_t;

typedef struct {
    int x0, y0;
    int x1, y1, x2, y2;
} vaapi_rect_t;

typedef struct {
  vo_frame_t         vo_frame;

  int                width, height, format, flags;
  double             ratio;

  vaapi_accel_t     vaapi_accel_data;
} vaapi_frame_t;

struct vaapi_driver_s {

  vo_driver_t        vo_driver;

  config_values_t   *config;

  /* X11 related stuff */
  Display            *display;
  int                 screen;
  Drawable            drawable;
  GC                  gc;
  XColor              black;

  uint32_t            capabilities;

  int ovl_changed;
  vo_overlay_t       *overlays[XINE_VORAW_MAX_OVL];
  uint32_t           *overlay_bitmap;
  int                 overlay_bitmap_size;
  uint32_t            overlay_bitmap_width;
  uint32_t            overlay_bitmap_height;
  vaapi_rect_t        overlay_bitmap_src;
  vaapi_rect_t        overlay_bitmap_dst;

  uint32_t            vdr_osd_width;
  uint32_t            vdr_osd_height;

  uint32_t            overlay_output_width;
  uint32_t            overlay_output_height;
  vaapi_rect_t        overlay_dirty_rect;
  int                 has_overlay;

  uint32_t            overlay_unscaled_width;
  uint32_t            overlay_unscaled_height;
  vaapi_rect_t        overlay_unscaled_dirty_rect;

  int                 osd_displayed;

  yuv2rgb_factory_t  *yuv2rgb_factory;
  yuv2rgb_t          *ovl_yuv2rgb;

  /*
  vaapi_frame_t      *recent_frames[VO_NUM_RECENT_FRAMES];
  vaapi_frame_t      *cur_frame;
  */

  /* all scaling information goes here */
  vo_scale_t          sc;

  xine_t             *xine;

  int                 zoom_x;
  int                 zoom_y;

  int                 init_vaapi;
  unsigned int        deinterlace;
  int                 reinit;
  
  int                 valid_context;
  int                 opengl_render;
  int                 opengl_init;
  int                 opengl_use_tfp;
  int                 query_va_status;

  GLuint              gl_texture;
  GLXContext          gl_context;
  XVisualInfo         *gl_vinfo;
  Pixmap              gl_pixmap;
  Pixmap              gl_image_pixmap;

  ff_vaapi_context_t  *va_context;

  int                  num_frame_buffers;
  vaapi_frame_t       *frames[RENDER_SURFACES];
};

VASurfaceID         *va_surface_ids = NULL;
VASurfaceID         *va_output_surface_ids = NULL;
VAImage             *va_output_image = NULL;

static void vaapi_destroy_subpicture(vo_driver_t *this_gen);
static void destroy_image(vo_driver_t *this_gen, VAImage *va_image);
static int vaapi_ovl_associate(vo_driver_t *this_gen, int bShow);

void (GLAPIENTRY *mpglGenTextures)(GLsizei, GLuint *);
void (GLAPIENTRY *mpglBindTexture)(GLenum, GLuint);
void (GLAPIENTRY *mpglXBindTexImage)(Display *, GLXDrawable, int, const int *);
void (GLAPIENTRY *mpglXReleaseTexImage)(Display *, GLXDrawable, int);
GLXPixmap (GLAPIENTRY *mpglXCreatePixmap)(Display *, GLXFBConfig, Pixmap, const int *);
void (GLAPIENTRY *mpglXDestroyPixmap)(Display *, GLXPixmap);
const GLubyte *(GLAPIENTRY *mpglGetString)(GLenum);
void (GLAPIENTRY *mpglGenPrograms)(GLsizei, GLuint *);

static VADisplay vaapi_get_display(Display *display, int opengl_render)
{
  VADisplay ret;

  if(opengl_render) {
    ret = vaGetDisplayGLX(display);
  } else {
    ret = vaGetDisplay(display);
  }

  if(vaDisplayIsValid(ret))
    return ret;
  else
    return 0;
}

static int vaapi_check_status(vo_driver_t *this_gen, VAStatus vaStatus, const char *msg)
{

  vaapi_driver_t *this = (vaapi_driver_t *) this_gen;

  if (vaStatus != VA_STATUS_SUCCESS) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " Error : %s: %s\n", msg, vaErrorStr(vaStatus));
    return 0;
  }
  return 1;

}

typedef struct {
  void *funcptr;
  const char *extstr;
  const char *funcnames[7];
  void *fallback;
} extfunc_desc_t;

#define DEF_FUNC_DESC(name) {&mpgl##name, NULL, {"gl"#name, NULL}, gl ##name}
static const extfunc_desc_t extfuncs[] = {
  DEF_FUNC_DESC(GenTextures),

  {&mpglBindTexture, NULL, {"glBindTexture", "glBindTextureARB", "glBindTextureEXT", NULL}},
  {&mpglXBindTexImage, "GLX_EXT_texture_from_pixmap", {"glXBindTexImageEXT", NULL}},
  {&mpglXReleaseTexImage, "GLX_EXT_texture_from_pixmap", {"glXReleaseTexImageEXT", NULL}},
  {&mpglXCreatePixmap, "GLX_EXT_texture_from_pixmap", {"glXCreatePixmap", NULL}},
  {&mpglXDestroyPixmap, "GLX_EXT_texture_from_pixmap", {"glXDestroyPixmap", NULL}},
  {&mpglGenPrograms, "_program", {"glGenProgramsARB", NULL}},
  {NULL}
};

typedef struct {
  video_driver_class_t driver_class;

  config_values_t     *config;
  xine_t              *xine;
} vaapi_class_t;

static int gl_visual_attr[] = {
  GLX_RGBA,
  GLX_RED_SIZE, 1,
  GLX_GREEN_SIZE, 1,
  GLX_BLUE_SIZE, 1,
  GLX_DOUBLEBUFFER,
  GL_NONE
};

/* X11 Error handler and error functions */
static int vaapi_x11_error_code = 0;
static int (*vaapi_x11_old_error_handler)(Display *, XErrorEvent *);

static int vaapi_x11_error_handler(Display *dpy, XErrorEvent *error)
{
    vaapi_x11_error_code = error->error_code;
    return 0;
}

static void vaapi_x11_trap_errors(void)
{
    vaapi_x11_error_code    = 0;
    vaapi_x11_old_error_handler = XSetErrorHandler(vaapi_x11_error_handler);
}

static int vaapi_x11_untrap_errors(void)
{
    XSetErrorHandler(vaapi_x11_old_error_handler);
    return vaapi_x11_error_code;
}

static void vaapi_appendstr(char **dst, const char *str)
{
    int newsize;
    char *newstr;
    if (!str)
        return;
    newsize = strlen(*dst) + 1 + strlen(str) + 1;
    newstr = realloc(*dst, newsize);
    if (!newstr)
        return;
    *dst = newstr;
    strcat(*dst, " ");
    strcat(*dst, str);
}

/* Return the address of a linked function */
static void *vaapi_getdladdr (const char *s) {
  void *ret = NULL;
  void *handle = dlopen(NULL, RTLD_LAZY);
  if (!handle)
    return NULL;
  ret = dlsym(handle, s);
  dlclose(handle);

  return ret;
}

/* Resolve opengl functions. */
static void vaapi_get_functions(vo_driver_t *this_gen, void *(*getProcAddress)(const GLubyte *),
                         const char *ext2) {
  vaapi_driver_t *this = (vaapi_driver_t *) this_gen;

  const extfunc_desc_t *dsc;
  const char *extensions;
  char *allexts;

  if (!getProcAddress)
    getProcAddress = (void *)vaapi_getdladdr;

  /* special case, we need glGetString before starting to find the other functions */
  mpglGetString = getProcAddress("glGetString");
  if (!mpglGetString)
      mpglGetString = glGetString;

  extensions = (const char *)mpglGetString(GL_EXTENSIONS);
  if (!extensions) extensions = "";
  if (!ext2) ext2 = "";
  allexts = malloc(strlen(extensions) + strlen(ext2) + 2);
  strcpy(allexts, extensions);
  strcat(allexts, " ");
  strcat(allexts, ext2);
  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " OpenGL extensions string:\n%s\n", allexts);
  for (dsc = extfuncs; dsc->funcptr; dsc++) {
    void *ptr = NULL;
    int i;
    if (!dsc->extstr || strstr(allexts, dsc->extstr)) {
      for (i = 0; !ptr && dsc->funcnames[i]; i++)
        ptr = getProcAddress((const GLubyte *)dsc->funcnames[i]);
    }
    if (!ptr)
        ptr = dsc->fallback;
    *(void **)dsc->funcptr = ptr;
  }
  printf("\n");
  free(allexts);
}

/* Check if opengl indirect/software rendering is used */
static int vaapi_opengl_verify_direct (x11_visual_t *vis) {
  Window        root, win;
  XVisualInfo  *visinfo;
  GLXContext    ctx;
  XSetWindowAttributes xattr;
  int           ret = 0;

  if (!vis || !vis->display || ! (root = RootWindow (vis->display, vis->screen))) {
    fprintf (stderr, "vo_vaapi: Don't have a root window to verify\n");
    return 0;
  }

  if (! (visinfo = glXChooseVisual (vis->display, vis->screen, gl_visual_attr)))
    return 0;

  if (! (ctx = glXCreateContext (vis->display, visinfo, NULL, 1)))
    return 0;

  memset (&xattr, 0, sizeof (xattr));
  xattr.colormap = XCreateColormap(vis->display, root, visinfo->visual, AllocNone);
  xattr.event_mask = StructureNotifyMask | ExposureMask;

  if ( (win = XCreateWindow (vis->display, root, 0, 0, 1, 1, 0, visinfo->depth,
			                       InputOutput, visinfo->visual,
			                       CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
			                       &xattr))) {
    if (glXMakeCurrent (vis->display, win, ctx)) {
	    const char *renderer = (const char *) glGetString(GL_RENDERER);
	    if (glXIsDirect (vis->display, ctx) &&
	                ! strstr (renderer, "Software") &&
	                ! strstr (renderer, "Indirect"))
	      ret = 1;
	      glXMakeCurrent (vis->display, None, NULL);
      }
      XDestroyWindow (vis->display, win);
  }
  glXDestroyContext (vis->display, ctx);
  XFreeColormap     (vis->display, xattr.colormap);

  return ret;
}

static int vaapi_glx_bind_texture(vo_driver_t *this_gen)
{
  vaapi_driver_t *this = (vaapi_driver_t *) this_gen;

  glEnable(GL_TEXTURE_2D);
  mpglBindTexture(GL_TEXTURE_2D, this->gl_texture);

  if (this->opengl_use_tfp) {
    vaapi_x11_trap_errors();
   mpglXBindTexImage(this->display, this->gl_pixmap, GLX_FRONT_LEFT_EXT, NULL);
    XSync(this->display, False);
    if (vaapi_x11_untrap_errors())
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_glx_bind_texture : Update bind_tex_image failed\n");
  }

  return 0;
}

static int vaapi_glx_unbind_texture(vo_driver_t *this_gen)
{
  vaapi_driver_t *this = (vaapi_driver_t *) this_gen;

  if (this->opengl_use_tfp) {
    vaapi_x11_trap_errors();
    mpglXReleaseTexImage(this->display, this->gl_pixmap, GLX_FRONT_LEFT_EXT);
    if (vaapi_x11_untrap_errors())
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_glx_unbind_texture : Failed to release?\n");
  }

  mpglBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_TEXTURE_2D);
  return 0;
}

static void vaapi_glx_render_frame(vo_frame_t *frame_gen, int left, int top, int right, int bottom)
{
  vaapi_driver_t        *this = (vaapi_driver_t *) frame_gen->driver;
  vaapi_frame_t         *frame = (vaapi_frame_t *) frame_gen;
  ff_vaapi_context_t    *va_context = this->va_context;
  int             x1, x2, y1, y2;
  float           tx, ty;

  if (vaapi_glx_bind_texture(frame_gen->driver) < 0)
    return;

  /* Calc texture/rectangle coords */
  x1 = this->sc.output_xoffset;
  y1 = this->sc.output_yoffset;
  x2 = x1 + this->sc.output_width;
  y2 = y1 + this->sc.output_height;
  tx = (float) frame->width  / va_context->width;
  ty = (float) frame->height / va_context->height;

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  /* Draw quad */
  glBegin (GL_QUADS);

    glTexCoord2f (tx, ty);   glVertex2i (x2, y2);
    glTexCoord2f (0,  ty);   glVertex2i (x1, y2);
    glTexCoord2f (0,  0);    glVertex2i (x1, y1);
    glTexCoord2f (tx, 0);    glVertex2i (x2, y1);
    lprintf("render_frame left %d top %d right %d bottom %d\n", x1, y1, x2, y2);

  glEnd ();

  if (vaapi_glx_unbind_texture(frame_gen->driver) < 0)
    return;
}

static void vaapi_glx_flip_page(vo_frame_t *frame_gen, int left, int top, int right, int bottom)
{
  vaapi_driver_t *this = (vaapi_driver_t *) frame_gen->driver;

  glClear(GL_COLOR_BUFFER_BIT);

  vaapi_glx_render_frame(frame_gen, left, top, right, bottom);

  //if (gl_finish)
  //  glFinish();

  glXSwapBuffers(this->display, this->drawable);

}

static void destroy_glx(vo_driver_t *this_gen)
{
  vaapi_driver_t *this = (vaapi_driver_t *) this_gen;

  if(!this->opengl_render)
    return;

  //if (gl_finish)
  //  glFinish();

  vaapi_x11_trap_errors();
  if(this->gl_pixmap)
    mpglXDestroyPixmap(this->display, this->gl_pixmap);
  this->gl_pixmap = None;
  XSync(this->display, False);
  vaapi_x11_untrap_errors();

  if(this->gl_image_pixmap)
    XFreePixmap(this->display, this->gl_image_pixmap);
  this->gl_image_pixmap = None;

  if(this->gl_texture)
    glDeleteTextures(1, &this->gl_texture);
  this->gl_texture = GL_NONE;

  if(this->gl_vinfo)
    XFree(this->gl_vinfo);
  this->gl_vinfo = NULL;

  if(this->gl_context) {
    glXMakeCurrent(this->display, None, NULL);
    glXDestroyContext(this->display, this->gl_context);
    this->gl_context = 0;
  }

  this->opengl_init = 0;

}

static GLXFBConfig *get_fbconfig_for_depth(vo_driver_t *this_gen, int depth)
{
  vaapi_driver_t *this = (vaapi_driver_t *) this_gen;

    GLXFBConfig *fbconfigs, *ret = NULL;
    int          n_elements, i, found;
    int          db, stencil, alpha, rgba, value;

    static GLXFBConfig *cached_config = NULL;
    static int          have_cached_config = 0;

    if (have_cached_config)
        return cached_config;

    fbconfigs = glXGetFBConfigs(this->display, this->screen, &n_elements);

    db      = SHRT_MAX;
    stencil = SHRT_MAX;
    rgba    = 0;

    found = n_elements;

    for (i = 0; i < n_elements; i++) {
        XVisualInfo *vi;
        int          visual_depth;

        vi = glXGetVisualFromFBConfig(this->display, fbconfigs[i]);
        if (!vi)
            continue;

        visual_depth = vi->depth;
        XFree(vi);

        if (visual_depth != depth)
            continue;

        glXGetFBConfigAttrib(this->display, fbconfigs[i], GLX_ALPHA_SIZE, &alpha);
        glXGetFBConfigAttrib(this->display, fbconfigs[i], GLX_BUFFER_SIZE, &value);
        if (value != depth && (value - alpha) != depth)
            continue;

        value = 0;
        if (depth == 32) {
            glXGetFBConfigAttrib(this->display, fbconfigs[i],
                                 GLX_BIND_TO_TEXTURE_RGBA_EXT, &value);
            if (value)
                rgba = 1;
        }

        if (!value) {
            if (rgba)
                continue;

            glXGetFBConfigAttrib(this->display, fbconfigs[i],
                                 GLX_BIND_TO_TEXTURE_RGB_EXT, &value);
            if (!value)
                continue;
        }

        glXGetFBConfigAttrib(this->display, fbconfigs[i], GLX_DOUBLEBUFFER, &value);
        if (value > db)
            continue;
        db = value;

        glXGetFBConfigAttrib(this->display, fbconfigs[i], GLX_STENCIL_SIZE, &value);
        if (value > stencil)
            continue;
        stencil = value;

        found = i;
    }

    if (found != n_elements) {
        ret = malloc(sizeof(*ret));
        *ret = fbconfigs[found];
    }

    if (n_elements)
        XFree(fbconfigs);

    have_cached_config = 1;
    cached_config = ret;
    return ret;
}

static int vaapi_glx_config_tfp(vo_driver_t *this_gen, unsigned int width, unsigned int height)
{
  vaapi_driver_t *this = (vaapi_driver_t *) this_gen;

  GLXFBConfig *fbconfig;
  int attribs[7], i = 0;
  const int depth = 24;

  if (!mpglXBindTexImage || !mpglXReleaseTexImage) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_glx_config_tfp : No GLX texture-from-pixmap extension available\n");
    return 0;
  }

  if (depth != 24 && depth != 32) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_glx_config_tfp : color depth wrong.\n");
    return 0;
  }

  this->gl_image_pixmap = XCreatePixmap(this->display, this->drawable, width, height, depth);
  if (!this->gl_image_pixmap) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_glx_config_tfp : Could not create X11 pixmap\n");
    return 0;
  }

  fbconfig = get_fbconfig_for_depth(this_gen, depth);
  if (!fbconfig) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_glx_config_tfp : Could not find an FBConfig for 32-bit pixmap\n");
    return 0;
  }

  attribs[i++] = GLX_TEXTURE_TARGET_EXT;
  attribs[i++] = GLX_TEXTURE_2D_EXT;
  attribs[i++] = GLX_TEXTURE_FORMAT_EXT;
  if (depth == 24)
    attribs[i++] = GLX_TEXTURE_FORMAT_RGB_EXT;
  else if (depth == 32)
    attribs[i++] = GLX_TEXTURE_FORMAT_RGBA_EXT;
  attribs[i++] = GLX_MIPMAP_TEXTURE_EXT;
  attribs[i++] = GL_FALSE;
  attribs[i++] = None;

  vaapi_x11_trap_errors();
  this->gl_pixmap = mpglXCreatePixmap(this->display, *fbconfig, this->gl_image_pixmap, attribs);
  XSync(this->display, False);
  if (vaapi_x11_untrap_errors()) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_glx_config_tfp : Could not create GLX pixmap\n");
    return 0;
  }

  return 1;
}

static int vaapi_glx_config_glx(vo_driver_t *this_gen, unsigned int width, unsigned int height)
{
  vaapi_driver_t        *this = (vaapi_driver_t *) this_gen;
  ff_vaapi_context_t    *va_context = this->va_context;

  this->gl_vinfo = glXChooseVisual(this->display, this->screen, gl_visual_attr);
  if(!this->gl_vinfo) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_glx_config_glx : error glXChooseVisual\n");
    this->opengl_render = 0;
  }

  glXMakeCurrent(this->display, None, NULL);
  this->gl_context = glXCreateContext (this->display, this->gl_vinfo, NULL, True);
  if (this->gl_context) {
    if(!glXMakeCurrent (this->display, this->drawable, this->gl_context)) {
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_glx_config_glx : error glXMakeCurrent\n");
      return 0;
    }
  } else {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_glx_config_glx : error glXCreateContext\n");
    return 0;
  }

  void *(*getProcAddress)(const GLubyte *);
  const char *(*glXExtStr)(Display *, int);
  char *glxstr = strdup("");

  getProcAddress = vaapi_getdladdr("glXGetProcAddress");
  if (!getProcAddress)
    getProcAddress = vaapi_getdladdr("glXGetProcAddressARB");
  glXExtStr = vaapi_getdladdr("glXQueryExtensionsString");
  if (glXExtStr)
      vaapi_appendstr(&glxstr, glXExtStr(this->display, this->screen));
  glXExtStr = vaapi_getdladdr("glXGetClientString");
  if (glXExtStr)
      vaapi_appendstr(&glxstr, glXExtStr(this->display, GLX_EXTENSIONS));
  glXExtStr = vaapi_getdladdr("glXGetServerString");
  if (glXExtStr)
      vaapi_appendstr(&glxstr, glXExtStr(this->display, GLX_EXTENSIONS));

  vaapi_get_functions(this_gen, getProcAddress, glxstr);
  if (!mpglGenPrograms && mpglGetString &&
      getProcAddress &&
      strstr(mpglGetString(GL_EXTENSIONS), "GL_ARB_vertex_program")) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_glx_config_glx : Broken glXGetProcAddress detected, trying workaround\n");
    vaapi_get_functions(this_gen, NULL, glxstr);
  }
  free(glxstr);

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_TEXTURE_2D);
  glDrawBuffer(GL_BACK);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  /* Create TFP resources */
  if(this->opengl_use_tfp && vaapi_glx_config_tfp(this_gen, width, height)) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_glx_config_glx : Using GLX texture-from-pixmap extension\n");
  } else {
    this->opengl_use_tfp = 0;
  }

  /* Create OpenGL texture */
  /* XXX: assume GL_ARB_texture_non_power_of_two is available */
  glEnable(GL_TEXTURE_2D);
  glGenTextures(1, &this->gl_texture);
  mpglBindTexture(GL_TEXTURE_2D, this->gl_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  if (!this->opengl_use_tfp) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, NULL);
  }
  mpglBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_TEXTURE_2D);

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  if(!this->gl_texture) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_glx_config_glx : gl_texture NULL\n");
    return 0;
  }

  if(!this->opengl_use_tfp) {
    if( vaCreateSurfaceGLX(va_context->va_display, GL_TEXTURE_2D, this->gl_texture, &va_context->gl_surface)) {
      return 0;
    }
  }

  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_glx_config_glx : GL setup done\n");

  return 1;
}

static uint32_t vaapi_get_capabilities (vo_driver_t *this_gen) {
  vaapi_driver_t *this = (vaapi_driver_t *) this_gen;

  return this->capabilities;
}

static const struct {
  int fmt;
  enum PixelFormat pix_fmt;
  enum CodecID codec_id;
} conversion_map[] = {
  {IMGFMT_VAAPI_MPEG2,     PIX_FMT_VAAPI_VLD,  CODEC_ID_MPEG2VIDEO},
  {IMGFMT_VAAPI_MPEG2_IDCT,PIX_FMT_VAAPI_IDCT, CODEC_ID_MPEG2VIDEO},
  {IMGFMT_VAAPI_MPEG2_MOCO,PIX_FMT_VAAPI_MOCO, CODEC_ID_MPEG2VIDEO},
  {IMGFMT_VAAPI_MPEG4,     PIX_FMT_VAAPI_VLD,  CODEC_ID_MPEG4},
  {IMGFMT_VAAPI_H263,      PIX_FMT_VAAPI_VLD,  CODEC_ID_H263},
  {IMGFMT_VAAPI_H264,      PIX_FMT_VAAPI_VLD,  CODEC_ID_H264},
  {IMGFMT_VAAPI_WMV3,      PIX_FMT_VAAPI_VLD,  CODEC_ID_WMV3},
  {IMGFMT_VAAPI_VC1,       PIX_FMT_VAAPI_VLD,  CODEC_ID_VC1},
  {0, PIX_FMT_NONE}
};

/*
static enum PixelFormat vaapi_imgfmt2pixfmt(int fmt)
{
    int i;
    enum PixelFormat pix_fmt;
    for (i = 0; conversion_map[i].fmt; i++)
        if (conversion_map[i].fmt == fmt)
            break;
    pix_fmt = conversion_map[i].pix_fmt;
    return pix_fmt;
}
*/

static int vaapi_pixfmt2imgfmt(enum PixelFormat pix_fmt, int codec_id)
{
    int i;
    int fmt;
    for (i = 0; conversion_map[i].pix_fmt != PIX_FMT_NONE; i++)
        if (conversion_map[i].pix_fmt == pix_fmt &&
            (conversion_map[i].codec_id == 0 ||
             conversion_map[i].codec_id == codec_id))
            break;
    fmt = conversion_map[i].fmt;
    return fmt;
}

static int vaapi_has_profile(VAProfile profile)
{
    // TODO query profiles from the output device
    /*
    if (va_profiles && va_num_profiles > 0) {
        int i;
        for (i = 0; i < va_num_profiles; i++) {
            if (va_profiles[i] == profile)
                return 1;
        }
    }
    return 0;
    */
    return 1;
}

static int profile_from_imgfmt(vo_frame_t *frame_gen, enum PixelFormat pix_fmt, int codec_id, int vaapi_mpeg_sofdec)
{
    uint32_t format = vaapi_pixfmt2imgfmt(pix_fmt, codec_id);

    static const int mpeg2_profiles[] =
        { VAProfileMPEG2Main, VAProfileMPEG2Simple, -1 };
    static const int mpeg4_profiles[] =
        { VAProfileMPEG4Main, VAProfileMPEG4AdvancedSimple, VAProfileMPEG4Simple, -1 };
    static const int h264_profiles[] =
        { VAProfileH264High, VAProfileH264Main, VAProfileH264Baseline, -1 };
    static const int wmv3_profiles[] =
        { VAProfileVC1Main, VAProfileVC1Simple, -1 };
    static const int vc1_profiles[] =
        { VAProfileVC1Advanced, -1 };

    const int *profiles = NULL;
    switch (IMGFMT_VAAPI_CODEC(format)) {
    case IMGFMT_VAAPI_CODEC_MPEG2:
        if(!vaapi_mpeg_sofdec) {
          profiles = mpeg2_profiles;
        }
        break;
    case IMGFMT_VAAPI_CODEC_MPEG4:
        profiles = mpeg4_profiles;
        break;
    case IMGFMT_VAAPI_CODEC_H264:
        profiles = h264_profiles;
        break;
    case IMGFMT_VAAPI_CODEC_VC1:
        switch (format) {
        case IMGFMT_VAAPI_WMV3:
            profiles = wmv3_profiles;
            break;
        case IMGFMT_VAAPI_VC1:
            profiles = vc1_profiles;
            break;
        }
        break;
    }

    if (profiles) {
      int i;
        for (i = 0; profiles[i] != -1; i++) {
            if (vaapi_has_profile(profiles[i]))
                return profiles[i];
        }
    }
    return -1;
}


static const char *vaapi_profile_to_string(VAProfile profile)
{
  switch(profile) {
#define PROFILE(profile) \
    case VAProfile##profile: return "VAProfile" #profile
      PROFILE(MPEG2Simple);
      PROFILE(MPEG2Main);
      PROFILE(MPEG4Simple);
      PROFILE(MPEG4AdvancedSimple);
      PROFILE(MPEG4Main);
      PROFILE(H264Baseline);
      PROFILE(H264Main);
      PROFILE(H264High);
      PROFILE(VC1Simple);
      PROFILE(VC1Main);
      PROFILE(VC1Advanced);
#undef PROFILE
    default: break;
  }
  return "<unknown>";
}

static const char *vaapi_entrypoint_to_string(VAEntrypoint entrypoint)
{
  switch(entrypoint)
  {
#define ENTRYPOINT(entrypoint) \
    case VAEntrypoint##entrypoint: return "VAEntrypoint" #entrypoint
      ENTRYPOINT(VLD);
      ENTRYPOINT(IZZ);
      ENTRYPOINT(IDCT);
      ENTRYPOINT(MoComp);
      ENTRYPOINT(Deblocking);
#undef ENTRYPOINT
    default: break;
  }
  return "<unknown>";
}

/*
static int nv12_to_yv12(uint8_t  *src_in, uint8_t  *dst_out, int len, int width, int height) {

    unsigned int Y_size  = width * height;
    unsigned int UV_size = width * height;
    unsigned int idx;
    unsigned char *dst_Y = dst_out;
    unsigned char *dst_U = dst_out + width * height;
    unsigned char *dst_V = dst_out + width * height * 5/4;
    unsigned char *src   = src_in + Y_size;

    // sanity check raw stream
    if ( (len != (Y_size + (UV_size<<1))) ) {
        printf("hmblck: Image size inconsistent with data size.\n");
        return 0;
    }

    // luma data is easy, just copy it
    xine_fast_memcpy(dst_Y, src, Y_size);

    // chroma data is interlaced UVUV... so deinterlace it
    for(idx=0; idx<UV_size; idx++ ) {
        *(dst_U + idx) = *(src + (idx<<1) + 0); 
        *(dst_V + idx) = *(src + (idx<<1) + 1);
    }

    return 1;
}
*/

/* Init subpicture */
static void vaapi_init_subpicture(vaapi_driver_t *this_gen) {
  vaapi_driver_t        *this = (vaapi_driver_t *) this_gen;
  ff_vaapi_context_t    *va_context = this->va_context;

  va_context->va_subpic_width               = 0;
  va_context->va_subpic_height              = 0;
  va_context->va_subpic_id                  = VA_INVALID_ID;
  va_context->va_subpic_image.image_id      = VA_INVALID_ID;
  this->osd_displayed                       = 0;
  va_context->va_osd_associated             = 0;

  this->overlay_output_width = this->overlay_output_height = 0;
  this->overlay_unscaled_width = this->overlay_unscaled_height = 0;
  this->ovl_changed = 0;
  this->has_overlay = 0;
  this->overlay_bitmap = NULL;
  this->overlay_bitmap_size = 0;

}

/* Init vaapi context */
static void vaapi_init_va_context(vaapi_driver_t *this_gen) {
  vaapi_driver_t        *this = (vaapi_driver_t *) this_gen;
  ff_vaapi_context_t    *va_context = this->va_context;
  int i;

  va_context->va_config_id              = VA_INVALID_ID;
  va_context->va_context_id             = VA_INVALID_ID;
  va_context->va_profile                = 0;
  this->init_vaapi                      = 0;
  this->valid_context                   = 0;
  va_context->va_head                   = 0;
  va_context->va_output_head            = 0;
  va_context->va_tail                   = 0;
  va_context->is_bound                  = 0;
  va_context->gl_surface                = NULL;
  va_context->va_osd_associated         = 0;

  for(i = 0; i < RENDER_SURFACES; i++) {
    va_surface_ids[i] = VA_INVALID_SURFACE;
  }

  for(i = 0; i < OUTPUT_SURFACES; i++) {
    va_output_surface_ids[i] = VA_INVALID_SURFACE;
    va_output_image[i].image_id = VA_INVALID_ID;
  }
}

/* Close vaapi  */
static void vaapi_close(vaapi_driver_t *this, ff_vaapi_context_t *va_context) {

  if(va_context->va_display == NULL)
    return;

  int i;

  if(!this->opengl_use_tfp && va_context->gl_surface)
    vaDestroySurfaceGLX(va_context->va_display, va_context->gl_surface);

  vaapi_destroy_subpicture((vo_driver_t *)this);

  if(va_context->va_context_id != VA_INVALID_ID)
    vaDestroyContext(va_context->va_display, va_context->va_context_id);

  for(i = 0; i < OUTPUT_SURFACES; i++) {
    destroy_image((vo_driver_t *)this, &va_output_image[i]);
  }

  for(i = 0; i < RENDER_SURFACES; i++) {
    if(va_surface_ids[i] != VA_INVALID_SURFACE) {
#ifdef DEBUG_SURFACE
      printf("vaapi_close destroy render surface 0x%08x\n", va_surface_ids[i]);
#endif
      vaDestroySurfaces(va_context->va_display, &va_surface_ids[i], 1);
    }
  }

  /*
  if(va_surface_ids[0] != VA_INVALID_SURFACE)
      vaDestroySurfaces(va_context->va_display, va_surface_ids, RENDER_SURFACES);
  */

  for(i = 0; i < OUTPUT_SURFACES; i++) {
    if(va_output_surface_ids[i] != VA_INVALID_SURFACE) {
#ifdef DEBUG_SURFACE
      printf("vaapi_close destroy output surface 0x%08x\n", va_output_surface_ids[i]);
#endif
      vaDestroySurfaces(va_context->va_display, &va_output_surface_ids[i], 1);
    }
  }

  destroy_glx((vo_driver_t *)this);

  if(va_context->va_config_id != VA_INVALID_ID)
    vaDestroyConfig(va_context->va_display, va_context->va_config_id);

  if(this->reinit) {
    vaTerminate(va_context->va_display);
    va_context->va_display = NULL;
  }

  vaapi_init_va_context(this);
}

/* Returns internal VAAPI context */
static ff_vaapi_context_t *get_context(vo_frame_t *frame_gen) {
  vaapi_driver_t        *this = (vaapi_driver_t *) frame_gen->driver;

  return this->va_context;
}

/* Free allocated VAAPI image */
static void destroy_image(vo_driver_t *this_gen, VAImage *va_image) {
  vaapi_driver_t        *this = (vaapi_driver_t *) this_gen;
  ff_vaapi_context_t    *va_context = this->va_context;

  if(va_image->image_id != VA_INVALID_ID) {
    lprintf("destroy_image 0x%08x\n", va_image->image_id);
    vaDestroyImage(va_context->va_display, va_image->image_id);
    va_image->image_id = VA_INVALID_ID;
    va_image->width = 0;
    va_image->height = 0;
  }
}

/* Allocated VAAPI image */
static VAStatus vaapi_create_image(vo_driver_t *this_gen, VASurfaceID va_surface_id, VAImage *va_image, int width, int height) {
  vaapi_driver_t        *this = (vaapi_driver_t *) this_gen;
  ff_vaapi_context_t    *va_context = this->va_context;

  int i = 0;
  int fmt_count = 0;
  VAImageFormat *va_p_fmt = NULL;
  VAStatus vaStatus;

  if(!va_context->va_display)
    return VA_STATUS_ERROR_UNKNOWN;

  fmt_count = vaMaxNumImageFormats( va_context->va_display );
  va_p_fmt = calloc( fmt_count, sizeof(*va_p_fmt) );

  destroy_image(this_gen, va_image);

  vaStatus = vaQueryImageFormats( va_context->va_display , va_p_fmt, &fmt_count );
  if(!vaapi_check_status(this_gen, vaStatus, "vaQueryImageFormats()"))
    goto error;

  vaStatus = vaDeriveImage(va_context->va_display, va_surface_id, va_image);
  if(vaStatus == VA_STATUS_SUCCESS) {
    va_context->is_bound = 1;
  } else {
    for (i = 0; i < fmt_count; i++) {
      if ( va_p_fmt[i].fourcc == VA_FOURCC( 'Y', 'V', '1', '2' ) /*||
           va_p_fmt[i].fourcc == VA_FOURCC( 'N', 'V', '1', '2' ) */) {
        vaStatus = vaCreateImage( va_context->va_display, &va_p_fmt[i], width, height, va_image );
        if(!vaapi_check_status(this_gen, vaStatus, "vaCreateImage()")) {
          va_image->image_id = VA_INVALID_ID;
          goto error;
        } 
        lprintf("sucessfull created vaImage 0x%08x\n", va_image->image_id);
        break;
      }
    }
  }

  void *p_base = NULL;

  vaStatus = vaMapBuffer( va_context->va_display, va_image->buf, &p_base );
  if(vaapi_check_status(this_gen, vaStatus, "vaMapBuffer()")) {
    memset((uint32_t *)p_base, 0x0, va_image->data_size);
    vaUnmapBuffer( va_context->va_display, va_image->buf );
  }

  lprintf("vaapi_create_image 0x%08x\n", va_image->image_id);

  free(va_p_fmt);
  return VA_STATUS_SUCCESS;

error:
  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " error create image\n");
  free(va_p_fmt);
  return VA_STATUS_ERROR_UNKNOWN;
}

/* Deassociate and free subpicture */
static void vaapi_destroy_subpicture(vo_driver_t *this_gen) {
  vaapi_driver_t        *this = (vaapi_driver_t *) this_gen;
  ff_vaapi_context_t    *va_context = this->va_context;

  if(va_context->va_osd_associated && va_context->va_subpic_id != VA_INVALID_ID) {
    if(va_context->softsurface) {
      vaDeassociateSubpicture(va_context->va_display, va_context->va_subpic_id,
          va_output_surface_ids, OUTPUT_SURFACES);
    } else {
      vaDeassociateSubpicture(va_context->va_display, va_context->va_subpic_id,
          va_surface_ids, RENDER_SURFACES);
    }
    va_context->va_osd_associated = 0;
  }

  destroy_image(this_gen, &va_context->va_subpic_image);

  if(va_context->va_subpic_id != VA_INVALID_ID)
    vaDestroySubpicture(va_context->va_display, va_context->va_subpic_id);

  va_context->va_subpic_id = VA_INVALID_ID;
}

/* Create VAAPI subpicture */
static VAStatus vaapi_create_subpicture(vo_driver_t *this_gen, int width, int height) {
  vaapi_driver_t      *this = (vaapi_driver_t *) this_gen;
  ff_vaapi_context_t  *va_context = this->va_context;
  VAStatus            vaStatus;

  int i = 0;
  int fmt_count = 0;
  VAImageFormat *va_p_fmt = NULL;

  if(!va_context->va_display)
    return VA_STATUS_ERROR_UNKNOWN;

  fmt_count = vaMaxNumSubpictureFormats( va_context->va_display );
  va_p_fmt = calloc( fmt_count, sizeof(*va_p_fmt) );

  vaStatus = vaQuerySubpictureFormats( va_context->va_display , va_p_fmt, 0, &fmt_count );
  if(!vaapi_check_status(this_gen, vaStatus, "vaQuerySubpictureFormats()"))
    goto error;
  
  for (i = 0; i < fmt_count; i++) {
    if ( va_p_fmt[i].fourcc == VA_FOURCC('B','G','R','A')) {
      vaStatus = vaCreateImage( va_context->va_display, &va_p_fmt[i], width, height, &va_context->va_subpic_image );
      if(!vaapi_check_status(this_gen, vaStatus, "vaCreateImage()")) {
        va_context->va_subpic_image.image_id = VA_INVALID_ID;
        goto error;
      }

      vaStatus = vaCreateSubpicture(va_context->va_display, va_context->va_subpic_image.image_id, &va_context->va_subpic_id );
      if(!vaapi_check_status(this_gen, vaStatus, "vaCreateSubpicture()")) {
        va_context->va_subpic_id = VA_INVALID_ID;

        destroy_image(this_gen, &va_context->va_subpic_image);

        goto error;
      }
      lprintf("sucessfull vaCreateSubpicture\n");
    }
  }

  lprintf("vaapi_create_subpicture 0x%08x\n", va_context->va_subpic_image.image_id);

  void *p_base = NULL;

  vaStatus = vaMapBuffer(va_context->va_display, va_context->va_subpic_image.buf, &p_base);
  if(vaapi_check_status(this_gen, vaStatus, "vaMapBuffer()")) {
    memset((uint32_t *)p_base, 0x0, va_context->va_subpic_image.data_size);
    vaUnmapBuffer(va_context->va_display, va_context->va_subpic_image.buf);
  }

  this->overlay_output_width  = width;
  this->overlay_output_height = height;

  free(va_p_fmt);
  return VA_STATUS_SUCCESS;

error:
  free(va_p_fmt);
  return VA_STATUS_ERROR_UNKNOWN;
}

/* VAAPI display attributes. */
static void vaapi_display_attribs(vo_driver_t *this_gen) {
  vaapi_driver_t      *this = (vaapi_driver_t *) this_gen;
  ff_vaapi_context_t  *va_context = this->va_context;

  int num_display_attrs, max_display_attrs;
  VAStatus vaStatus;
  VADisplayAttribute *display_attrs;
  int i;

  memset(&va_context->va_equalizer, 0x0, sizeof(struct vaapi_equalizer));

  max_display_attrs = vaMaxNumDisplayAttributes(va_context->va_display);
  display_attrs = calloc(max_display_attrs, sizeof(*display_attrs));
  if (display_attrs) {
    num_display_attrs = 0;
    vaStatus = vaQueryDisplayAttributes(va_context->va_display,
                                        display_attrs, &num_display_attrs);
    if(vaapi_check_status(this_gen, vaStatus, "vaQueryDisplayAttributes()")) {
      for (i = 0; i < num_display_attrs; i++) {
        VADisplayAttribute *attr;
        switch (display_attrs[i].type) {
          case VADisplayAttribBrightness:
            attr = &va_context->va_equalizer.brightness;
            break;
          case VADisplayAttribContrast:
            attr = &va_context->va_equalizer.contrast;
            break;
          case VADisplayAttribHue:
            attr = &va_context->va_equalizer.hue;
            break;
          case VADisplayAttribSaturation:
            attr = &va_context->va_equalizer.saturation;
            break;
          default:
            attr = NULL;
            break;
        }
        if (attr)
          *attr = display_attrs[i];
      }
    }
    free(display_attrs);
  }
}

/* Init VAAPI. This function is called from the decoder side.
 * When the decoder uses software decoding vaapi_init is not called.
 * Therefore we do it in vaapi_display_frame to get a valid VAAPI context too.*/
static VAStatus vaapi_init(vo_frame_t *frame_gen, int va_profile, int width, int height, int softsurface) {
  vo_driver_t         *this_gen = (vo_driver_t *) frame_gen->driver;
  vaapi_driver_t      *this = (vaapi_driver_t *) frame_gen->driver;
  ff_vaapi_context_t  *va_context = this->va_context;
  VAConfigAttrib      va_attrib;
  int                 maj, min, i;
  VAStatus            vaStatus;

  XLockDisplay(this->display);

  /* Reinit VAAPI or reuse context. */
  if(this->reinit) {

    vaapi_close(this, va_context);

    va_context->va_display = vaapi_get_display(this->display, this->opengl_render);

    if(!va_context->va_display)
      goto error;

    vaStatus = vaInitialize(va_context->va_display, &maj, &min);
    if(!vaapi_check_status(this_gen, vaStatus, "vaInitialize()"))
      goto error;

    lprintf("libva: %d.%d\n", maj, min);
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_init: Vendor : %s\n", vaQueryVendorString(va_context->va_display));

  } else {
    if(va_context->va_profile == va_profile && va_context->width == width && va_context->height == height)
      return VA_STATUS_SUCCESS;
    vaapi_close(this, va_context);
  }

  VADisplayAttribute attr;
  memset( &attr, 0, sizeof(attr) );
  attr.type  = VADisplayAttribBackgroundColor;
  attr.value = 0x00000000;
  vaSetDisplayAttributes(va_context->va_display, &attr, 1);

  va_context->width = width;
  va_context->height = height;
  va_context->softsurface = softsurface;

  va_context->va_profile = va_profile;

  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_init : Context width %d height %d\n", va_context->width, va_context->height);

  /* allocate decoding surfaces */
  vaStatus = vaCreateSurfaces(va_context->va_display, va_context->width, va_context->height, VA_RT_FORMAT_YUV420, RENDER_SURFACES, va_surface_ids);
  if(!vaapi_check_status(this_gen, vaStatus, "vaCreateSurfaces()")) {
    for(i = 0; i < RENDER_SURFACES; i++) {
      va_surface_ids[i] = VA_INVALID_SURFACE;
    }
    goto error;
  }

  /* xine was told to allocate RENDER_SURFACES frames. assign the frames the rendering surfaces. */
  for(i = 0; i < RENDER_SURFACES; i++) {
    lprintf("setup frames\n");
    if(this->frames[i]) {
      vaapi_frame_t *frame = this->frames[i];
      frame->vaapi_accel_data.va_surface_id = va_surface_ids[i];
      lprintf("frame->surface_id 0x%08x\n", frame->vaapi_accel_data.va_surface_id);
    }
  }

  /* allocate output surfaces */
  vaStatus = vaCreateSurfaces(va_context->va_display, va_context->width, va_context->height, VA_RT_FORMAT_YUV420, OUTPUT_SURFACES, va_output_surface_ids);
  if(!vaapi_check_status(this_gen, vaStatus, "vaCreateSurfaces()")) {
    for(i = 0; i < OUTPUT_SURFACES; i++) {
      va_output_surface_ids[i] = VA_INVALID_SURFACE;
    }
    goto error;
  }

  /* Special case for vaapi softoutput only */
  if(!softsurface) {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_init : Profile: %d (%s) Entrypoint %d (%s) Surfaces %d\n", va_context->va_profile, vaapi_profile_to_string(va_context->va_profile), VAEntrypointVLD, vaapi_entrypoint_to_string(VAEntrypointVLD), RENDER_SURFACES);

    memset( &va_attrib, 0, sizeof(va_attrib) );
    va_attrib.type = VAConfigAttribRTFormat;

    vaStatus = vaGetConfigAttributes(va_context->va_display, va_context->va_profile, VAEntrypointVLD, &va_attrib, 1);
    if(!vaapi_check_status(this_gen, vaStatus, "vaGetConfigAttributes()"))
      goto error;
  
    if( (va_attrib.value & VA_RT_FORMAT_YUV420) == 0 )
      goto error;

    vaStatus = vaCreateConfig(va_context->va_display, va_context->va_profile, VAEntrypointVLD, &va_attrib, 1, &va_context->va_config_id);
    if(!vaapi_check_status(this_gen, vaStatus, "vaCreateConfig()")) {
      va_context->va_config_id = VA_INVALID_ID;
      goto error;
    }

    vaStatus = vaCreateContext(va_context->va_display, va_context->va_config_id, va_context->width, va_context->height,
                               VA_PROGRESSIVE, va_surface_ids, RENDER_SURFACES, &va_context->va_context_id);
    if(!vaapi_check_status(this_gen, vaStatus, "vaCreateContext()")) {
      va_context->va_context_id = VA_INVALID_ID;
      goto error;
    }
  }

  for(i = 0; i < OUTPUT_SURFACES; i++) {
    vaStatus = vaapi_create_image((vo_driver_t *)this, va_output_surface_ids[i], &va_output_image[i], va_context->width, va_context->height);
    if(!vaapi_check_status(this_gen, vaStatus, "vaapi_create_image()"))
      goto error;
  }

  this->valid_context = 1;

  vaapi_display_attribs((vo_driver_t *)this);

  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_init : softsurface : %d\n", softsurface);
  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_init : glxsurface : %d\n", this->opengl_render);
  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_init : glxsurface tfp : %d\n", this->opengl_use_tfp);
  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_init : brightness : %d\n", va_context->va_equalizer.brightness.value);
  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_init : contrast : %d\n", va_context->va_equalizer.contrast.value);
  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_init : hue : %d\n", va_context->va_equalizer.hue.value);
  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_init : saturation : %d\n", va_context->va_equalizer.saturation.value); 

  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_init : sucessfull\n");

  XUnlockDisplay(this->display);

  return VA_STATUS_SUCCESS;

error:
  vaapi_close(this, va_context);
  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_init : error init vaapi\n");

  XUnlockDisplay(this->display);

  return VA_STATUS_ERROR_UNKNOWN;
}

static void vaapi_frame_proc_slice (vo_frame_t *vo_img, uint8_t **src)
{
  vo_img->proc_called = 1;
}

static void vaapi_frame_field (vo_frame_t *vo_img, int which_field)
{
}

static void vaapi_frame_dispose (vo_frame_t *vo_img) {
  //vaapi_driver_t  *this  = (vaapi_driver_t *) vo_img->driver;
  vaapi_frame_t  *frame = (vaapi_frame_t *) vo_img ;

  lprintf("vaapi_frame_dispose\n");

  av_free (frame->vo_frame.base[0]);
  av_free (frame->vo_frame.base[1]);
  av_free (frame->vo_frame.base[2]);
  free (frame);
}

static vo_frame_t *vaapi_alloc_frame (vo_driver_t *this_gen) {
  vaapi_driver_t  *this = (vaapi_driver_t *) this_gen;
  vaapi_frame_t   *frame;

  frame = (vaapi_frame_t *) calloc(1, sizeof(vaapi_frame_t));

  if (!frame)
    return NULL;

  this->frames[this->num_frame_buffers++] = frame;

  frame->vo_frame.base[0] = frame->vo_frame.base[1] = frame->vo_frame.base[2] = NULL;
  frame->width = frame->height = frame->format = frame->flags = 0;

  frame->vo_frame.accel_data = &frame->vaapi_accel_data;

  pthread_mutex_init (&frame->vo_frame.mutex, NULL);

  /*
   * supply required functions
   */
  frame->vo_frame.proc_duplicate_frame_data         = NULL;
  frame->vo_frame.proc_provide_standard_frame_data  = NULL;
  frame->vo_frame.proc_slice                        = vaapi_frame_proc_slice;
  frame->vo_frame.proc_frame                        = NULL;
  frame->vo_frame.field                             = vaapi_frame_field;
  frame->vo_frame.dispose                           = vaapi_frame_dispose;
  frame->vo_frame.driver                            = this_gen;

  frame->vaapi_accel_data.vo_frame                  = &frame->vo_frame;
  frame->vaapi_accel_data.vaapi_init                = &vaapi_init;
  frame->vaapi_accel_data.profile_from_imgfmt       = &profile_from_imgfmt;
  frame->vaapi_accel_data.get_context               = &get_context;

  lprintf("alloc frame\n");

  return (vo_frame_t *) frame;
}

static void vaapi_update_frame_format (vo_driver_t *this_gen,
				    vo_frame_t *frame_gen,
				    uint32_t width, uint32_t height,
				    double ratio, int format, int flags) {
  vaapi_driver_t  *this  = (vaapi_driver_t *) this_gen;
  vaapi_frame_t   *frame = (vaapi_frame_t*)frame_gen;

  XLockDisplay(this->display);

  frame->vo_frame.width = width;
  frame->vo_frame.height = height;

  if ((frame->width != width)
      || (frame->height != height)
      || (frame->format != format)) {

    // (re-) allocate render space
    av_freep (&frame->vo_frame.base[0]);
    av_freep (&frame->vo_frame.base[1]);
    av_freep (&frame->vo_frame.base[2]);

    /* set init_vaapi on frame formats XINE_IMGFMT_YV12/XINE_IMGFMT_YUY2 only.
     * for XINE_IMGFMT_VAAPI the init was already done.
     */
    if (format == XINE_IMGFMT_YV12) {
      this->init_vaapi = 1;
      frame->vo_frame.pitches[0] = 8*((width + 7) / 8);
      frame->vo_frame.pitches[1] = 8*((width + 15) / 16);
      frame->vo_frame.pitches[2] = 8*((width + 15) / 16);
      frame->vo_frame.base[0] = av_mallocz (frame->vo_frame.pitches[0] * height);
      frame->vo_frame.base[1] = av_mallocz (frame->vo_frame.pitches[1] * ((height+1)/2));
      frame->vo_frame.base[2] = av_mallocz (frame->vo_frame.pitches[2] * ((height+1)/2));
    } else if (format == XINE_IMGFMT_YUY2){
      this->init_vaapi = 1;
      frame->vo_frame.pitches[0] = 8*((width + 3) / 4);
      frame->vo_frame.base[0] = av_mallocz (frame->vo_frame.pitches[0] * height);
    } else {
      this->init_vaapi = 0;
      frame->vo_frame.proc_duplicate_frame_data = NULL;
      frame->vo_frame.proc_provide_standard_frame_data = NULL;
    }

    frame->width  = width;
    frame->height = height;
    frame->format = format;
    frame->flags  = flags;

    vaapi_frame_field ((vo_frame_t *)frame, flags);
  }

  XUnlockDisplay(this->display);

  frame->ratio = ratio;
  frame->vo_frame.future_frame = NULL;
}

/* Display OSD */
static int vaapi_ovl_associate(vo_driver_t *this_gen, int bShow) {
  vaapi_driver_t      *this = (vaapi_driver_t *) this_gen;
  ff_vaapi_context_t  *va_context = this->va_context;
  VAStatus vaStatus;

  if(!this->valid_context)
    return 0;

  if(!bShow) {
    if(va_context->va_osd_associated)
      vaapi_destroy_subpicture(this_gen);
    return 1;
  }
  
  if(!this->osd_displayed) {

    void *p_base = NULL;

    vaapi_destroy_subpicture(this_gen);
    vaapi_create_subpicture(this_gen, this->overlay_bitmap_width, this->overlay_bitmap_height);

    lprintf( "vaapi overlay: overlay_width=%d overlay_height=%d unscaled %d va_subpic_id 0x%08x ovl_changed %d has_overlay %d bShow %d overlay_bitmap_width %d overlay_bitmap_height %d va_context->width %d va_context->height %d\n", 
           this->overlay_output_width, this->overlay_output_height, this->has_overlay, 
           va_context->va_subpic_id, this->ovl_changed, this->has_overlay, bShow,
           this->overlay_bitmap_width, this->overlay_bitmap_height,
           va_context->width, va_context->height);

    if(va_context->va_subpic_id != VA_INVALID_ID && 
       va_context->va_subpic_image.image_id != VA_INVALID_ID) {

      vaStatus = vaMapBuffer(va_context->va_display, va_context->va_subpic_image.buf, &p_base);

      if(vaapi_check_status(this_gen, vaStatus, "vaMapBuffer()")) {
        xine_fast_memcpy((uint32_t *)p_base, this->overlay_bitmap, this->overlay_bitmap_width * this->overlay_bitmap_height * sizeof(uint32_t));
  
        vaUnmapBuffer(va_context->va_display, va_context->va_subpic_image.buf);
      }

      unsigned int flags = 0;
      unsigned int output_width = va_context->width;
      unsigned int output_height = va_context->height;

      /*
      if(!this->vdr_osd_width && !this->vdr_osd_height) {
        flags |= VA_SUBPICTURE_DESTINATION_IS_SCREEN_COORD;
        output_width = this->sc.gui_width;
        output_height = this->sc.gui_height;
      }
      */

      lprintf( "vaapi overlay: va_context->va_subpic_image.width %d va_context->va_subpic_image.height %d this->overlay_bitmap_height %d this->overlay_bitmap_width %d\n", va_context->va_subpic_image.width, va_context->va_subpic_image.height, this->overlay_bitmap_width, this->overlay_bitmap_height);

      if(va_context->softsurface) {
        vaStatus = vaAssociateSubpicture(va_context->va_display, va_context->va_subpic_id,
                                va_output_surface_ids, OUTPUT_SURFACES,
                                0, 0, va_context->va_subpic_image.width, va_context->va_subpic_image.height,
                                0, 0, output_width, output_height, flags);
      } else {
        vaStatus = vaAssociateSubpicture(va_context->va_display, va_context->va_subpic_id,
                                va_surface_ids, RENDER_SURFACES,
                                0, 0, va_context->va_subpic_image.width, va_context->va_subpic_image.height,
                                0, 0, output_width, output_height, flags);
      }
      if(vaapi_check_status(this_gen, vaStatus, "vaAssociateSubpicture()")) {
        this->osd_displayed = 1;
        va_context->va_osd_associated = 1;
      }
      else {
        this->osd_displayed = 0;
        va_context->va_osd_associated = 0;
      }
    }
    return 1;
  }

  return 0;
}

static void vaapi_overlay_clut_yuv2rgb(vaapi_driver_t  *this, vo_overlay_t *overlay, vaapi_frame_t *frame)
{
  int i;
  clut_t* clut = (clut_t*) overlay->color;

  if (!overlay->rgb_clut) {
    for ( i=0; i<sizeof(overlay->color)/sizeof(overlay->color[0]); i++ ) {
      *((uint32_t *)&clut[i]) = this->ovl_yuv2rgb->yuv2rgb_single_pixel_fun(this->ovl_yuv2rgb, clut[i].y, clut[i].cb, clut[i].cr);
    }
    overlay->rgb_clut++;
  }
  if (!overlay->hili_rgb_clut) {
    clut = (clut_t*) overlay->hili_color;
    for ( i=0; i<sizeof(overlay->color)/sizeof(overlay->color[0]); i++) {
      *((uint32_t *)&clut[i]) = this->ovl_yuv2rgb->yuv2rgb_single_pixel_fun(this->ovl_yuv2rgb, clut[i].y, clut[i].cb, clut[i].cr);
    }
    overlay->hili_rgb_clut++;
  }
}

static void vaapi_overlay_begin (vo_driver_t *this_gen,
			      vo_frame_t *frame_gen, int changed) {
  vaapi_driver_t  *this = (vaapi_driver_t *) this_gen;

  if ( !changed )
    return;

  lprintf("vaapi_overlay_begin chaned %d\n", changed);

  this->has_overlay = 0;
  ++this->ovl_changed;

  this->osd_displayed = 0;
  /* Apply OSD layer. */
  if(this->valid_context) {
    vaapi_ovl_associate(frame_gen->driver, this->has_overlay);
  }
}

static void vaapi_overlay_blend (vo_driver_t *this_gen,
			      vo_frame_t *frame_gen, vo_overlay_t *overlay) {
  vaapi_driver_t  *this = (vaapi_driver_t *) this_gen;

  int i = this->ovl_changed;

  if (!i)
    return;

  if (--i >= XINE_VORAW_MAX_OVL)
    return;

  if (overlay->width <= 0 || overlay->height <= 0 || (!overlay->rle && (!overlay->argb_layer || !overlay->argb_layer->buffer)))
    return;

  if (overlay->rle)
    lprintf("overlay[%d] rle %s%s %dx%d@%d,%d hili rect %d,%d-%d,%d\n", i,
            overlay->unscaled ? " unscaled ": " scaled ",
            (overlay->rgb_clut > 0 || overlay->hili_rgb_clut > 0) ? " rgb ": " ycbcr ",
            overlay->width, overlay->height, overlay->x, overlay->y,
            overlay->hili_left, overlay->hili_top,
            overlay->hili_right, overlay->hili_bottom);
  if (overlay->argb_layer && overlay->argb_layer->buffer)
    lprintf("overlay[%d] argb %s %dx%d@%d,%d dirty rect %d,%d-%d,%d\n", i,
            overlay->unscaled ? " unscaled ": " scaled ",
            overlay->width, overlay->height, overlay->x, overlay->y,
            overlay->argb_layer->x1, overlay->argb_layer->y1,
            overlay->argb_layer->x2, overlay->argb_layer->y2);


  this->overlays[i] = overlay;

  ++this->ovl_changed;
  /* Apply OSD layer. */
  /*
  if(this->valid_context) {
    vaapi_ovl_associate(frame_gen->driver, this->has_overlay);
  }
  */
}

static void vaapi_overlay_end (vo_driver_t *this_gen, vo_frame_t *frame_gen) {
  vaapi_driver_t  *this = (vaapi_driver_t *) this_gen;
  vaapi_frame_t   *frame = (vaapi_frame_t *) frame_gen;

  int novls = this->ovl_changed;
  if (novls < 2) {
    this->ovl_changed = 0;
    return;
  }
  --novls;

  uint32_t output_width = frame->width;
  uint32_t output_height = frame->height;
  uint32_t unscaled_width = 0, unscaled_height = 0;
  vo_overlay_t *first_scaled = NULL, *first_unscaled = NULL;
  vaapi_rect_t dirty_rect, unscaled_dirty_rect;
  int has_rle = 0;

  int i;
  for (i = 0; i < novls; ++i) {
    vo_overlay_t *ovl = this->overlays[i];

    if (ovl->rle)
      has_rle = 1;

    if (ovl->unscaled) {
      if (first_unscaled) {
        if (ovl->x < unscaled_dirty_rect.x1)
          unscaled_dirty_rect.x1 = ovl->x;
        if (ovl->y < unscaled_dirty_rect.y1)
          unscaled_dirty_rect.y1 = ovl->y;
        if ((ovl->x + ovl->width) > unscaled_dirty_rect.x2)
          unscaled_dirty_rect.x2 = ovl->x + ovl->width;
        if ((ovl->y + ovl->height) > unscaled_dirty_rect.y2)
          unscaled_dirty_rect.y2 = ovl->y + ovl->height;
      } else {
        first_unscaled = ovl;
        unscaled_dirty_rect.x1 = ovl->x;
        unscaled_dirty_rect.y1 = ovl->y;
        unscaled_dirty_rect.x2 = ovl->x + ovl->width;
        unscaled_dirty_rect.y2 = ovl->y + ovl->height;
      }

      unscaled_width = unscaled_dirty_rect.x2;
      unscaled_height = unscaled_dirty_rect.y2;
    } else {
      if (first_scaled) {
        if (ovl->x < dirty_rect.x1)
          dirty_rect.x1 = ovl->x;
        if (ovl->y < dirty_rect.y1)
          dirty_rect.y1 = ovl->y;
        if ((ovl->x + ovl->width) > dirty_rect.x2)
          dirty_rect.x2 = ovl->x + ovl->width;
        if ((ovl->y + ovl->height) > dirty_rect.y2)
          dirty_rect.y2 = ovl->y + ovl->height;
      } else {
        first_scaled = ovl;
        dirty_rect.x1 = ovl->x;
        dirty_rect.y1 = ovl->y;
        dirty_rect.x2 = ovl->x + ovl->width;
        dirty_rect.y2 = ovl->y + ovl->height;
      }

      if (dirty_rect.x2 > output_width)
        output_width = dirty_rect.x2;
      if (dirty_rect.y2 > output_height)
        output_height = dirty_rect.y2;

    }
  }

  int need_init = 0;

  lprintf("dirty_rect.x0 %d dirty_rect.y0 %d dirty_rect.x2 %d dirty_rect.y2 %d output_width %d output_height %d\n",
      dirty_rect.x0, dirty_rect.y0, dirty_rect.x2, dirty_rect.y2, output_width, output_height);

  if (first_scaled) {
    vaapi_rect_t dest;
    dest.x1 = first_scaled->x;
    dest.y1 = first_scaled->y;
    dest.x2 = first_scaled->x + first_scaled->width;
    dest.y2 = first_scaled->y + first_scaled->height;
    if (!RECT_IS_EQ(dest, dirty_rect))
      need_init = 1;
  }

  int need_unscaled_init = (first_unscaled &&
                                  (first_unscaled->x != unscaled_dirty_rect.x1 ||
                                   first_unscaled->y != unscaled_dirty_rect.y1 ||
                                   (first_unscaled->x + first_unscaled->width) != unscaled_dirty_rect.x2 ||
                                   (first_unscaled->y + first_unscaled->height) != unscaled_dirty_rect.y2));

  if (first_scaled) {
    this->overlay_output_width = output_width;
    this->overlay_output_height = output_height;

    need_init = 1;

    this->overlay_dirty_rect = dirty_rect;
  }

  if (first_unscaled) {
    this->overlay_unscaled_width = unscaled_width;
    this->overlay_unscaled_height = unscaled_height;

    need_unscaled_init = 1;
    this->overlay_unscaled_dirty_rect = unscaled_dirty_rect;
  }

  if (has_rle || need_init || need_unscaled_init) {
    lprintf("has_rle %d need_init %d need_unscaled_init %d unscaled_width %d unscaled_height %d output_width %d output_height %d\n", 
        has_rle, need_init, need_unscaled_init, unscaled_width, unscaled_height, output_width, output_height);
    if (need_init) {
      this->overlay_bitmap_width = output_width;
      this->overlay_bitmap_height = output_height;
    }
    if (need_unscaled_init) {

      if(this->vdr_osd_width) 
        this->overlay_bitmap_width =  (this->vdr_osd_width >  this->sc.gui_width) ? this->vdr_osd_width : this->sc.gui_width;
      else
        this->overlay_bitmap_width =  (unscaled_width >  this->sc.gui_width) ? unscaled_width : this->sc.gui_width;

      if(this->vdr_osd_height) 
        this->overlay_bitmap_height = (this->vdr_osd_height > this->sc.gui_height) ? this->vdr_osd_height : this->sc.gui_height;
      else
        this->overlay_bitmap_height = (unscaled_height > this->sc.gui_height) ? unscaled_height : this->sc.gui_height;

    } else if (need_init) {

      if(this->vdr_osd_width) 
        this->overlay_bitmap_width =  (this->vdr_osd_width >  this->sc.gui_width) ? this->vdr_osd_width : this->sc.gui_width;
      else
        this->overlay_bitmap_width =  (output_width >  this->sc.gui_width) ? output_width : this->sc.gui_width;

      if(this->vdr_osd_height) 
        this->overlay_bitmap_height = (this->vdr_osd_height > this->sc.gui_height) ? this->vdr_osd_height : this->sc.gui_height;
      else
        this->overlay_bitmap_height = (output_height > this->sc.gui_height) ? output_height : this->sc.gui_height;

    }
  }

  if ((this->overlay_bitmap_width * this->overlay_bitmap_height) > this->overlay_bitmap_size) {
    this->overlay_bitmap_size = this->overlay_bitmap_width * this->overlay_bitmap_height;
    free(this->overlay_bitmap);
    this->overlay_bitmap = calloc( this->overlay_bitmap_size, sizeof(uint32_t));
  } else {
    memset(this->overlay_bitmap, 0x0, this->overlay_bitmap_size * sizeof(uint32_t));
  }

  for (i = 0; i < novls; ++i) {
    vo_overlay_t *ovl = this->overlays[i];
    uint32_t *bitmap = NULL;
    uint32_t *rgba = NULL;

    if (ovl->rle) {
      if(ovl->width<=0 || ovl->height<=0)
        continue;

      if (!ovl->rgb_clut || !ovl->hili_rgb_clut)
        vaapi_overlay_clut_yuv2rgb (this, ovl, frame);

      bitmap = rgba = calloc(ovl->width * ovl->height * 4, sizeof(uint32_t));

      int num_rle = ovl->num_rle;
      rle_elem_t *rle = ovl->rle;
      uint32_t red, green, blue, alpha;
      clut_t *low_colors = (clut_t*)ovl->color;
      clut_t *hili_colors = (clut_t*)ovl->hili_color;
      uint8_t *low_trans = ovl->trans;
      uint8_t *hili_trans = ovl->hili_trans;
      clut_t *colors;
      uint8_t *trans;
      int rlelen = 0;
      uint8_t clr = 0;
      int i, pos=0, x, y;

      while (num_rle > 0) {
        x = pos % ovl->width;
        y = pos / ovl->width;

        if ( (x>=ovl->hili_left && x<=ovl->hili_right) && (y>=ovl->hili_top && y<=ovl->hili_bottom) ) {
          colors = hili_colors;
          trans = hili_trans;
        }
        else {
          colors = low_colors;
          trans = low_trans;
        }
        rlelen = rle->len;
        clr = rle->color;
        for ( i=0; i<rlelen; ++i ) {
          if ( trans[clr] == 0 ) {
            alpha = red = green = blue = 0;
          }
          else {
            red = colors[clr].y; // red
            green = colors[clr].cr; // green
            blue = colors[clr].cb; // blue
            alpha = trans[clr]*255/15;
          }
          *rgba = (alpha<<24) | (red<<16) | (green<<8) | blue;
          rgba++;
          ++pos;
        }
        ++rle;
        --num_rle;
      }
      lprintf("width %d height %d pos %d %d\n", ovl->width, ovl->height, pos, ovl->width * ovl->height);
    } else {
      pthread_mutex_lock(&ovl->argb_layer->mutex);
      bitmap = ovl->argb_layer->buffer;
    }

    /* Blit overlay to destination */

    uint32_t pitch = ovl->width * sizeof(uint32_t);
    uint32_t *copy_dst = this->overlay_bitmap;
    uint32_t *copy_src = NULL;
    uint32_t height = 0;

    copy_src = bitmap;

    copy_dst += ovl->y * this->overlay_bitmap_width;

    lprintf("overlay_bitmap_width %d overlay_bitmap_height %d  ovl->x %d ovl->y %d ovl->width %d ovl->height %d width %d height %d\n",
        this->overlay_bitmap_width, this->overlay_bitmap_height, ovl->x, ovl->y, ovl->width, ovl->height, this->overlay_bitmap_width, this->overlay_bitmap_height);

    for(height = 0; height < ovl->height; height++) {

      if((height + ovl->y) >= this->overlay_bitmap_height)
        break;

      xine_fast_memcpy(copy_dst + ovl->x, copy_src, pitch);
      copy_dst += this->overlay_bitmap_width;
      copy_src += ovl->width;

    }

    if (ovl->rle) {
      if(bitmap) {
        free(bitmap);
        bitmap = NULL;
      }
    }

    if (!ovl->rle)
      pthread_mutex_unlock(&ovl->argb_layer->mutex);

  }

  this->ovl_changed = 0;
  this->has_overlay = (first_scaled != NULL) | (first_unscaled != NULL);

  lprintf("this->has_overlay %d\n", this->has_overlay);
  /* Apply OSD layer. */
  if(this->valid_context) {
    vaapi_ovl_associate(frame_gen->driver, this->has_overlay);
  }
}

/*
static void vaapi_add_recent_frame (vaapi_driver_t *this, vaapi_frame_t *frame) {
  int i;

  i = VO_NUM_RECENT_FRAMES-1;
  if( this->recent_frames[i] )
    this->recent_frames[i]->vo_frame.free(&this->recent_frames[i]->vo_frame);

  for( ; i ; i-- )
    this->recent_frames[i] = this->recent_frames[i-1];

  this->recent_frames[0] = frame;
}
*/

static int vaapi_redraw_needed (vo_driver_t *this_gen) {
  vaapi_driver_t  *this = (vaapi_driver_t *) this_gen;

  _x_vo_scale_compute_ideal_size( &this->sc );

  if ( _x_vo_scale_redraw_needed( &this->sc ) ) {
    _x_vo_scale_compute_output_size( &this->sc );

    int width = this->sc.gui_width;
    int height = this->sc.gui_height;

    if(this->opengl_render && this->opengl_init) {
      glViewport(0, 0, width, height);
      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      gluPerspective(FOVY, ASPECT, Z_NEAR, Z_FAR);
      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();
      glTranslatef(-0.5f, -0.5f, -Z_CAMERA);
      glScalef(1.0f / (GLfloat)width, 
               -1.0f / (GLfloat)height,
               1.0f / (GLfloat)width);
      glTranslatef(0.0f, -1.0f * (GLfloat)height, 0.0f);
    }
    return 1;
  }

  
  return 0;
}

/* Used in vaapi_display_frame to determine how long displaying a frame takes
   - if slower than 60fps, print a message
*/
static double timeOfDay()
{
    struct timeval t;
    gettimeofday( &t, NULL );
    return ((double)t.tv_sec) + (((double)t.tv_usec)/1000000.0);
}

static void vaapi_display_frame (vo_driver_t *this_gen, vo_frame_t *frame_gen) {
  vaapi_driver_t     *this          = (vaapi_driver_t *) this_gen;
  vaapi_frame_t      *frame         = (vaapi_frame_t *) frame_gen;
  vaapi_accel_t      *accel         = &frame->vaapi_accel_data;
  ff_vaapi_context_t *va_context    = this->va_context;
  VASurfaceID        va_surface_id  = VA_INVALID_SURFACE;
  VAImage            *va_image      = NULL;
  void               *p_base_dst    = NULL;
  VAStatus            vaStatus;
  VAStatus            vaStatusDst;

  //vaapi_add_recent_frame (this, frame); /* deinterlacing */

  //this->cur_frame = frame;

  /*
   * let's see if this frame is different in size / aspect
   * ratio from the previous one
   */

  if ( (frame->width != this->sc.delivered_width)
       || (frame->height != this->sc.delivered_height)
       || (frame->ratio != this->sc.delivered_ratio)
       || (frame->vo_frame.crop_left != this->sc.crop_left)
       || (frame->vo_frame.crop_right != this->sc.crop_right)
       || (frame->vo_frame.crop_top != this->sc.crop_top)
       || (frame->vo_frame.crop_bottom != this->sc.crop_bottom) ) {
    lprintf("frame format changed\n");
    this->sc.force_redraw = 1;
  }

  this->sc.delivered_height = frame->height;
  this->sc.delivered_width  = frame->width;
  this->sc.delivered_ratio  = frame->ratio;
  this->sc.crop_left        = frame->vo_frame.crop_left;
  this->sc.crop_right       = frame->vo_frame.crop_right;
  this->sc.crop_top         = frame->vo_frame.crop_top;
  this->sc.crop_bottom      = frame->vo_frame.crop_bottom;

  /*
   * tell gui that we are about to display a frame,
   * ask for offset and output size
   */

  /* XINE_IMGFMT_YUY2/XINE_IMGFMT_YV12 are not stored in a VAAPI surface.
   * This are software decoded frames. So VAAPI is not initialized from
   * the decoder side. Lets inititalize here */

  if(!this->valid_context)
    this->init_vaapi = 1;

  if(this->init_vaapi) {
    vaapi_init(frame_gen, 0, frame->width, frame->height, 1);
    this->init_vaapi = 0;
    this->sc.force_redraw = 1;
  }

  /* The opengl rendering is not inititalized till here.
   * Lets do the opengl magic here and inititalize it.
   * Opengl init must be done before redraw 
   */
  if(this->opengl_render && !this->opengl_init) {
    destroy_glx(frame_gen->driver);
    vaapi_glx_config_glx(frame_gen->driver, frame->width, frame->height);
    this->opengl_init = 1;
    this->sc.force_redraw = 1;
  }

  vaapi_redraw_needed (this_gen);

  XLockDisplay(this->display);

  double start_time;
  double end_time;
  double elapse_time;
  int factor;

  start_time = timeOfDay();

  if(this->valid_context && ( (frame->format == XINE_IMGFMT_VAAPI) || (frame->format == XINE_IMGFMT_YV12) || (frame->format == XINE_IMGFMT_YUY2) ) ) {

    if(va_context->softsurface && ((frame_gen->width != va_context->width) || (frame_gen->height != va_context->height))) {
      lprintf("frame_gen->width %d va_context->width %d frame_gen->height %d va_context->height %d\n",
          frame_gen->width, va_context->width, frame_gen->height, va_context->height);

      vaapi_init(frame_gen, 0, frame->width, frame->height, 1);
    }

    if ( frame->format == XINE_IMGFMT_VAAPI ) {
#ifdef DEBUG_SURFACE
      printf("accel->va_surface 0x%08x\n", accel->va_surface_id);
#endif
      va_surface_id = accel->va_surface_id;
    } else {
      va_surface_id = va_output_surface_ids[va_context->va_output_head];
      va_image = &va_output_image[va_context->va_output_head];

      va_context->va_output_head = (va_context->va_output_head + 1) % (OUTPUT_SURFACES);
    }

    VASurfaceStatus surf_status = 0;
    if(va_surface_id != VA_INVALID_SURFACE) {

      if(this->query_va_status) {
        vaQuerySurfaceStatus(va_context->va_display, va_surface_id, &surf_status);
      } else {
        surf_status = VASurfaceReady;
      }

      if(surf_status != VASurfaceReady) {
        va_surface_id = VA_INVALID_SURFACE;
        va_image = NULL;
#ifdef DEBUG_SURFACE
        printf("Surface srfc 0x%08X not ready for render\n", va_surface_id);
#endif
      }
    } else {
#ifdef DEBUG_SURFACE
      printf("Invalid srfc 0x%08X\n", va_surface_id);
#endif
    }

    if(va_surface_id != VA_INVALID_SURFACE) {

      lprintf("vaapi_display_frame: 0x%08x %d %d\n", va_surface_id, va_context->width, va_context->height);
    
      int i = 0;
      int interlaced_frame    = !frame->vo_frame.progressive_frame;
      int top_field_first     = frame->vo_frame.top_field_first;

      if(frame->format == XINE_IMGFMT_YUY2 || frame->format == XINE_IMGFMT_YV12) {

        lprintf("imageconvert : va_surface_id 0x%08x va_image.image_id 0x%08x\n", va_surface_id, va_image->image_id);

        vaStatusDst = vaMapBuffer( va_context->va_display, va_image->buf, &p_base_dst ) ;

        if(vaapi_check_status(this_gen, vaStatusDst, "vaMapBuffer()")) {

          /* Copy xine frames into VAAPI images */
          if(frame->format == XINE_IMGFMT_YV12) {

            if(va_context->softsurface) {

              lprintf("softsurface yv12_to_yv12 convert\n");

              yv12_to_yv12(
                /* Y */
                  frame_gen->base[0], frame_gen->pitches[0],
                  (uint8_t*)p_base_dst + va_image->offsets[0], va_image->pitches[0],
                /* U */
                  frame_gen->base[1], frame_gen->pitches[1],
                  (uint8_t*)p_base_dst + va_image->offsets[2], va_image->pitches[2],
                /* V */
                  frame_gen->base[2], frame_gen->pitches[2],
                  (uint8_t*)p_base_dst + va_image->offsets[1], va_image->pitches[1],
                /* width x height */
                  frame_gen->width, frame_gen->height);
            }

          } else if (frame->format == XINE_IMGFMT_YUY2) {

            lprintf("softsurface yuy2_to_yv12 convert\n");

            yuy2_to_yv12(frame_gen->base[0], frame_gen->pitches[0],
                   (uint8_t*)p_base_dst + va_image->offsets[0], va_image->pitches[0],
                   (uint8_t*)p_base_dst + va_image->offsets[2], va_image->pitches[2],
                   (uint8_t*)p_base_dst + va_image->offsets[1], va_image->pitches[1],
                   frame_gen->width, frame_gen->height);

            lprintf("XINE_IMGFMT_YV12 %d %d %d %d %d %d\n", va_image->width, va_image->height, 
                frame_gen->width, frame_gen->height, va_context->width, va_context->height);
          }

          vaUnmapBuffer(va_context->va_display, va_image->buf);

          if(!va_context->is_bound) {
            vaStatus = vaPutImage(va_context->va_display, va_surface_id, va_image->image_id,
                       0, 0, va_image->width, va_image->height,
                       0, 0, va_image->width, va_image->height);
            vaapi_check_status(this_gen, vaStatus, "vaPutSurface()");
          }

        }
      }

      /* Final VAAPI rendering. The deinterlacing can be controled by xine config.*/
      for(i = 0; i <= !!((this->deinterlace > 1) && interlaced_frame); i++) {
        unsigned int flags = (this->deinterlace && (interlaced_frame) ? (((!!(top_field_first)) ^ i) == 0 ? VA_BOTTOM_FIELD : VA_TOP_FIELD) : VA_FRAME_PICTURE);

        //flags |= VA_FILTER_SCALING_NL_ANAMORPHIC;

        lprintf("Putsrfc srfc 0x%08X flags 0x%08x %dx%d -> %dx%d interlaced %d top_field_first %d\n", 
                va_surface_id, flags, va_context->width, va_context->height, 
                this->sc.output_width, this->sc.output_height,
                interlaced_frame, top_field_first);

        if(this->opengl_render) {

          if(this->opengl_use_tfp) {
            lprintf("opengl render tfp\n");
            vaStatus = vaPutSurface(va_context->va_display, va_surface_id, this->gl_image_pixmap,
                     0, 0, va_context->width, va_context->height,
                     0, 0, va_context->width, va_context->height,
                     NULL, 0, flags);
            if(!vaapi_check_status(this_gen, vaStatus, "vaPutSurface()"))
              break;
          } else {
            lprintf("opengl render\n");
            vaStatus = vaCopySurfaceGLX(va_context->va_display,
                                      va_context->gl_surface,
                                      va_surface_id,
                                      flags);
            if(!vaapi_check_status(this_gen, vaStatus, "vaCopySurfaceGLX()"))
              break;
          }

          vaapi_glx_flip_page(frame_gen, 0, 0, va_context->width, va_context->height);

        } else {

          vaStatus = vaPutSurface(va_context->va_display, va_surface_id, this->drawable,
                   0, 0, va_context->width, va_context->height,
                   this->sc.output_xoffset, this->sc.output_yoffset,
                   this->sc.output_width, this->sc.output_height,
                   NULL, 0, flags);
          if(!vaapi_check_status(this_gen, vaStatus, "vaPutSurface()"))
            break;
        }

      }

      //vaSyncSurface(va_context->va_display, va_surface_id);
    }
  } else {
   xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " unsupported image format 0x%08x\n", frame->format);
  }

  /* Apply OSD layer. */
  /*
  if(this->valid_context) {
    vaapi_ovl_associate(frame_gen->driver, this->has_overlay);
  }
  */

  XSync(this->display, False);

  frame->vo_frame.free( frame_gen );

  end_time = timeOfDay();

  XUnlockDisplay(this->display);

  elapse_time = end_time - start_time;
  factor = (int)(elapse_time/(1.0/60.0));

  if( factor > 1 )
  {
    xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " PutImage %dX interval (%fs)\n", factor, elapse_time );
  }
}

static int vaapi_get_property (vo_driver_t *this_gen, int property) {
  vaapi_driver_t *this = (vaapi_driver_t *) this_gen;

  switch (property) {
    case VO_PROP_WINDOW_WIDTH:
      return this->sc.gui_width;
    case VO_PROP_WINDOW_HEIGHT:
      return this->sc.gui_height;
    case VO_PROP_OUTPUT_WIDTH:
      return this->sc.output_width;
    case VO_PROP_OUTPUT_HEIGHT:
      return this->sc.output_height;
    case VO_PROP_OUTPUT_XOFFSET:
      return this->sc.output_xoffset;
    case VO_PROP_OUTPUT_YOFFSET:
      return this->sc.output_yoffset;
    case VO_PROP_ZOOM_X:
      return this->zoom_x;
    case VO_PROP_ZOOM_Y:
      return this->zoom_y;
    case VO_PROP_ASPECT_RATIO:
      return this->sc.user_ratio;
    case VO_PROP_MAX_NUM_FRAMES:
      return RENDER_SURFACES;
  } 

  return -1;
}

static int vaapi_set_property (vo_driver_t *this_gen, int property, int value) {

  vaapi_driver_t *this = (vaapi_driver_t *) this_gen;

  lprintf("vaapi_set_property: property=%d, value=%d\n", property, value );

  switch (property) {

    case VO_PROP_ASPECT_RATIO:
      if (value>=XINE_VO_ASPECT_NUM_RATIOS)
	      value = XINE_VO_ASPECT_AUTO;
      this->sc.user_ratio = value;
      this->sc.force_redraw = 1;    /* trigger re-calc of output size */
      break;

    case VO_PROP_ZOOM_X:
      if ((value >= XINE_VO_ZOOM_MIN) && (value <= XINE_VO_ZOOM_MAX)) {
        xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " : VO_PROP_ZOOM_X = %d\n", value);

        this->zoom_x = value;
	      this->sc.zoom_factor_x = (double)value / (double)XINE_VO_ZOOM_STEP;
        _x_vo_scale_compute_ideal_size (&this->sc);
        this->sc.force_redraw = 1;    /* trigger re-calc of output size */
      }
      break;

    case VO_PROP_ZOOM_Y:
      if ((value >= XINE_VO_ZOOM_MIN) && (value <= XINE_VO_ZOOM_MAX)) {
	      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " : VO_PROP_ZOOM_Y = %d\n", value);
        
        this->zoom_y = value;
        this->sc.zoom_factor_y = (double)value / (double)XINE_VO_ZOOM_STEP;
	      _x_vo_scale_compute_ideal_size (&this->sc);
	      this->sc.force_redraw = 1;    /* trigger re-calc of output size */
      }
      break;
  }
  return value;
}

static void vaapi_get_property_min_max (vo_driver_t *this_gen,
				     int property, int *min, int *max) {
  switch ( property ) {
    default:
      *max = 0; *min = 0;
  }
}

static int vaapi_gui_data_exchange (vo_driver_t *this_gen,
				 int data_type, void *data) {
  vaapi_driver_t     *this = (vaapi_driver_t *) this_gen;

  switch (data_type) {
#ifndef XINE_DISABLE_DEPRECATED_FEATURES
  case XINE_GUI_SEND_COMPLETION_EVENT:
    break;
#endif

  case XINE_GUI_SEND_EXPOSE_EVENT: {
    XExposeEvent * xev = (XExposeEvent *) data;
    if (xev && xev->count == 0) {
      XLockDisplay( this->display );
      XSync(this->display, False);
      XUnlockDisplay( this->display );
      this->sc.force_redraw = 1;
    }
  }
  break;

  case XINE_GUI_SEND_DRAWABLE_CHANGED: {
    lprintf("XINE_GUI_SEND_DRAWABLE_CHANGED\n");
    XLockDisplay( this->display );
    this->drawable = (Drawable) data;
    XFreeGC(this->display, this->gc);
    this->gc = XCreateGC (this->display, this->drawable, 0, NULL);
    this->opengl_init = 0;
    XUnlockDisplay( this->display );
    this->sc.force_redraw = 1;
  }
  break;

  case XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO: {
    int x1, y1, x2, y2;
    x11_rectangle_t *rect = data;

    _x_vo_scale_translate_gui2video(&this->sc, rect->x, rect->y, &x1, &y1);
    _x_vo_scale_translate_gui2video(&this->sc, rect->x + rect->w, rect->y + rect->h, &x2, &y2);
    rect->x = x1;
    rect->y = y1;
    rect->w = x2-x1;
    rect->h = y2-y1;
  } 
  break;

  default:
    return -1;
  }

  return 0;
}

static void vaapi_dispose (vo_driver_t *this_gen) {
  vaapi_driver_t      *this = (vaapi_driver_t *) this_gen;
  ff_vaapi_context_t  *va_context = this->va_context;

  /*
  int i;
  for( i=0; i < VO_NUM_RECENT_FRAMES; i++ ) {
    if( this->recent_frames[i] )
      this->recent_frames[i]->vo_frame.dispose
         (&this->recent_frames[i]->vo_frame);
    this->recent_frames[i] = NULL;
  }
  */

  this->ovl_yuv2rgb->dispose(this->ovl_yuv2rgb);
  this->yuv2rgb_factory->dispose (this->yuv2rgb_factory);

  this->init_vaapi              = 0;

  XLockDisplay(this->display);

  XFreeGC(this->display, this->gc);

  if(va_context) {

    vaapi_close(this, va_context);

    if(va_context->va_display)
      vaTerminate(va_context->va_display);

    free(va_context);
  }

  XUnlockDisplay(this->display);

  if(this->overlay_bitmap)
    free(this->overlay_bitmap);

  if(va_surface_ids)
    free(va_surface_ids);
  if(va_output_surface_ids)
    free(va_output_surface_ids);
  if(va_output_image)
    free(va_output_image);

  free (this);
}

static void vaapi_vdr_osd_width_flag( void *this_gen, xine_cfg_entry_t *entry )
{
  vaapi_driver_t  *this  = (vaapi_driver_t *) this_gen;

  this->vdr_osd_width = entry->num_value;

  fprintf(stderr, "vo_vaapi: vdr_osd_width=%d\n", this->vdr_osd_width );
}

static void vaapi_vdr_osd_height_flag( void *this_gen, xine_cfg_entry_t *entry )
{
  vaapi_driver_t  *this  = (vaapi_driver_t *) this_gen;

  this->vdr_osd_height = entry->num_value;

  fprintf(stderr, "vo_vaapi: vdr_osd_height=%d\n", this->vdr_osd_height );
}

static void vaapi_deinterlace_flag( void *this_gen, xine_cfg_entry_t *entry )
{
  vaapi_driver_t  *this  = (vaapi_driver_t *) this_gen;

  this->deinterlace = entry->num_value;
  if(this->deinterlace > 2)
    this->deinterlace = 2;

  fprintf(stderr, "vo_vaapi: deinterlace=%d\n", this->deinterlace );
}

static void vaapi_reinit_flag( void *this_gen, xine_cfg_entry_t *entry )
{
  vaapi_driver_t  *this  = (vaapi_driver_t *) this_gen;

  this->reinit = entry->num_value;
  if(this->reinit > 1)
    this->reinit = 1;

  fprintf(stderr, "vo_vaapi: reinit=%d\n", this->reinit );
}

static void vaapi_opengl_render( void *this_gen, xine_cfg_entry_t *entry )
{
  vaapi_driver_t  *this  = (vaapi_driver_t *) this_gen;

  this->opengl_render = entry->num_value;

  fprintf(stderr, "vo_vaapi: opengl_render=%d\n", this->opengl_render );
}

static void vaapi_opengl_use_tfp( void *this_gen, xine_cfg_entry_t *entry )
{
  vaapi_driver_t  *this  = (vaapi_driver_t *) this_gen;

  this->opengl_use_tfp = entry->num_value;

  fprintf(stderr, "vo_vaapi: opengl_use_tfp=%d\n", this->opengl_use_tfp );
}

static vo_driver_t *vaapi_open_plugin (video_driver_class_t *class_gen, const void *visual_gen) {

  vaapi_class_t           *class  = (vaapi_class_t *) class_gen;
  x11_visual_t            *visual = (x11_visual_t *) visual_gen;
  vaapi_driver_t          *this;
  config_values_t         *config = class->config;
  XColor                  dummy;
  int                     maj, min;

  this = (vaapi_driver_t *) calloc(1, sizeof(vaapi_driver_t));
  if (!this)
    return NULL;

  this->config                  = config;
  this->xine                    = class->xine;

  this->display                 = visual->display;
  this->screen                  = visual->screen;
  this->drawable                = visual->d;

  this->va_context              = calloc(1, sizeof(ff_vaapi_context_t));
  ff_vaapi_context_t            *va_context = this->va_context;

  /* number of video frames from config - register it with the default value. */
  int frame_num = config->register_num (config, "engine.buffers.video_num_frames", RENDER_SURFACES, /* default */
       _("default number of video frames"),
       _("The default number of video frames to request "
         "from xine video out driver. Some drivers will "
         "override this setting with their own values."),
      20, NULL, this);

  /* now make sure we have at least 22 frames, to prevent
   * locks with vdpau_h264 */
  if(frame_num != RENDER_SURFACES)
    config->update_num(config,"engine.buffers.video_num_frames", RENDER_SURFACES);

  this->opengl_render = config->register_bool( config, "video.output.vaapi_opengl_render", 0,
        _("vaapi: opengl output rendering"),
        _("vaapi: opengl output rendering"),
        20, vaapi_opengl_render, this );

  this->opengl_use_tfp = config->register_bool( config, "video.output.vaapi_opengl_use_tfp", 0,
        _("vaapi: opengl rendering tfp"),
        _("vaapi: opengl rendering tfp"),
        20, vaapi_opengl_use_tfp, this );

  if(this->opengl_render) {
      this->opengl_render = vaapi_opengl_verify_direct ((x11_visual_t *)visual_gen);
      if(!this->opengl_render)
        xprintf (this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_open: Opengl indirect/software rendering does not work. Fallback to plain VAAPI output !!!!\n");
  }

  this->opengl_init                     = 0;
  this->gl_vinfo                        = NULL;
  this->gl_pixmap                       = None;
  this->gl_image_pixmap                 = None;

  XLockDisplay( this->display ) ;
  this->gc                      = XCreateGC (this->display, this->drawable, 0, NULL);
  XAllocNamedColor (this->display,
		    DefaultColormap(this->display, this->screen),
		    "black", &this->black, &dummy);
  XUnlockDisplay( this->display ) ;

  this->num_frame_buffers               = 0;

  va_surface_ids                        = calloc(RENDER_SURFACES + 1, sizeof(VASurfaceID));
  va_output_surface_ids                 = calloc(OUTPUT_SURFACES + 1, sizeof(VASurfaceID));
  va_output_image                       = calloc(OUTPUT_SURFACES + 1, sizeof(VAImage));

  vaapi_init_va_context(this);
  vaapi_init_subpicture(this);

  va_context->va_display        = NULL;

  va_context->va_display = vaapi_get_display(this->display, this->opengl_render);

  if( va_context->va_display ) {
    VAStatus vaStatus;

    vaStatus = vaInitialize(va_context->va_display, &maj, &min);

    if(vaapi_check_status((vo_driver_t *)this, vaStatus, "vaInitialize()")) {
      lprintf("libva: %d.%d\n", maj, min);
      const char *vendor = vaQueryVendorString(va_context->va_display);
      xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_open: Vendor : %s\n", vendor);

      this->query_va_status = 1;
      int i =0;
      char *p = (char *)vendor;
      for(i = 0; i < strlen(vendor); i++, p++) {
        if(strncmp(p, "VDPAU", strlen("VDPAU")) == 0) {
          xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_open: Enable Splitted-Desktop Systems VDPAU workarounds. Please contact Splitted-Desktop to fix this missbehavior. VAAPI will not work stable without a working vaQuerySurfaceStatus extension.\n");
          this->query_va_status = 0;
          break;
        }
      }

    } else {
      free(this);
      free(va_context);
      return NULL;
    }
  } else {
    free(this);
    free(va_context);
    return NULL;
  }

  _x_vo_scale_init (&this->sc, 1, 0, config );

  this->sc.frame_output_cb      = visual->frame_output_cb;
  this->sc.dest_size_cb         = visual->dest_size_cb;
  this->sc.user_data            = visual->user_data;
  this->sc.user_ratio           = XINE_VO_ASPECT_AUTO;

  this->zoom_x                  = 100;
  this->zoom_y                  = 100;

  this->init_vaapi              = 0;

  this->capabilities            = VO_CAP_YV12 | VO_CAP_YUY2 | VO_CAP_CROP | VO_CAP_UNSCALED_OVERLAY | VO_CAP_ARGB_LAYER_OVERLAY | VO_CAP_VAAPI | VO_CAP_CUSTOM_EXTENT_OVERLAY;

  /*  overlay converter */
  this->yuv2rgb_factory = yuv2rgb_factory_init (MODE_24_BGR, 0, NULL);
  this->ovl_yuv2rgb = this->yuv2rgb_factory->create_converter( this->yuv2rgb_factory );

  this->vo_driver.get_capabilities     = vaapi_get_capabilities;
  this->vo_driver.alloc_frame          = vaapi_alloc_frame;
  this->vo_driver.update_frame_format  = vaapi_update_frame_format;
  this->vo_driver.overlay_begin        = vaapi_overlay_begin;
  this->vo_driver.overlay_blend        = vaapi_overlay_blend;
  this->vo_driver.overlay_end          = vaapi_overlay_end;
  this->vo_driver.display_frame        = vaapi_display_frame;
  this->vo_driver.get_property         = vaapi_get_property;
  this->vo_driver.set_property         = vaapi_set_property;
  this->vo_driver.get_property_min_max = vaapi_get_property_min_max;
  this->vo_driver.gui_data_exchange    = vaapi_gui_data_exchange;
  this->vo_driver.dispose              = vaapi_dispose;
  this->vo_driver.redraw_needed        = vaapi_redraw_needed;

  this->init_vaapi                     = 0;
  this->deinterlace                    = 0;
  this->reinit                         = 0;
  this->vdr_osd_width                  = 0;
  this->vdr_osd_height                 = 0;

  this->vdr_osd_width = config->register_num( config, "video.output.vaapi_vdr_osd_width", 0,
        _("vaapi: VDR osd width workaround."),
        _("vaapi: VDR osd width workaround."),
        10, vaapi_vdr_osd_width_flag, this );

  this->vdr_osd_height = config->register_num( config, "video.output.vaapi_vdr_osd_height", 0,
        _("vaapi: VDR osd height workaround."),
        _("vaapi: VDR osd height workaround."),
        10, vaapi_vdr_osd_height_flag, this );

  this->deinterlace = config->register_num( config, "video.output.vaapi_deinterlace", 0,
        _("vaapi: set deinterlace to 0 ( none ), 1 ( top field ), 2 ( bob )."),
        _("vaapi: set deinterlace to 0 ( none ), 1 ( top field ), 2 ( bob )."),
        10, vaapi_deinterlace_flag, this );

  this->reinit = config->register_num( config, "video.output.vaapi_reinit", 1,
        _("vaapi: reinit vaapi on every format change. When no reinit is used deinterlaced is turned off."),
        _("vaapi: reinit vaapi on every format change. When no reinit is used deinterlaced is turned off."),
        10, vaapi_reinit_flag, this );

  if(!this->reinit)
    this->deinterlace = 0;

  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_open: Deinterlace : %d\n", this->deinterlace);
  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_open: Reinit : %d\n", this->reinit);
  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_open: Render surfaces : %d\n", RENDER_SURFACES);
  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_open: Output surfaces : %d\n", OUTPUT_SURFACES);
  xprintf(this->xine, XINE_VERBOSITY_LOG, LOG_MODULE " vaapi_open: Opengl render : %d\n", this->opengl_render);

  return &this->vo_driver;
}

/*
 * class functions
 */
static void *vaapi_init_class (xine_t *xine, void *visual_gen) {
  vaapi_class_t        *this = (vaapi_class_t *) calloc(1, sizeof(vaapi_class_t));

  this->driver_class.open_plugin     = vaapi_open_plugin;
  this->driver_class.identifier      = "vaapi";
  this->driver_class.description     = N_("xine video output plugin using the MIT X video extension");
  this->driver_class.dispose         = default_video_driver_class_dispose;
  this->config                       = xine->config;
  this->xine                         = xine;

  return this;
}

static const vo_info_t vo_info_vaapi = {
  9,                      /* priority    */
  XINE_VISUAL_TYPE_X11    /* visual type */
};

/*
 * exported plugin catalog entry
 */

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_VIDEO_OUT, 22, "vaapi", XINE_VERSION_CODE, &vo_info_vaapi, vaapi_init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
