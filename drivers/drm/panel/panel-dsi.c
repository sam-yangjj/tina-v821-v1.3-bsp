// SPDX-License-Identifier: GPL-2.0+
/*
 * Generic dsi panel driver
 *
 * Copyright (C) 2023 Allwinner.
 *
 */

#include <linux/of_graph.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <video/display_timing.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>

#include <drm/drm_panel.h>
#include <drm/drm_print.h>
#include <drm/drm_connector.h>
#include <drm/drm_modes.h>
#include <drm/drm_edid.h>
#include <drm/drm_property.h>
#include <linux/backlight.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <video/mipi_display.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_crtc.h>
#include "panels.h"

struct panel_cmd_header {
	u8 data_type;
	u8 delay;
	u8 payload_length;
} __packed;

struct panel_cmd_desc {
	struct panel_cmd_header header;
	u8 *payload;
};

struct panel_cmd_seq {
	struct panel_cmd_desc *cmds;
	unsigned int cmd_cnt;
};

struct panel_desc {
	const struct display_timings *timings;
	struct {
		unsigned int width;
		unsigned int height;
	} size;

	unsigned int reset_num;
	 struct {
		unsigned int power;
		unsigned int enable;
		unsigned int reset;
	} delay;

	struct panel_cmd_seq *init_seq;
	struct panel_cmd_seq *exit_seq;
};

struct panel_dsi {
	struct drm_panel panel;
	struct device *dev;
	struct device *panel_dev;
	struct mipi_dsi_device *dsi;

	const struct panel_desc *desc;
	unsigned int bus_format;

	unsigned int power_num;
	unsigned int gpio_num;
	struct regulator *supply[10];
	struct regulator *avdd_supply;
	struct regulator *avee_supply;
	unsigned int avdd_output_voltage;
	unsigned int avee_output_voltage;
	struct gpio_desc *enable_gpio[10];
	struct gpio_desc *reset_gpio;

	enum drm_panel_orientation orientation;
};

struct panel_desc_dsi {
	struct panel_desc desc;
	unsigned long flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;
};
/*
static const struct drm_display_mode auo_b080uan01_mode = {
	.clock = 154500,
	.hdisplay = 1200,
	.hsync_start = 1200 + 62,
	.hsync_end = 1200 + 62 + 4,
	.htotal = 1200 + 62 + 4 + 62,
	.vdisplay = 1920,
	.vsync_start = 1920 + 9,
	.vsync_end = 1920 + 9 + 2,
	.vtotal = 1920 + 9 + 2 + 8,
	.vrefresh = 60,
};

static const struct panel_desc_dsi auo_b080uan01 = {
	.desc = {
		.modes = &auo_b080uan01_mode,
		.bpc = 8,
		.size = {
			.width = 108,
			.height = 272,
		},
	},
	.flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS,
	.format = MIPI_DSI_FMT_RGB888,
	.lanes = 4,
};
*/
static inline struct panel_dsi *to_panel_dsi(struct drm_panel *panel)
{
	return container_of(panel, struct panel_dsi, panel);
}

static void panel_dsi_sleep(unsigned int msec)
{
	if (msec > 20)
		msleep(msec);
	else
		usleep_range(msec * 1000, (msec + 1) * 1000);
}

static struct device *sunxi_of_get_child_panel(struct device *dev)
{
	struct device_node *node = dev->of_node;
	struct device_node *panel_node;
	struct device_node *child_node;
	struct platform_device *pdev = NULL;
	struct device *child_panel_dev = NULL;
	u32 reg = 0;

	of_property_read_u32(node, "panel-out-reg", &reg);
	panel_node = of_graph_get_endpoint_by_regs(node, 1, reg);
	if (!panel_node) {
		DRM_ERROR("endpoint panel_node not fount\n");
		return NULL;
	}

	child_node = of_graph_get_remote_port_parent(panel_node);
	if (!child_node) {
		DRM_ERROR("node child_node not fount\n");
		child_panel_dev = NULL;
		goto PANEL_PUT;
	}

