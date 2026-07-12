/*
 *    xfz1986 USB Graphic Display Linux Driver
 *
 *    Copyright (C) 2019 - 2025
 *    This file is licensed under the GPL. See LICENSE in the package.
 *
 *    Reference: linux/drivers/gpu/drm/tiny/gm12u320.c (Hans de Goede)
 *
 *   ---------------------------------------------------
 *    DRM/KMS Display Handlers (replaces legacy fbdev)
 *    Uses drm_simple_display_pipe + drm_gem_shmem_helper
 *    Target kernel: 5.4
 */

#include "inc/base_type.h"
#include "inc/usb_transfer.h"
#include "inc/drm_handlers.h"
#include "inc/log.h"
#include "inc/udisp_config.h"

#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_state_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_damage_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_edid.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_file.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_gem_shmem_helper.h>
#include <drm/drm_modes.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_simple_kms_helper.h>
#include <linux/version.h>
#include <drm/drm_gem_shmem_helper.h>
extern int debug_level;
extern int scale_thd;

/* Downscale 2:1 — samples every other pixel, reduces w*h/4 total */
void scale_for_sample_2to1(uint32_t *dst, uint32_t *src, int w, int h)
{
	LOGD("%s %d\n", __func__, __LINE__);
	int i = 0, j = 0;
	int line = w;
	int len = w * h;

	for (i = 0; i < len / 4; i++) {
		*dst++ = *src++;
		src++;
		j += 2;
		if (j >= line) {
			j = 0;
			src += line;
		}
	}
}

/* ------------------------------------------------------------------ */
/* Device accessor                                                     */

#define to_xfz1986_drm_device(__dev) \
	container_of(__dev, struct xfz1986_drm_device, dev)

#define DRIVER_NAME	"xfz1986_udisp"
#define DRIVER_DESC	"xfz1986 USB Graphic Display Driver"
#define DRIVER_DATE	"2025"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	0

/* ------------------------------------------------------------------ */
/* Connector - provides display mode matching device resolution        */

