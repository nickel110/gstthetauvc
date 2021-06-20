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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_THETATRANSFORM_H_
#define _GST_THETATRANSFORM_H_

#include <gst/gl/gstglfilter.h>
#include <gst/gl/gstglfuncs.h>

G_BEGIN_DECLS

#define GST_TYPE_THETATRANSFORM   (gst_thetatransform_get_type())
#define GST_THETATRANSFORM(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_THETATRANSFORM,GstThetatransform))
#define GST_THETATRANSFORM_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_THETATRANSFORM,GstThetatransformClass))
#define GST_IS_THETATRANSFORM(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_THETATRANSFORM))
#define GST_IS_THETATRANSFORM_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_THETATRANSFORM))

typedef struct _GstThetatransform GstThetatransform;
typedef struct _GstThetatransformClass GstThetatransformClass;

struct drawObject
{
    unsigned int x_count, y_count;
    unsigned int i_count;
    float *vertex;
    short *indices;
};

struct transTbl
{
    short x_count, y_count;
    float *data;
};

struct _GstThetatransform
{
    GstGLFilter base_thetatransform;

    struct drawObject vtx;
    struct transTbl tbl;

    GstGLShader *shader;
    GstGLMemory *in_tex;

    GLfloat rotation[3];
    GLfloat mat[9], gap[28];
    GLuint vao, tid;
    gchar *tbl_file_L, *tbl_file_R;
    gchar *vs_file, *fs_file;
    gboolean skip_stitch;
};

struct _GstThetatransformClass
{
    GstGLFilterClass base_thetatransform_class;
};

GType gst_thetatransform_get_type (void);

G_END_DECLS

#endif