	pdev = of_find_device_by_node(child_node);
	if (!pdev) {
		DRM_ERROR("child_node platform device not fount\n");
		child_panel_dev = NULL;
		goto CHILD_PANEL_PUT;
	}

	child_panel_dev = &pdev->dev;
	platform_device_put(pdev);

CHILD_PANEL_PUT:
	of_node_put(child_node);
PANEL_PUT:
	of_node_put(panel_node);

	return child_panel_dev;
}
static int panel_dsi_cmd_seq(struct panel_dsi *dsi_panel,
		struct panel_cmd_seq *seq)
{
	struct device *dev = dsi_panel->panel.dev;
	struct mipi_dsi_device *dsi = dsi_panel->dsi;
	unsigned int i;
	int err;

	if (!seq)
		return -EINVAL;

	for (i = 0; i < seq->cmd_cnt; i++) {
		struct panel_cmd_desc *cmd = &seq->cmds[i];

		switch (cmd->header.data_type) {
		case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
		case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
		case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
		case MIPI_DSI_GENERIC_LONG_WRITE:
			err = mipi_dsi_generic_write(dsi, cmd->payload,
						cmd->header.payload_length);
			break;
		case MIPI_DSI_DCS_SHORT_WRITE:
		case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
		case MIPI_DSI_DCS_LONG_WRITE:
			err = mipi_dsi_dcs_write_buffer(dsi, cmd->payload,
					cmd->header.payload_length);
			break;
		default:
			dev_err(dev,
				"There are %d packets in total, the %d packet length is error!!!\n",
				seq->cmd_cnt, i);
			return -EINVAL;
		}

		if (err < 0)
			dev_err(dev, "failed to write dcs cmd: %d\n", err);

		if (cmd->header.delay)
			panel_dsi_sleep(cmd->header.delay);
	}

	return 0;
}

static int panel_dsi_get_modes(struct drm_panel *panel,
				struct drm_connector *connector)
{
	struct panel_dsi *dsi_panel = to_panel_dsi(panel);
	const struct panel_desc *desc = dsi_panel->desc;
	struct drm_display_mode *mode;
	unsigned int i, num = 0;

	for (i = 0; i < desc->timings->num_timings; i++) {
		struct videomode vm;
		const struct display_timing *timing;

		timing = display_timings_get(desc->timings, i);
		if (!timing) {
			dev_err(panel->dev, "problems parsing panel-timin: %d\n", i);
			continue;
		}
		videomode_from_timing(timing, &vm);
		mode = drm_mode_create(connector->dev);
		if (!mode) {
			dev_err(panel->dev, "failed to add mode %ux%u\n",
				timing->hactive.typ, timing->vactive.typ);
			continue;
		}
		drm_display_mode_from_videomode(&vm, mode);

		mode->type |= DRM_MODE_TYPE_DRIVER;
		if (desc->timings->native_mode == i)
			mode->type |= DRM_MODE_TYPE_PREFERRED;

		drm_mode_probed_add(connector, mode);
		num++;
	}
/*
	connector->display_info.width_mm = dsi_panel->desc->size.width;
	connector->display_info.height_mm = dsi_panel->desc->size.height;
	drm_display_info_set_bus_formats(&connector->display_info,
					&dsi_panel->desc->bus_format, 1);
	connector->display_info.bus_flags =
		dsi_panel->data_mirror ? DRM_BUS_FLAG_DATA_LSB_TO_MSB :
				DRM_BUS_FLAG_DATA_MSB_TO_LSB;
*/
	drm_connector_set_panel_orientation(connector, dsi_panel->orientation);

	return num;
}
/*
static int panel_dsi_get_timings(struct drm_panel *panel,
				unsigned int num_timings,
				struct display_timing *timings)
{
	struct panel_dsi *dsi_panel = to_panel_dsi(panel);
	unsigned int i;

	if (dsi_panel->desc->timings->num_timings < num_timings)
		num_timings = dsi_panel->desc->timings->num_timings;

	for (i = 0; i < num_timings; i++)
		timings[i] = display_timings_get(dsi_panel->desc->timings, i);

	return dsi_panel->desc->timings->num_timings;
}
*/