static int xfz1986_drm_conn_get_modes(struct drm_connector *connector)
{
	LOGD("%s %d\n", __func__, __LINE__);
	struct xfz1986_drm_device *xdev = to_xfz1986_drm_device(connector->dev);
	struct drm_display_mode *mode;
	struct drm_device *dev = connector->dev;
	int w, h;

	w = xdev->w;
	h = xdev->h;
	if (w <= scale_thd) {
		w *= 2;
		h *= 2;
	}

	mode = drm_mode_create(dev);
	if (!mode)
		return 0;

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	mode->hdisplay = w;
	mode->hsync_start = w + 16;
	mode->hsync_end = w + 16 + 64;
	mode->htotal = w + 16 + 64 + 16;
	mode->vdisplay = h;
	mode->vsync_start = h + 4;
	mode->vsync_end = h + 4 + 4;
	mode->vtotal = h + 4 + 4 + 4;
	mode->clock = mode->htotal * mode->vtotal * 60 / 1000;

	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_connector_helper_funcs xfz1986_drm_conn_helper_funcs = {
	.get_modes = xfz1986_drm_conn_get_modes,
};

static const struct drm_connector_funcs xfz1986_drm_conn_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

/* ------------------------------------------------------------------ */
/* Frame update work - encodes framebuffer and sends via USB           */

static void xfz1986_drm_fb_update_work(struct work_struct *work)
{
	LOGD("%s %d\n", __func__, __LINE__);
	struct xfz1986_drm_device *xdev =
		container_of(to_delayed_work(work), struct xfz1986_drm_device,
			     fb_update.work);
	struct drm_framebuffer *fb;
	void *vaddr;
	pixel_type_t *src;
	int w, h, fb_line_width;
	int ret, total_bytes = 0;
	u64 a, b, c, d;
	bool scaled = false;
	urb_itm_t *purb = NULL;

	mutex_lock(&xdev->fb_update.lock);
	fb = xdev->fb_update.fb;
	if (!fb)
		goto unlock;

	/* Cached vaddr from shadow-plane mapping — read under lock */
	vaddr = xdev->fb_update.src_map.vaddr;
	LOGD("%s: got fb %dx%d, pitches[0]=%d, vaddr=%p\n",
	     __func__, fb->width, fb->height, fb->pitches[0], vaddr);

	ret = drm_gem_fb_begin_cpu_access(fb, DMA_FROM_DEVICE);
	if (ret) {
		LOGE("drm_gem_fb_begin_cpu_access err: %d\n", ret);
		goto put_fb;
	}

	w = xdev->w;
	h = xdev->h;
	fb_line_width = fb->pitches[0] / 4;

	/* Downscale 2:1 if framebuffer is 2x the device resolution */
	if (fb->width == w * 2 && fb->height == h * 2) {
		scale_for_sample_2to1(xdev->scale_buf, (uint32_t *)vaddr,
				      fb->width, fb->height);
		src = xdev->scale_buf;
		fb_line_width = w;
		scaled = true;
		LOGD("drm: downscaled %dx%d to %dx%d\n",
		     fb->width, fb->height, w, h);
	} else {
		src = (pixel_type_t *)vaddr;
	}

	/* Encode under lock — writes to pre-allocated URB buffer */
	LOGD("%s: encoding %dx%d, line_width=%d, is_alive=%d\n",
	     __func__, w, h, fb_line_width, xdev->is_alive);
	a = get_os_us();
	if (xdev->is_alive) {
		purb = get_one_urb_item(xdev);
		if (purb) {
			b = get_os_us();
			total_bytes = xdev->encoder->enc(
				xdev->encoder,
				&purb->urb->transfer_buffer[sizeof(udisp_frame_header_t)],
				(uint8_t *)src, 0, 0, w - 1, h - 1,
				fb_line_width);
			c = get_os_us();
			xdev->encoder->enc_header(
				xdev->encoder,
				purb->urb->transfer_buffer,
				0, 0, w - 1, h - 1, total_bytes);
			total_bytes += sizeof(udisp_frame_header_t);
		}
	}

	drm_gem_fb_end_cpu_access(fb, DMA_FROM_DEVICE);

put_fb:
	drm_framebuffer_put(fb);
	xdev->fb_update.fb = NULL;
unlock:
	mutex_unlock(&xdev->fb_update.lock);

	/* USB send outside lock (same pattern as gm12u320) */
	if (purb) {
		usb_send_msg_async(xdev, purb,
				   purb->urb->transfer_buffer,
				   total_bytes);
		LOGD("%s: urb submitted, total_bytes=%d\n", __func__, total_bytes);

		d = get_os_us();
		put_fps_data(&xdev->fps_mgr, d);
		LOGM("drm fb: %d bytes fps:%d enc:%lld send:%lld total:%lld\n",
		     total_bytes + (int)sizeof(udisp_frame_header_t),
		     ((int)get_fps(&xdev->fps_mgr)) / 10,
		     (long long)(c - b), (long long)(d - c),
		     (long long)(d - a));
	}
}

/* ------------------------------------------------------------------ */
/* Mark framebuffer dirty, accumulate rect, schedule update work      */

static void xfz1986_drm_fb_mark_dirty(struct xfz1986_drm_device *xdev,
				       struct drm_framebuffer *fb,
				       const struct dma_buf_map *map,
				       struct drm_rect *dirty)
{
	LOGD("%s %d\n", __func__, __LINE__);
	struct drm_framebuffer *old_fb = NULL;
	bool wakeup = false;

	mutex_lock(&xdev->fb_update.lock);

	if (xdev->fb_update.fb != fb) {
		LOGD("%s: new fb, dirty=[%d,%d,%d,%d]\n",
		     __func__, dirty->x1, dirty->y1, dirty->x2, dirty->y2);
		old_fb = xdev->fb_update.fb;
		drm_framebuffer_get(fb);
		xdev->fb_update.fb = fb;
		xdev->fb_update.rect = *dirty;
		xdev->fb_update.src_map = *map;
		wakeup = true;
	} else {
		struct drm_rect *rect = &xdev->fb_update.rect;

		LOGD("%s: accumulate dirty=[%d,%d,%d,%d] + [%d,%d,%d,%d]\n",
		     __func__, rect->x1, rect->y1, rect->x2, rect->y2,
		     dirty->x1, dirty->y1, dirty->x2, dirty->y2);
		rect->x1 = min(rect->x1, dirty->x1);
		rect->y1 = min(rect->y1, dirty->y1);
		rect->x2 = max(rect->x2, dirty->x2);
		rect->y2 = max(rect->y2, dirty->y2);
	}

	mutex_unlock(&xdev->fb_update.lock);

	if (wakeup)
		mod_delayed_work(system_long_wq, &xdev->fb_update.work, 0);

	if (old_fb)
		drm_framebuffer_put(old_fb);
}

/* ------------------------------------------------------------------ */
/* Simple display pipe functions                                       */

static void xfz1986_drm_pipe_enable(struct drm_simple_display_pipe *pipe,
				     struct drm_crtc_state *crtc_state,
				     struct drm_plane_state *plane_state)
{
	LOGD("%s %d\n", __func__, __LINE__);
	struct xfz1986_drm_device *xdev =
		to_xfz1986_drm_device(pipe->crtc.dev);
	struct drm_shadow_plane_state *shadow_plane_state =
		to_drm_shadow_plane_state(plane_state);
	struct drm_rect rect = {
		.x1 = 0,
		.y1 = 0,
		.x2 = crtc_state->mode.hdisplay,
		.y2 = crtc_state->mode.vdisplay,
	};

	xfz1986_drm_fb_mark_dirty(xdev, plane_state->fb,
				  &shadow_plane_state->data[0], &rect);
}

static void xfz1986_drm_pipe_disable(struct drm_simple_display_pipe *pipe)
{
	LOGD("%s %d\n", __func__, __LINE__);
	struct xfz1986_drm_device *xdev =
		to_xfz1986_drm_device(pipe->crtc.dev);
	struct drm_framebuffer *old_fb;

	cancel_delayed_work_sync(&xdev->fb_update.work);

	mutex_lock(&xdev->fb_update.lock);
	old_fb = xdev->fb_update.fb;
	xdev->fb_update.fb = NULL;
	dma_buf_map_clear(&xdev->fb_update.src_map);
	mutex_unlock(&xdev->fb_update.lock);

	if (old_fb)
		drm_framebuffer_put(old_fb);
}

static void xfz1986_drm_pipe_update(struct drm_simple_display_pipe *pipe,
				     struct drm_plane_state *old_state)
{
	LOGD("%s %d\n", __func__, __LINE__);
	struct xfz1986_drm_device *xdev =
		to_xfz1986_drm_device(pipe->crtc.dev);
	struct drm_plane_state *state = pipe->plane.state;
	struct drm_shadow_plane_state *shadow_plane_state =
		to_drm_shadow_plane_state(state);
	struct drm_rect rect;

	if (drm_atomic_helper_damage_merged(old_state, state, &rect))
		xfz1986_drm_fb_mark_dirty(xdev, state->fb,
					  &shadow_plane_state->data[0], &rect);
}


static const struct drm_simple_display_pipe_funcs xfz1986_drm_pipe_funcs = {
	.enable	    = xfz1986_drm_pipe_enable,
	.disable    = xfz1986_drm_pipe_disable,
	.update	    = xfz1986_drm_pipe_update,
	DRM_GEM_SIMPLE_DISPLAY_PIPE_SHADOW_PLANE_FUNCS,
};

static const uint32_t xfz1986_drm_pipe_formats[] = {
	DRM_FORMAT_XRGB8888,
};

static const uint64_t xfz1986_drm_pipe_modifiers[] = {
	DRM_FORMAT_MOD_LINEAR,
	DRM_FORMAT_MOD_INVALID
};

/* ------------------------------------------------------------------ */
/* GEM prime import — ensure correct DMA device for USB                */

static struct drm_gem_object *
xfz1986_drm_gem_prime_import(struct drm_device *dev, struct dma_buf *dma_buf)
{
	LOGD("%s %d\n", __func__, __LINE__);
	struct xfz1986_drm_device *xdev = to_xfz1986_drm_device(dev);

	if (!xdev->dmadev)
		return ERR_PTR(-ENODEV);

	return drm_gem_prime_import_dev(dev, dma_buf, xdev->dmadev);
}

/* ------------------------------------------------------------------ */
/* DRM file operations and driver definition                           */

DEFINE_DRM_GEM_FOPS(xfz1986_drm_fops);

const struct drm_driver xfz1986_drm_driver = {
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_ATOMIC,

	.name		= DRIVER_NAME,
	.desc		= DRIVER_DESC,
	.date		= DRIVER_DATE,
	.major		= DRIVER_MAJOR,
	.minor		= DRIVER_MINOR,

	.fops		 = &xfz1986_drm_fops,
	DRM_GEM_SHMEM_DRIVER_OPS,
	.gem_prime_import = xfz1986_drm_gem_prime_import,
};

/* ------------------------------------------------------------------ */
/* Mode config functions                                                */

static const struct drm_mode_config_funcs xfz1986_mode_config_funcs = {
	.fb_create = drm_gem_fb_create_with_dirty,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

/* ------------------------------------------------------------------ */
/* DRM device initialization                                           */

int xfz1986_drm_device_init(struct xfz1986_drm_device *xdev)
{
	LOGD("%s %d\n", __func__, __LINE__);
	struct drm_device *dev = &xdev->dev;
	int mode_w, mode_h;
	int ret;

	mode_w = xdev->w;
	mode_h = xdev->h;
	if (mode_w <= scale_thd) {
		mode_w *= 2;
		mode_h *= 2;
		LOGI("drm: scaling enabled: %dx%d -> %dx%d\n",
		     xdev->w, xdev->h, mode_w, mode_h);
	}

	/* Pre-allocate scale buffer (native device resolution) */
	if (mode_w != xdev->w) {
		xdev->scale_buf = devm_kzalloc(dev->dev,
					       xdev->w * xdev->h * 4,
					       GFP_KERNEL);
		if (!xdev->scale_buf)
			return -ENOMEM;
	}

	INIT_DELAYED_WORK(&xdev->fb_update.work, xfz1986_drm_fb_update_work);
	mutex_init(&xdev->fb_update.lock);

	ret = drm_mode_config_init(dev);
	if (ret) {
		LOGE("drm: mode_config_init failed: %d\n", ret);
		return ret;
	}

	dev->mode_config.min_width = mode_w;
	dev->mode_config.max_width = mode_w;
	dev->mode_config.min_height = mode_h;
	dev->mode_config.max_height = mode_h;
	dev->mode_config.funcs = &xfz1986_mode_config_funcs;

	/* Connector */
	drm_connector_helper_add(&xdev->conn, &xfz1986_drm_conn_helper_funcs);
	ret = drm_connector_init(dev, &xdev->conn, &xfz1986_drm_conn_funcs,
				 DRM_MODE_CONNECTOR_DisplayPort);
	if (ret) {
		LOGE("drm: connector_init failed: %d\n", ret);
		return ret;
	}

	/* Simple display pipe */
	ret = drm_simple_display_pipe_init(
		dev, &xdev->pipe, &xfz1986_drm_pipe_funcs,
		xfz1986_drm_pipe_formats,
		ARRAY_SIZE(xfz1986_drm_pipe_formats),
		xfz1986_drm_pipe_modifiers,
		&xdev->conn);
	if (ret) {
		LOGE("drm: simple_display_pipe_init failed: %d\n", ret);
		return ret;
	}

	drm_mode_config_reset(dev);

	ret = drm_dev_register(dev, 0);
	if (ret) {
		LOGE("drm: dev_register failed: %d\n", ret);
		return ret;
	}

	drm_fbdev_generic_setup(dev, 32);

	LOGI("drm: device %dx%d registered as %s\n",
	     mode_w, mode_h, dev->driver->name);

	return 0;
}

void xfz1986_drm_device_fini(struct xfz1986_drm_device *xdev)
{
	LOGI("%s %d\n", __func__, __LINE__);
	struct drm_device *dev = &xdev->dev;

	/* Cancel any pending frame update work */
	cancel_delayed_work_sync(&xdev->fb_update.work);

	/* Drop any held framebuffer reference */
	mutex_lock(&xdev->fb_update.lock);
	if (xdev->fb_update.fb) {
		drm_framebuffer_put(xdev->fb_update.fb);
		xdev->fb_update.fb = NULL;
	}
	mutex_unlock(&xdev->fb_update.lock);

	drm_dev_unplug(dev);
	drm_atomic_helper_shutdown(dev);
	drm_dev_unregister(dev);
}
