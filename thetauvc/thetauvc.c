/*
 * Copyright 2020-2022 K. Takeo. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 * 3. Neither the name of the author nor other contributors may be
 * used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libusb.h>
#include "libuvc/libuvc.h"
#include "thetauvc.h"

static thetauvc_mode_t stream_mode[] = {
    {
     .mode = THETAUVC_MODE_UHD,
     .width = 3840,
     .height = 1920,
     .fps = 0,
     },
    {
     .mode = THETAUVC_MODE_FHD,
     .width = 1920,
     .height = 960,
     .fps = 0,
     },
    {
     .mode = THETAUVC_MODE_FHD_S,
     .width = 1920,
     .height = 1080,
     .fps = 0,
     },
    {
     .mode = THETAUVC_MODE_NUM,
     .width = 0,
     .height = 0,
     .fps = 0,
     }
};

static const char iProduct[] = "RICOH THETA";

uvc_error_t
thetauvc_find_devices(uvc_context_t * ctx, uvc_device_t *** devs)
{
    uvc_device_t **devlist, **ret;
    uvc_device_t *dev;
    uvc_error_t res;

    int     idx, devcnt;

    res = uvc_find_devices(ctx, &devlist, USBVID_RICOH, 0, NULL);
    if (res != UVC_SUCCESS) {
	return res;
    }

    ret = (uvc_device_t **) malloc(sizeof(uvc_device_t *));
    if (ret == NULL) {
	uvc_free_device_list(devlist, 1);
	return UVC_ERROR_NO_MEM;
    }
    *ret = NULL;

    for (idx = 0, devcnt = 0; (dev = devlist[idx]) != NULL; idx++) {
	uvc_device_descriptor_t *desc;

	if (uvc_get_device_descriptor(dev, &desc) != UVC_SUCCESS)
	    continue;

	if (desc->product && strncmp(desc->product, iProduct, strlen(iProduct)) == 0) {
	    void   *tmp_ptr;

	    devcnt++;

	    tmp_ptr = realloc(ret, (devcnt + 1) * sizeof(uvc_device_t *));
	    if (tmp_ptr == NULL) {
		uvc_free_device_list(devlist, 1);
		uvc_free_device_descriptor(desc);
		free(ret);
		return UVC_ERROR_NO_MEM;
	    }

	    ret = tmp_ptr;
	    ret[devcnt - 1] = dev;
	    ret[devcnt] = NULL;
	}
	uvc_free_device_descriptor(desc);
    }

    for (idx = 0; idx < devcnt; idx++)
	uvc_ref_device(ret[idx]);

    uvc_free_device_list(devlist, 1);

    if (devcnt) {
	*devs = ret;
	return UVC_SUCCESS;
    } else {
	free(ret);
	return UVC_ERROR_NO_DEVICE;
    }

}

uvc_error_t
thetauvc_print_devices(uvc_context_t * ctx, FILE * fp)
{
    uvc_device_t **devlist;
    uvc_error_t res;
    FILE   *outfp;
    int     idx;

    outfp = (fp == NULL) ? stdout : fp;
    res = thetauvc_find_devices(ctx, &devlist);
    if (res != UVC_SUCCESS) {
	uvc_perror(res, "");
	return res;
    }

    fprintf(outfp, "No : %-18s : %-10s\n", "Product", "Serial");
    for (idx = 0; devlist[idx] != NULL; idx++) {
	uvc_device_descriptor_t *desc;

	if (uvc_get_device_descriptor(devlist[idx], &desc) != UVC_SUCCESS)
	    continue;

	fprintf(outfp, "%2d : %-18s : %-10s\n", idx, desc->product,
		desc->serialNumber);
	uvc_free_device_descriptor(desc);
    }

    uvc_free_device_list(devlist, 1);

    return UVC_SUCCESS;
}

uvc_error_t
thetauvc_find_device(uvc_context_t * ctx, uvc_device_t ** devh,
		     unsigned int index)
{
    uvc_device_t **devlist;
    uvc_error_t res;
    unsigned int idx;

    res = thetauvc_find_devices(ctx, &devlist);
    if (res != UVC_SUCCESS)
	return res;

    for (idx = 0; idx <= index; idx++) {
	if (devlist[idx] == NULL) {
	    uvc_free_device_list(devlist, 1);
	    return UVC_ERROR_NO_DEVICE;
	}
    }

    uvc_ref_device(devlist[index]);
    *devh = devlist[index];
    uvc_free_device_list(devlist, 1);

    return UVC_SUCCESS;
}

uvc_error_t
thetauvc_find_device_by_serial(uvc_context_t * ctx, uvc_device_t ** devh,
			       const char *serial)
{
    uvc_device_t **devlist, *dev;
    uvc_error_t res;
    unsigned int idx;
    int     found;

    res = thetauvc_find_devices(ctx, &devlist);
    if (res != UVC_SUCCESS)
	return res;

    found = 0;
    if (serial == NULL) {
	dev = devlist[0];
	found = 1;
    } else {
	uvc_device_descriptor_t *desc;

	idx = 0;
	do {
	    dev = devlist[idx++];
	    if (dev == NULL) {
		res = UVC_ERROR_NO_DEVICE;
		break;
	    }

	    if (uvc_get_device_descriptor(dev, &desc) != UVC_SUCCESS)
		continue;

	    if (desc->serialNumber && !strcmp(desc->serialNumber, serial))
		found = 1;

	    uvc_free_device_descriptor(desc);

	} while (found != 1);
    }

    if (dev != NULL) {
	uvc_ref_device(dev);
	*devh = dev;
    }

    uvc_free_device_list(devlist, 1);
    return res;
}

uvc_error_t
thetauvc_get_stream_ctrl_format_size(uvc_device_handle_t * devh,
				     unsigned int mode,
				     uvc_stream_ctrl_t * ctrl,
				     thetauvc_mode_t *mode_val)
{
    uvc_error_t res;
    thetauvc_mode_t *m;
    int     i;

    if (!(mode < THETAUVC_MODE_NUM))
	return UVC_ERROR_INVALID_MODE;

    for (i = 0; i < THETAUVC_MODE_NUM; i++) {
	if (stream_mode[i].mode == mode)
	    m = &stream_mode[i];
    }

    memcpy(mode_val, m, sizeof(thetauvc_mode_t));
    res = uvc_get_stream_ctrl_format_size(devh, ctrl,
					  UVC_FRAME_FORMAT_H264, m->width,
					  m->height, m->fps);

    return res;
}

uvc_error_t
thetauvc_switch_configuration(uint8_t bus, uint8_t addr, uint8_t ncfg)
{
    libusb_context *ctx;
    libusb_device_handle *devh;
    libusb_device **udev, *dev;
    int cfg, sz, i;

    libusb_init(&ctx);
    sz = libusb_get_device_list(ctx, &udev);
    for (i = 0; i < sz; i++) {
	dev = udev[i];
	if (libusb_get_bus_number(dev) != bus)
	    continue;
	if (libusb_get_device_address(dev) == addr)
	    break;
    }

    if (i == sz)
        return UVC_ERROR_INVALID_PARAM;

    libusb_open(dev, &devh);
    libusb_get_configuration(devh, &cfg);

    if (cfg != ncfg) {
        libusb_detach_kernel_driver(devh, 0);
	libusb_detach_kernel_driver(devh, 2);
	libusb_detach_kernel_driver(devh, 3);
	libusb_set_configuration(devh, ncfg);
    }

    libusb_close(devh);
    libusb_free_device_list(udev, 1);
    libusb_exit(ctx);

    return UVC_SUCCESS;
}