static int panel_dsi_disable(struct drm_panel *panel)
{
	if (panel->backlight)
		backlight_disable(panel->backlight);

	return 0;
}

static int panel_dsi_unprepare(struct drm_panel *panel)
{
	struct panel_dsi *dsi_panel = to_panel_dsi(panel);
	int i;

	if (dsi_panel->desc->exit_seq)
		if (dsi_panel->dsi)
			panel_dsi_cmd_seq(dsi_panel, dsi_panel->desc->exit_seq);

	for (i = dsi_panel->gpio_num; i > 0; i--) {
		if (dsi_panel->enable_gpio[i - 1]) {
			gpiod_set_value_cansleep(dsi_panel->enable_gpio[i - 1], 0);
			if (dsi_panel->desc->delay.enable)
				panel_dsi_sleep(dsi_panel->desc->delay.enable);
		}
	}

	if (dsi_panel->reset_gpio)
		gpiod_set_value_cansleep(dsi_panel->reset_gpio, 0);
	if (dsi_panel->desc->delay.reset)
		panel_dsi_sleep(dsi_panel->desc->delay.reset);

	if (dsi_panel->avdd_supply) {
		regulator_disable(dsi_panel->avdd_supply);
		if (dsi_panel->desc->delay.power)
				panel_dsi_sleep(dsi_panel->desc->delay.power);
	}
	if (dsi_panel->avee_supply) {
		regulator_disable(dsi_panel->avee_supply);
		if (dsi_panel->desc->delay.power)
				panel_dsi_sleep(dsi_panel->desc->delay.power);
	}

	for (i = dsi_panel->power_num; i > 0; i--) {
		if (dsi_panel->supply[i - 1]) {
			regulator_disable(dsi_panel->supply[i - 1]);
			if (dsi_panel->desc->delay.power)
				panel_dsi_sleep(dsi_panel->desc->delay.power);
		}
	}

	return 0;
}

int panel_dsi_regulator_enable(struct drm_panel *panel)
{
	struct panel_dsi *dsi_panel = to_panel_dsi(panel);
	int err, i;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
	panel->prepared = true;
#endif
	for (i = 0; i < dsi_panel->power_num; i++) {
		if (dsi_panel->supply[i]) {
			err = regulator_enable(dsi_panel->supply[i]);
			if (err < 0) {
				dev_err(dsi_panel->dev, "failed to enable supply%d: %d\n",
					i, err);
				return err;
			}
			if (dsi_panel->desc->delay.power)
				panel_dsi_sleep(dsi_panel->desc->delay.power);
		}
	}
	if (dsi_panel->avdd_supply) {
		err = regulator_enable(dsi_panel->avdd_supply);
		if (err < 0) {
			dev_err(dsi_panel->dev, "failed to enable supply%d: %d\n",
				i, err);
			return err;
		}
		if (dsi_panel->avdd_output_voltage)
			regulator_set_voltage(dsi_panel->avdd_supply,
				dsi_panel->avdd_output_voltage, dsi_panel->avdd_output_voltage);
		if (dsi_panel->desc->delay.power)
			panel_dsi_sleep(dsi_panel->desc->delay.power);
	}

	if (dsi_panel->avee_supply) {
		err = regulator_enable(dsi_panel->avee_supply);
		if (err < 0) {
			dev_err(dsi_panel->dev, "failed to enable supply%d: %d\n",
				i, err);
			return err;
		}
		if (dsi_panel->avee_output_voltage)
			regulator_set_voltage(dsi_panel->avee_supply,
				dsi_panel->avee_output_voltage, dsi_panel->avee_output_voltage);
		if (dsi_panel->desc->delay.power)
			panel_dsi_sleep(dsi_panel->desc->delay.power);
	}

	return 0;
}
EXPORT_SYMBOL(panel_dsi_regulator_enable);

