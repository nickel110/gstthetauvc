/* GStreamer
 * Copyright (C) 2021 Koji Takeo <nickel110@icloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstthetatransform
 *
 * The thetatransform element does fisheye to equirectanguler transformation stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v thetauvcsrc ! h264parse ! decodebin ! glupload ! thetatransform tablefile-l=l.dat tablefile-r=r.dat ! glimagesink
 * ]|
 * Image transformation from fisheye to equirectangular image and top-bottom correction
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>

#include <gst/gst.h>
#include <gst/gl/gstglapi.h>
#include "gstthetatransform.h"
#include "gst/gl/gstglutils.h"

#include "gstglutils.h"

#include "shader.h"

GST_DEBUG_CATEGORY_STATIC (gst_thetatransform_debug_category);
#define GST_CAT_DEFAULT gst_thetatransform_debug_category

#define gst_thetatransform_parent_class parent_class


/* prototypes */


static void gst_thetatransform_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_thetatransform_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);

static gboolean gst_thetatransform_set_caps(GstGLFilter *, GstCaps *, GstCaps *);
static gboolean gst_thetatransform_start (GstGLBaseFilter *);
static void gst_thetatransform_stop (GstGLBaseFilter *);
static gboolean gst_thetatransform_filter(GstGLFilter *, GstBuffer *, GstBuffer *);
static gboolean gst_thetatransform_filter_texture(GstGLFilter *, GstGLMemory *, GstGLMemory *);

static gboolean draw(gpointer);

enum
{
    PROP_0,
    PROP_ROT_X,
    PROP_ROT_Y,
    PROP_ROT_Z,
    PROP_TBLFILE_L,
    PROP_TBLFILE_R,
    PROP_VSHADER_FILE,
    PROP_FSHADER_FILE,
    PROP_DISABLE_STITCH
};


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstThetatransform, gst_thetatransform, GST_TYPE_GL_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_thetatransform_debug_category, "thetatransform", 0,
	"debug category for thetatransform element"));