bool panel_dsi_is_support_backlight(struct drm_panel *panel)
{
	return panel->backlight;
}
EXPORT_SYMBOL(panel_dsi_is_support_backlight);

int panel_dsi_get_backlight_value(struct drm_panel *panel)
{
	if (panel->backlight)
		return backlight_get_brightness(panel->backlight);

	return 0;
}
EXPORT_SYMBOL(panel_dsi_get_backlight_value);

void panel_dsi_set_backlight_value(struct drm_panel *panel, int brightness)
{
	if (!panel->backlight || backlight_is_blank(panel->backlight) || brightness <= 0)
		return ;

	// TODO: support backlight mapping
	panel->backlight->props.brightness = brightness;
	backlight_update_status(panel->backlight);
}
EXPORT_SYMBOL(panel_dsi_set_backlight_value);

static int panel_dsi_prepare(struct drm_panel *panel)
{
	struct panel_dsi *dsi_panel = to_panel_dsi(panel);
	int i;

	panel_dsi_regulator_enable(panel);
	for (i = 0; i < dsi_panel->gpio_num; i++) {
		if (dsi_panel->enable_gpio[i]) {
			gpiod_set_value_cansleep(dsi_panel->enable_gpio[i], 1);

			if (dsi_panel->desc->delay.enable)
				panel_dsi_sleep(dsi_panel->desc->delay.enable);
		}
	}

	for (i = 0; i < dsi_panel->desc->reset_num; i++) {
		if (dsi_panel->reset_gpio)
			gpiod_set_value_cansleep(dsi_panel->reset_gpio, 0);
		if (dsi_panel->desc->delay.reset)
			panel_dsi_sleep(dsi_panel->desc->delay.reset);
		if (dsi_panel->reset_gpio)
			gpiod_set_value_cansleep(dsi_panel->reset_gpio, 1);
		if (dsi_panel->desc->delay.reset)
			panel_dsi_sleep(dsi_panel->desc->delay.reset);
	}

	if (dsi_panel->desc->init_seq)
		if (dsi_panel->dsi)
			panel_dsi_cmd_seq(dsi_panel, dsi_panel->desc->init_seq);

	return 0;
}


static int panel_dsi_enable(struct drm_panel *panel)
{
	if (panel->backlight)
		backlight_enable(panel->backlight);

	return 0;
}

static const struct drm_panel_funcs panel_dsi_funcs = {
	.unprepare = panel_dsi_unprepare,
	.disable = panel_dsi_disable,
	.prepare = panel_dsi_prepare,
	.enable = panel_dsi_enable,
	.get_modes = panel_dsi_get_modes,
//	.get_timings = panel_dsi_get_timings,
};

static int panel_dsi_parse_dt(struct panel_dsi *dsi_panel)
{
	struct device_node *np = dsi_panel->dev->of_node;
	char *power_name = NULL;
	char *gpio_name = NULL;
	int ret, i;

	ret = of_drm_get_panel_orientation(np, &dsi_panel->orientation);
	if (ret < 0) {
		dsi_panel->orientation = DRM_MODE_PANEL_ORIENTATION_NORMAL;
	}

	dsi_panel->power_num = 3;
	np = dsi_panel->panel_dev->of_node;
	of_property_read_u32(np, "power-num", &dsi_panel->power_num);
	for (i = 0; i < dsi_panel->power_num; i++) {
		power_name = kasprintf(GFP_KERNEL, "power%d", i);
		dsi_panel->supply[i] = devm_regulator_get_optional(dsi_panel->panel_dev, power_name);
		if (IS_ERR(dsi_panel->supply[i])) {
			ret = PTR_ERR(dsi_panel->supply[i]);

			if (ret != -ENODEV) {
				if (ret != -EPROBE_DEFER)
					dev_err(dsi_panel->dev,
						"failed to request regulator(%s): %d\n",
						power_name, ret);
				return ret;
			}

			dsi_panel->supply[i] = NULL;
		}
	}
	power_name = "avdd";
	dsi_panel->avdd_supply = devm_regulator_get_optional(dsi_panel->panel_dev, power_name);
	if (IS_ERR(dsi_panel->avdd_supply)) {
		ret = PTR_ERR(dsi_panel->avdd_supply);

		if (ret != -ENODEV) {
			if (ret != -EPROBE_DEFER)
				dev_err(dsi_panel->dev,
					"failed to request regulator(%s): %d\n",
					power_name, ret);
			return ret;
		}

		dsi_panel->avdd_supply = NULL;
	} else
		of_property_read_u32(np, "avdd-output-voltage", &dsi_panel->avdd_output_voltage);
	power_name = "avee";
	dsi_panel->avee_supply = devm_regulator_get_optional(dsi_panel->panel_dev, power_name);
	if (IS_ERR(dsi_panel->avee_supply)) {
		ret = PTR_ERR(dsi_panel->avee_supply);

		if (ret != -ENODEV) {
			if (ret != -EPROBE_DEFER)
				dev_err(dsi_panel->dev,
					"failed to request regulator(%s): %d\n",
					power_name, ret);
			return ret;
		}

		dsi_panel->avee_supply = NULL;
	} else
		of_property_read_u32(np, "avee-output-voltage", &dsi_panel->avee_output_voltage);


	/* Get GPIOs and backlight controller. */
	dsi_panel->gpio_num = 3;
	of_property_read_u32(np, "gpio-num", &dsi_panel->gpio_num);
	for (i = 0; i < dsi_panel->gpio_num; i++) {
		gpio_name = kasprintf(GFP_KERNEL, "enable%d", i);
		dsi_panel->enable_gpio[i] =
			devm_gpiod_get_optional(dsi_panel->panel_dev, gpio_name, GPIOD_OUT_HIGH);
		if (IS_ERR(dsi_panel->enable_gpio[i])) {
			ret = PTR_ERR(dsi_panel->enable_gpio[i]);
			dev_err(dsi_panel->dev, "failed to request %s GPIO: %d\n", gpio_name,
				ret);
			return ret;
		}
	}

	dsi_panel->reset_gpio =
		devm_gpiod_get_optional(dsi_panel->panel_dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(dsi_panel->reset_gpio)) {
		ret = PTR_ERR(dsi_panel->reset_gpio);
		dev_err(dsi_panel->dev, "failed to request %s GPIO: %d\n", "reset",
			ret);
		return ret;
	}

	return 0;
}
static int panel_simple_parse_cmd_seq(struct device *dev,
				const u8 *data, int length,
				struct panel_cmd_seq *seq)
{
	struct panel_cmd_header *header;
	struct panel_cmd_desc *desc;
	char *buf, *d;
	unsigned int i, cnt, len;

	if (!seq)
		return -EINVAL;

	buf = devm_kmemdup(dev, data, length, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	d = buf;
	len = length;
	cnt = 0;
	while (len > sizeof(*header)) {
		header = (struct panel_cmd_header *)d;

		d += sizeof(*header);
		len -= sizeof(*header);

		if (header->payload_length > len)
			return -EINVAL;

		d += header->payload_length;
		len -= header->payload_length;
		cnt++;
	}

	if (len) {
		dev_err(dev, "dts panel-init data length error:%d\n", len);
		return -EINVAL;
	}

	seq->cmd_cnt = cnt;
	seq->cmds = devm_kcalloc(dev, cnt, sizeof(*desc), GFP_KERNEL);
	if (!seq->cmds)
		return -ENOMEM;

	d = buf;
	len = length;
	for (i = 0; i < cnt; i++) {
		header = (struct panel_cmd_header *)d;
		len -= sizeof(*header);
		d += sizeof(*header);

		desc = &seq->cmds[i];
		desc->header = *header;
		desc->payload = d;

		d += header->payload_length;
		len -= header->payload_length;
	}

	return 0;
}

static int panel_of_get_desc_data(struct device *dev,
					struct panel_desc *desc, struct device_node *np)
{
	struct display_timings *timings = NULL;
	const void *data;
	int len;
	int err;