static void
gst_thetatransform_class_init (GstThetatransformClass * klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GstGLBaseFilterClass *gl_base_filter_class = GST_GL_BASE_FILTER_CLASS (klass);
    GstGLFilterClass *gl_filter_class = GST_GL_FILTER_CLASS (klass);

    gst_gl_filter_add_rgba_pad_templates (GST_GL_FILTER_CLASS (klass));

    gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
	"theta transformation filter", "Filter/Effect/Video",
	"Fisheye to equirectanguler transformation",
	"Koji Takeo <nickel110@icloud.com>");

    gobject_class->set_property = gst_thetatransform_set_property;
    gobject_class->get_property = gst_thetatransform_get_property;

    gl_base_filter_class->gl_start = GST_DEBUG_FUNCPTR (gst_thetatransform_start);
    gl_base_filter_class->gl_stop = GST_DEBUG_FUNCPTR (gst_thetatransform_stop);

  
    gl_filter_class->set_caps = GST_DEBUG_FUNCPTR (gst_thetatransform_set_caps);
    gl_filter_class->filter = GST_DEBUG_FUNCPTR (gst_thetatransform_filter);
    gl_filter_class->filter_texture = GST_DEBUG_FUNCPTR (gst_thetatransform_filter_texture);

    gl_base_filter_class->supported_gl_api = GST_GL_API_OPENGL3 | GST_GL_API_GLES2;


    g_object_class_install_property(gobject_class, PROP_ROT_X,
	g_param_spec_float("rotX", "Rotation X",
	    "Rotation angle around X-axis in degree by x-y-z intrinsic rotation",
	    -180.f, 180.f, 0.f, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));
    g_object_class_install_property(gobject_class, PROP_ROT_Y,
	g_param_spec_float("rotY", "Rotation Y",
	    "Rotation angle around Y-axis in degree by x-y-z intrinsic rotation",
	    -180.f, 180.f, -90.f, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));
    g_object_class_install_property(gobject_class, PROP_ROT_Z,
	g_param_spec_float("rotZ", "Rotation Z",
	    "Rotation angle around Z-axis in degree by x-y-z intrinsic rotation",
	    -180.f, 180.f, 0.f, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));
    g_object_class_install_property(gobject_class, PROP_TBLFILE_L,
	g_param_spec_string("tablefile-l", "Table file L",
	    "transform table for left image", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_TBLFILE_R,
	g_param_spec_string("tablefile-r", "Table file R",
	    "transform table for left image", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_VSHADER_FILE,
	g_param_spec_string("vertex", "vertex shader filename",
	    "Alternative vertex shader", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_FSHADER_FILE,
	g_param_spec_string("fragment", "fragment shader filename",
	    "Alternative fragment shader", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_DISABLE_STITCH,
	g_param_spec_boolean("disable-stitch", "disable stitching",
	    "Disable fisheye to equirectanguler transformation", FALSE,
	    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_thetatransform_init (GstThetatransform *thetatransform)
{
    GST_DEBUG_OBJECT (thetatransform, "init");

    thetatransform->shader = NULL;
    thetatransform->vtx.x_count = 121;
    thetatransform->vtx.y_count = 61;
    thetatransform->vao = 0;
    thetatransform->tbl_file_L = NULL;
    thetatransform->tbl_file_R = NULL;
    thetatransform->vs_file = NULL;
    thetatransform->fs_file = NULL;
    thetatransform->rotation[0] = 0.;
    thetatransform->rotation[1] = -90.;
    thetatransform->rotation[2] = 0.;
}

static void
gst_thetatransform_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
    GstThetatransform *thetatransform = GST_THETATRANSFORM (object);

    GST_DEBUG_OBJECT (thetatransform, "set_property");

    switch (property_id) {
    case PROP_ROT_X:
	thetatransform->rotation[0] = g_value_get_float(value);
	break;
    case PROP_ROT_Y:
	thetatransform->rotation[1] = g_value_get_float(value);
	break;
    case PROP_ROT_Z:
	thetatransform->rotation[2] = g_value_get_float(value);
	break;
    case PROP_TBLFILE_L:
	thetatransform->tbl_file_L = g_strdup(g_value_get_string(value));
	break;
    case PROP_TBLFILE_R:
	thetatransform->tbl_file_R = g_strdup(g_value_get_string(value));
	break;
    case PROP_VSHADER_FILE:
	thetatransform->vs_file = g_strdup(g_value_get_string(value));
	break;
    case PROP_FSHADER_FILE:
	thetatransform->fs_file = g_strdup(g_value_get_string(value));
	break;
    case PROP_DISABLE_STITCH:
	thetatransform->skip_stitch = g_value_get_boolean(value);
	break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	break;
    }
}

static void
gst_thetatransform_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
    GstThetatransform *thetatransform = GST_THETATRANSFORM (object);

    GST_DEBUG_OBJECT (thetatransform, "get_property");

    switch (property_id) {
    case PROP_ROT_X:
	g_value_set_float(value, thetatransform->rotation[0]);
	break;
    case PROP_ROT_Y:
	g_value_set_float(value, thetatransform->rotation[1]);
	break;
    case PROP_ROT_Z:
	g_value_set_float(value, thetatransform->rotation[2]);
	break;
    case PROP_TBLFILE_L:
	g_value_set_string(value, thetatransform->tbl_file_L);
	break;
    case PROP_TBLFILE_R:
	g_value_set_string(value, thetatransform->tbl_file_R);
	break;
    case PROP_VSHADER_FILE:
	g_value_set_string(value, thetatransform->vs_file);
	break;
    case PROP_FSHADER_FILE:
	g_value_set_string(value, thetatransform->fs_file);
	break;
    case PROP_DISABLE_STITCH:
	g_value_set_boolean(value, thetatransform->skip_stitch);
	break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	break;
  }
}

static gboolean
gst_thetatransform_set_caps (GstGLFilter *filter, GstCaps *incaps, GstCaps *outcaps)
{
    GstThetatransform *thetatransform = GST_THETATRANSFORM (filter);

    GST_DEBUG_OBJECT (thetatransform, "set_caps");

    return TRUE;
}

static char *
load_program(GstThetatransform *thetatransform, char *fn)
{
    int fd, len, ret;
    gchar *buff;

    fd = open(fn, O_RDONLY);
    if (fd < 0) {
	GST_ERROR_OBJECT(thetatransform, "Can't open file \"%s\"", fn);
	return NULL;
    }
    len = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    buff = (char *)malloc(len+1);
    if (buff == NULL) {
	GST_ERROR_OBJECT(thetatransform, "memory allocation failed");
	close(fd);
	return NULL;
    }
    ret = read(fd, buff, len);
    buff[len+1]='\0';
    close(fd);

    return buff;
}

static gboolean
init_object(struct drawObject *ve)
{
    int vcnt, stride;
    int x, y, fr;
    short *vptr;

    stride = ve->x_count * 2;
    vcnt = ve->x_count * ve->y_count;
    ve->i_count = (ve->x_count - 1) * (ve->y_count - 1) * 6;
    ve->vertex = (float *)malloc(vcnt * sizeof(float)*2);
    ve->indices = (short *)malloc(ve->i_count * sizeof(short));

    fr = ve->x_count - 1;
    for (x = 0; x < ve->x_count; x++)
	ve->vertex[2*x] = -1.+2.0*x/fr;

    for (y = 1; y < ve->y_count; y++)
	memcpy(ve->vertex + stride * y, ve->vertex, stride * sizeof(float));

    fr = ve->y_count - 1;
    for (y = 0; y < ve->y_count; y++) {
	float yval;
	int offset;
	yval = 1. - 2.0 * y/fr;
	offset = stride * y + 1;
	for (x = 0; x < 2*ve->x_count; x+=2)
	    ve->vertex[offset + x] = yval;
    }

    vptr = ve->indices;
    for (y = 0; y < ve->x_count * (ve->y_count - 1); y+=ve->x_count) {
	for (x = y; x < y+ve->x_count - 1; x++) {
	    *vptr++ = x;
	    *vptr++ = x + ve->x_count + 1;
	    *vptr++ = x + 1;
	    *vptr++ = x;
	    *vptr++ = x + ve->x_count;
	    *vptr++ = x + ve->x_count + 1;
	}
    }
}

static gboolean
init_tbl(GstThetatransform *thetatransform, char *tblfile1, char *tblfile2, float aspectRatio)
{
    struct transTbl *tbl;
    GstGLFuncs *gl;
    float *buff, *ptr;
    GLuint tex, b;
    int fd;
    size_t stride, len, nbytes, idx, offset, nb;
    guint16 hdr[2];

    tbl = &thetatransform->tbl;
    buff = NULL;
    fd = open(tblfile1, O_RDONLY);
    if (fd < 0) {
	GST_ELEMENT_ERROR(thetatransform, RESOURCE, NOT_FOUND,
	    ("Transform table file \"%s\" not found", tblfile1),(NULL));
	return FALSE;
    }
    nb = read(fd, hdr, sizeof(hdr));
    if (nb != sizeof(hdr)) {
	close(fd);
	return FALSE;
    }
    nbytes = hdr[0] * hdr[1] * sizeof(float)*2;
    buff = (float *)malloc(nbytes * 2);
    if (!buff) {
	GST_ELEMENT_ERROR(thetatransform, RESOURCE, NOT_FOUND,
	    ("Can't allocate enough memory (%ld bytes)", nbytes*2), (NULL));
	close(fd);
	return FALSE;
    }
    nb = read(fd, buff, nbytes);
    close(fd);
    if (nb != nbytes) {
	GST_ELEMENT_ERROR(thetatransform, RESOURCE, READ,
	    ("invalid table file size %ld should be %ld", nb, nbytes ), (NULL));
	return FALSE;
    }

    offset = hdr[0]*hdr[1]*2;
    guint16 h[2];
    fd = open(tblfile2, O_RDONLY);
    if (fd < 0) {
	free(buff);
	return FALSE;
    }

    nb = read(fd, h, sizeof(h));
    nb = read(fd, buff+offset, nbytes);
    close(fd);

    if ((h[0] != hdr[0]) || (h[1] != hdr[1])) {
	free(buff);
	return FALSE;
    }
    
    tbl->data = (float*)malloc((nbytes+hdr[0]*sizeof(float)*2)*2);
    stride = hdr[0]*4;
    ptr = tbl->data;
    for (int line = 0; line < hdr[1]; line++) {
      idx = line*hdr[0]*2;
      for (int col = 0; col < hdr[0]; col++) {
	*ptr++ = buff[idx+col*2];
	*ptr++ = buff[idx+col*2+1]*aspectRatio;
	*ptr++ = buff[idx+offset+col*2];
	*ptr++ = buff[idx+offset+col*2+1]*aspectRatio;
      }
    }

    idx = (hdr[1]-1) * hdr[0] * 2 - 2;
    for (int col = 0; col < hdr[0]/2; col++) {
	*ptr++ = buff[idx - col*2];
	*ptr++ = buff[idx - col*2+1] * aspectRatio;
	*ptr++ = buff[idx+offset - col*2];
	*ptr++ = buff[idx+offset - col*2+1] * aspectRatio;
    }

    idx = (hdr[1]-1) * hdr[0] * 2 - (hdr[0]-1)-2;
    for (int col = 0; col < hdr[0]/2; col++) {
	*ptr++ = buff[idx - col*2];
	*ptr++ = buff[idx - col*2+1] * aspectRatio;
	*ptr++ = buff[idx+offset - col*2];
	*ptr++ = buff[idx+offset - col*2+1] * aspectRatio;
    }
    tbl->x_count = hdr[0];
    tbl->y_count = hdr[1] + 1;
    free(buff);

    return TRUE;
}

static void
load_tbl(GstThetatransform *thetatransform)
{
    GstGLFuncs *gl;
    float *buff, *tbl, *ptr;
    GLuint tex, b;
    size_t stride, len, nbytes, idx, offset;
    int fd;

    gl = GST_GL_BASE_FILTER(thetatransform)->context->gl_vtable;

    gl->GenTextures(1, &tex);
    gl->BindTexture(GL_TEXTURE_2D, tex);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, thetatransform->tbl.x_count,
	thetatransform->tbl.y_count, 0, GL_RGBA, GL_FLOAT, thetatransform->tbl.data);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    thetatransform->tid = tex;
}

static void
load_object(GstThetatransform *thetatransform)
{
    GstGLFuncs *gl;
    struct drawObject *d;
    GLuint vao, buff[2], pv;
    size_t sz;

    gst_gl_shader_use(thetatransform->shader);
    gl = GST_GL_BASE_FILTER(thetatransform)->context->gl_vtable;
    d = &(thetatransform->vtx);

    gl->GenVertexArrays(1, &vao);
    gl->GenBuffers(2, buff);

    gl->BindVertexArray(vao);

    gl->BindBuffer(GL_ARRAY_BUFFER, buff[0]);
    sz = d->x_count * d->y_count * sizeof(float) * 2;
    gl->BufferData(GL_ARRAY_BUFFER, sz, d->vertex, GL_STATIC_DRAW);

    gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, buff[1]);
    sz = d->i_count * sizeof(short);
    gl->BufferData(GL_ELEMENT_ARRAY_BUFFER, sz, d->indices, GL_STATIC_DRAW);

    pv = gst_gl_shader_get_attribute_location(thetatransform->shader, "pv");

    gl->VertexAttribPointer(pv, 2, GL_FLOAT, GL_FALSE, 0, 0);
    gl->EnableVertexAttribArray(pv);

    gl->BindVertexArray(0);

    gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    gl->BindBuffer(GL_ARRAY_BUFFER, 0);

    thetatransform->vao = vao;
}

static void
rotation(GstThetatransform *thetatransform)
{
    float s[3], c[3], *mat;
    int i;

    for (i = 0; i < 3; i++) {
	s[i] = sin(thetatransform->rotation[i] * M_PI / 180.);
	c[i] = cos(thetatransform->rotation[i] * M_PI / 180.);
    }

    mat = thetatransform->mat;
    mat[0] =  c[1] * c[2];
    mat[1] = -c[1] * s[2];
    mat[2] =  s[1];
    mat[3] =  s[0] * s[1] * c[2] + c[0] * s[2];
    mat[4] = -s[0] * s[1] * s[2] + c[0] * c[2];
    mat[5] = -s[0] * c[1];
    mat[6] = -c[0] * s[1] * c[2] + s[0] * s[2];
    mat[7] =  c[0] * s[1] * s[2] + s[0] * c[2];
    mat[8] =  c[0] * c[1];

}

static gboolean
gst_thetatransform_start (GstGLBaseFilter * filter)
{
    GstThetatransform *thetatransform = GST_THETATRANSFORM (filter);
    struct vertexElement *ve;
    int i;
    char *vs, *fs;
    gboolean ret;

    GST_DEBUG_OBJECT (thetatransform, "start");

    vs =  thetatransform->vs_file ? load_program(thetatransform, thetatransform->vs_file)
	: strdup(v_code);
    if (!vs)
	return FALSE;

    fs =  thetatransform->fs_file ? load_program(thetatransform, thetatransform->fs_file)
	: strdup(f_code);
    if (!fs)
	return FALSE;
    
    ret =  gst_gl_context_gen_shader(GST_GL_BASE_FILTER(filter)->context,
	vs, fs, &thetatransform->shader);

    if (vs != v_code)
	free(vs);
    if (fs != f_code)
	free(fs);
    if (!ret)
	return ret;

    init_object(&thetatransform->vtx);

    if (!thetatransform->skip_stitch) {
	if (thetatransform->tbl_file_L == NULL || thetatransform->tbl_file_R == NULL) {
	    GST_ELEMENT_ERROR(thetatransform, RESOURCE, NOT_FOUND,
		("Require transform table files"), (NULL));
	    return FALSE;
	}
	ret = init_tbl(thetatransform, thetatransform->tbl_file_L,
	    thetatransform->tbl_file_R, 960./1080.);
    } else {
	thetatransform->tbl.x_count=120;
	thetatransform->tbl.x_count=61;
	thetatransform->tbl.data = (float *)malloc(sizeof(float) * 61 * 120 * 2);
	
    }

    for (i = 0; i < 28; i++)
	thetatransform->gap[i] = 0.f;

    return ret;
}

static void
gst_thetatransform_stop (GstGLBaseFilter * filter)
{
    GstThetatransform *thetatransform = GST_THETATRANSFORM (filter);
    GstGLFuncs *gl;

    gl = filter->context->gl_vtable;

    GST_DEBUG_OBJECT (thetatransform, "stop");

    if (thetatransform->vao) {
	gl->DeleteVertexArrays(1, &thetatransform->vao);
	thetatransform->vao = 0;
    }

    if (thetatransform->tid) {
	gl->DeleteTextures(1, &thetatransform->tid);
	thetatransform->tid = 0;
    }

    if (thetatransform->shader) {
	gst_object_unref(thetatransform->shader);
	thetatransform->shader = NULL;
    }

    GST_GL_BASE_FILTER_CLASS(parent_class)->gl_stop(filter);
}

static gboolean
gst_thetatransform_filter (GstGLFilter *filter, GstBuffer *inbuf, GstBuffer *outbuf)
{
    gst_object_sync_values (GST_OBJECT (filter), GST_BUFFER_PTS(inbuf));

    return gst_gl_filter_filter_texture(filter, inbuf, outbuf);
}

static gboolean
gst_thetatransform_filter_texture (GstGLFilter *filter, GstGLMemory *intex, GstGLMemory *outtex)
{
    GstThetatransform *thetatransform;

    GST_DEBUG_OBJECT (thetatransform, "filter_texture");

    thetatransform = GST_THETATRANSFORM (filter);

    thetatransform->in_tex = intex;

    return gst_gl_framebuffer_draw_to_texture(filter->fbo, outtex, draw, thetatransform);
}

static gboolean
draw(gpointer ptr)
{
    GstGLFilter *filter;
    GstThetatransform *thetatransform;
    GstGLFuncs *gl;
    GstGLShader *shader;

    filter = GST_GL_FILTER(ptr);
    thetatransform = GST_THETATRANSFORM(filter);
    shader = thetatransform->shader;
    gl = GST_GL_BASE_FILTER(filter)->context->gl_vtable;

    gl->ClearColor(1., 0., 0., 0.);
    gl->Clear(GL_COLOR_BUFFER_BIT);

    gst_gl_shader_use(thetatransform->shader);
    if (!thetatransform->vao) {
	load_object(thetatransform);
	load_tbl(thetatransform);
	free(thetatransform->vtx.vertex);
	free(thetatransform->vtx.indices);
    }

    rotation(thetatransform);

    gl->ActiveTexture(GL_TEXTURE0);
    gl->BindTexture(GL_TEXTURE_2D, thetatransform->in_tex->tex_id);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);

    gl->ActiveTexture(GL_TEXTURE1);
    gl->BindTexture(GL_TEXTURE_2D, thetatransform->tid);

    gst_gl_shader_set_uniform_1i(shader, "image", 0);
    gst_gl_shader_set_uniform_1i(shader, "tbl", 1);
    gst_gl_shader_set_uniform_matrix_3fv(shader, "rmat", 1, GL_TRUE, thetatransform->mat);
    gst_gl_shader_set_uniform_2fv(shader, "gap", 14, thetatransform->gap);
    gst_gl_shader_set_uniform_1i(shader, "skip_stitch", thetatransform->skip_stitch);
    gl->BindVertexArray(thetatransform->vao);
    gl->DrawElements(GL_TRIANGLES, thetatransform->vtx.i_count, GL_UNSIGNED_SHORT,  0);

    gl->BindVertexArray(0);
    
    return TRUE;
}