	timings = of_get_display_timings(np);
	if (!timings) {
		dev_err(dev, "%pOF: problems parsing panel-timin\n",
			np);
		return -ENODEV;
	}
	desc->timings = timings;

	of_property_read_u32(np, "width-mm", &desc->size.width);
	of_property_read_u32(np, "height-mm", &desc->size.height);

	of_property_read_u32(np, "power-delay-ms", &desc->delay.power);
	of_property_read_u32(np, "enable-delay-ms", &desc->delay.enable);
	of_property_read_u32(np, "reset-delay-ms", &desc->delay.reset);
	of_property_read_u32(np, "reset-num", &desc->reset_num);

	data = of_get_property(np, "panel-init-sequence", &len);
	if (data) {
		desc->init_seq = devm_kzalloc(dev, sizeof(*desc->init_seq),
					GFP_KERNEL);
		if (!desc->init_seq)
			return -ENOMEM;

		err = panel_simple_parse_cmd_seq(dev, data, len,
						 desc->init_seq);
		if (err) {
			dev_err(dev, "failed to parse init sequence\n");
			return err;
		}
	}

	data = of_get_property(np, "panel-exit-sequence", &len);
	if (data) {
		desc->exit_seq = devm_kzalloc(dev, sizeof(*desc->exit_seq),
					GFP_KERNEL);
		if (!desc->exit_seq)
			return -ENOMEM;

		err = panel_simple_parse_cmd_seq(dev, data, len,
						desc->exit_seq);
		if (err) {
			dev_err(dev, "failed to parse exit sequence\n");
			return err;
		}
	}

	return 0;
}

static int panel_dsi_of_get_desc_data(struct device *dev,
					struct panel_desc_dsi *desc, struct device_node *np)
{
	u32 val;
	int ret;

	ret = panel_of_get_desc_data(dev, &desc->desc, np);
	if (ret)
		return ret;

	if (!of_property_read_u32(np, "dsi,flags", &val))
		desc->flags = val;
	if (!of_property_read_u32(np, "dsi,format", &val))
		desc->format = val;
	if (!of_property_read_u32(np, "dsi,lanes", &val))
		desc->lanes = val;

	return 0;
}

static const struct of_device_id dsi_of_match[] = {
	{
		.compatible = "allwinner,virtual-panel",
		.data = NULL,
	},
	{ /* Sentinel */ },
};

static const struct of_device_id platform_of_match[] = {
	{
		.compatible = "allwinner,panel-dsi",
		.data = NULL,
	},
	{ /* Sentinel */ },
};

static int panel_probe(struct platform_device *pdev)
{
	return 0;
}

static int panel_dsi_probe(struct mipi_dsi_device *dsi)
{
	struct panel_dsi *dsi_panel;
	struct device *dev = &dsi->dev, *panel_dev;
	struct device_driver *panel_drv;
	const struct panel_desc_dsi *desc;
	struct panel_desc_dsi *d;
	const struct of_device_id *id;
	struct device_node *np = NULL;
	int ret;

	DRM_WARN("[DSI-PANEL] panel_dsi_probe start\n");
	dsi_panel = devm_kzalloc(dev, sizeof(*dsi_panel), GFP_KERNEL);
	if (!dsi_panel)
		return -ENOMEM;

	id = of_match_node(dsi_of_match, dsi->dev.of_node);

	panel_dev = sunxi_of_get_child_panel(dev);
	if (panel_dev) {
		panel_drv = panel_dev->driver;
		if (panel_drv && try_module_get(panel_drv->owner))
			module_put(panel_drv->owner);
		else {
			DRM_ERROR("[DSI-PANEL] panel-dsi driver not probe\n");
			return -EPROBE_DEFER;
		}

		np = panel_dev->of_node;
		dsi_panel->panel_dev = panel_dev;
	} else {
		dev_err(dev, "get child panel fail\n");
		np = dev->of_node;
		dsi_panel->panel_dev = dev;
	}

	if (!id->data) {
		d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
		if (!d)
		return -ENOMEM;

		ret = panel_dsi_of_get_desc_data(dev, d, np);
		if (ret) {
			dev_err(dev, "failed to get desc data: %d\n", ret);
			return ret;
		}
	}

	desc = id->data ? id->data : d;
	dsi_panel->dev = dev;
	dsi_panel->desc = &desc->desc;
	dsi_panel->dsi = dsi;

	dsi->mode_flags = desc->flags;
	dsi->format = desc->format;
	dsi->lanes = desc->lanes;

	ret = panel_dsi_parse_dt(dsi_panel);
	if (ret < 0)
		return ret;

	/* Register the panel. */
	drm_panel_init(&dsi_panel->panel, dev, &panel_dsi_funcs,
			DRM_MODE_CONNECTOR_DSI);

	/* Give the dev of panel-dsi to virtual-panel to obtain backlight,
	 * After obtaining it, restore the dev of virtual-panel.
	 */
	dsi_panel->panel.dev = dsi_panel->panel_dev;
	ret = drm_panel_of_backlight(&dsi_panel->panel);
	if (ret)
		return ret;
	dsi_panel->panel.dev = dev;

	drm_panel_add(&dsi_panel->panel);

	dev_set_drvdata(dev, dsi_panel);
	mipi_dsi_attach(dsi);
	DRM_WARN("[DSI-PANEL] panel_dsi_probe finish\n");

	return 0;
}

static int panel_simple_remove(struct device *dev)
{
	struct panel_dsi *panel = dev_get_drvdata(dev);

	drm_panel_remove(&panel->panel);

	panel_dsi_disable(&panel->panel);
	panel_dsi_unprepare(&panel->panel);

	return 0;
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
static int panel_dsi_remove(struct mipi_dsi_device *dsi)
#else
static void panel_dsi_remove(struct mipi_dsi_device *dsi)
#endif
{
/*	int err;

	err = mipi_dsi_detach(dsi);
	if (err < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", err);
*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return panel_simple_remove(&dsi->dev);
#else
	panel_simple_remove(&dsi->dev);
#endif
}

MODULE_DEVICE_TABLE(of, dsi_of_match);
static struct mipi_dsi_driver panel_dsi_driver = {
	.probe = panel_dsi_probe,
	.remove = panel_dsi_remove,
	.driver = {
		.name = "virtual-panel",
		.of_match_table = dsi_of_match,
	},
};

static struct platform_driver panel_simple_platform_driver = {
	.probe = panel_probe,
	.driver = {
		.name = "panel-dsi",
		.of_match_table = platform_of_match,
	},
};

static int __init panel_dsi_init(void)
{
	int err;

	DRM_WARN("[DSI-PANEL] panel_dsi_init start\n");
	err = platform_driver_register(&panel_simple_platform_driver);
	if (err < 0)
		return err;

	err = mipi_dsi_driver_register(&panel_dsi_driver);
	if (err < 0) {
		DRM_WARN("[DSI-PANEL] dsi driver regsiter fail\n");
		return err;
	}
	DRM_WARN("[DSI-PANEL] dsi driver regsiter finsh\n");

	return 0;
}
static void __exit panel_dsi_exit(void)
{
	mipi_dsi_driver_unregister(&panel_dsi_driver);

	platform_driver_unregister(&panel_simple_platform_driver);
}

module_init(panel_dsi_init);
module_exit(panel_dsi_exit);
//module_mipi_dsi_driver(panel_dsi_driver);

MODULE_AUTHOR("xiaozhineng <xiaozhineng@allwinnertech.com>");
MODULE_VERSION("1.0.0");
MODULE_DESCRIPTION("dsi Panel Driver");
MODULE_LICENSE("GPL");
