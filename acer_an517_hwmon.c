// SPDX-License-Identifier: GPL-2.0-only
/* Acer Nitro AN517-41 WMI fan telemetry and control, BIOS V1.14.
 * No raw EC/memory access. Control is restricted to the tested BIOS.
 */
#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/hwmon.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <linux/wmi.h>

#define ACER_GAMING_GUID "7A4DDFE7-5B5D-40B4-8595-4408E0CC7F56"
#define FAN_COUNT 2

/* Load-time only: changing the floor must be an explicit administrator choice. */
static unsigned int min_fan_percent = 50;
module_param(min_fan_percent, uint, 0444);
MODULE_PARM_DESC(min_fan_percent,
 "Minimum manual fan target, 0..100 percent (default 50; lower values untested)");

struct an517_data {
	struct wmi_device *wdev;
	struct mutex lock;
	unsigned long last_update;
	bool valid;
	int rpm[FAN_COUNT];
	int error[FAN_COUNT];
	bool control;
	bool suspended;
	bool owned[FAN_COUNT];
	struct notifier_block reboot;
};

static const struct dmi_system_id supported_models[] = {
	{
		.ident = "Acer Nitro AN517-41",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Nitro AN517-41"),
		},
	},
	{ }
};

/* Caller serializes all WMI access with data->lock. */
static int an517_command(struct an517_data *data, u32 method, u64 input,
                         size_t size, size_t minimum, u64 *result)
{
 struct acpi_buffer in = { size, &input };
 struct acpi_buffer out = { ACPI_ALLOCATE_BUFFER, NULL };
 union acpi_object *obj;
 acpi_status status;
 int i, ret = -EIO;

 status = wmidev_evaluate_method(data->wdev, 0, method, &in, &out);
 if (ACPI_FAILURE(status))
  goto done;
 obj = out.pointer;
 if (!obj || obj->type != ACPI_TYPE_BUFFER ||
     obj->buffer.length < minimum || !obj->buffer.pointer)
  goto done;
 *result = 0;
 for (i = 0; i < min_t(u32, 8, obj->buffer.length); i++)
  *result |= (u64)obj->buffer.pointer[i] << (8 * i);
 if (!(*result & 0xff))
  ret = 0;
 done:
 kfree(out.pointer);
 return ret;
}

static int an517_get_mode(struct an517_data *data, int ch)
{
 u64 result;
 int ret = an517_command(data, 15, ch ? 8 : 1, 4, 8, &result);
 int mode;

 if (ret)
  return ret;
 mode = (result >> (ch ? 14 : 8)) & 3;
 return mode >= 1 && mode <= 3 ? mode : -EIO;
}

static int an517_set_mode(struct an517_data *data, int ch, int mode)
{
 u64 result, input = (ch ? 8 : 1) | ((u64)mode << (ch ? 22 : 16));
 int ret = an517_command(data, 14, input, 8, 8, &result);

 if (ret)
  return ret;
 ret = an517_get_mode(data, ch);
 return ret < 0 ? ret : (ret == mode ? 0 : -EIO);
}

static int an517_get_target(struct an517_data *data, int ch)
{
 u64 result;
 int ret = an517_command(data, 17, ch ? 4 : 1, 4, 2, &result);
 int percent;

 if (ret)
  return ret;
 percent = (result >> 8) & 0xff;
 return percent <= 100 ? percent : -EIO;
}

static int an517_set_target(struct an517_data *data, int ch, int percent)
{
 u64 result, input = (ch ? 4 : 1) | ((u64)percent << 8);
 int ret = an517_command(data, 16, input, 8, 4, &result);

 if (ret)
  return ret;
 ret = an517_get_target(data, ch);
 return ret < 0 ? ret : (ret == percent ? 0 : -EIO);
}

static int an517_restore_auto(struct an517_data *data)
{
 int i, ret, error = 0;

 for (i = 0; i < FAN_COUNT; i++) {
  if (!data->owned[i])
   continue;
  ret = an517_set_mode(data, i, 1);
  if (ret) {
   dev_err(&data->wdev->dev, "Failed to restore fan %d to Auto: %d\n", i + 1, ret);
   error = ret;
  } else {
   data->owned[i] = false;
  }
 }
 return error;
}

static int an517_read_rpm(struct wmi_device *wdev, int channel)
{
	u8 request[8] = { 1, channel ? 6 : 2 };
	struct acpi_buffer in = { sizeof(request), request };
	struct acpi_buffer out = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	acpi_status status;
	int ret = -EIO;

	/* Fixed read-only sensor method; no user-supplied method or payload. */
	status = wmidev_evaluate_method(wdev, 0, 0x05, &in, &out);
	if (ACPI_FAILURE(status))
		goto done;
	obj = out.pointer;
	if (!obj || obj->type != ACPI_TYPE_BUFFER ||
	    obj->buffer.length < 3 || !obj->buffer.pointer)
		goto done;
	if (obj->buffer.pointer[0])
		goto done;
	ret = obj->buffer.pointer[1] | (obj->buffer.pointer[2] << 8);
done:
	kfree(out.pointer);
	return ret;
}

static void an517_update_locked(struct an517_data *data)
{
	int i, ret;

	if (data->valid && time_before(jiffies, data->last_update + HZ))
		return;
	for (i = 0; i < FAN_COUNT; i++) {
		ret = an517_read_rpm(data->wdev, i);
		data->error[i] = ret < 0 ? ret : 0;
		if (ret >= 0)
			data->rpm[i] = ret;
	}
	data->last_update = jiffies;
	data->valid = true;
}

static umode_t an517_is_visible(const void *data,
			       enum hwmon_sensor_types type, u32 attr,
			       int channel)
{
	if (type == hwmon_fan && channel >= 0 && channel < FAN_COUNT &&
	    (attr == hwmon_fan_input || attr == hwmon_fan_label))
		return 0444;
	if (type == hwmon_pwm && channel >= 0 && channel < FAN_COUNT &&
	    ((const struct an517_data *)data)->control &&
	    (attr == hwmon_pwm_input || attr == hwmon_pwm_enable))
		return 0644;
	return 0;
}

static int an517_read(struct device *dev, enum hwmon_sensor_types type,
                      u32 attr, int channel, long *val)
{
 struct an517_data *data = dev_get_drvdata(dev);
 int ret = -EOPNOTSUPP;

 if (channel < 0 || channel >= FAN_COUNT)
  return -EOPNOTSUPP;
 mutex_lock(&data->lock);
 if (data->suspended) {
  ret = -EBUSY;
  goto done;
 }
 if (type == hwmon_fan && attr == hwmon_fan_input) {
  an517_update_locked(data);
  ret = data->error[channel];
  if (!ret)
   *val = data->rpm[channel];
 } else if (type == hwmon_pwm && data->control) {
  if (attr == hwmon_pwm_input) {
   ret = an517_get_target(data, channel);
   if (ret >= 0) {
    *val = DIV_ROUND_CLOSEST(ret * 255, 100);
    ret = 0;
   }
  } else if (attr == hwmon_pwm_enable) {
   ret = an517_get_mode(data, channel);
   if (ret >= 0) {
    *val = ret == 1 ? 2 : (ret == 2 ? 0 : 1);
    ret = 0;
   }
  }
 }
 done:
 mutex_unlock(&data->lock);
 return ret;
}

static int an517_write(struct device *dev, enum hwmon_sensor_types type,
                       u32 attr, int channel, long val)
{
 struct an517_data *data = dev_get_drvdata(dev);
 int ret = -EOPNOTSUPP, mode, prior_mode, target;

 if (type != hwmon_pwm || !data->control || channel < 0 || channel >= FAN_COUNT)
  return -EOPNOTSUPP;
 /* Preserve the configured floor in the standard 0..255 PWM scale. */
 if ((attr == hwmon_pwm_input && (val < 0 || val > 255 ||
      val < DIV_ROUND_CLOSEST(min_fan_percent * 255, 100))) ||
     (attr == hwmon_pwm_enable && (val < 0 || val > 2)))
  return -EINVAL;
 mutex_lock(&data->lock);
 if (data->suspended) {
  ret = -EBUSY;
  goto done;
 }
 if (attr == hwmon_pwm_input) {
  ret = an517_set_target(data, channel, DIV_ROUND_CLOSEST(val * 100, 255));
 } else if (attr == hwmon_pwm_enable) {
  mode = val == 2 ? 1 : (val == 0 ? 2 : 3);
  /* Manual sessions start at 100%, never at a stale low target. */
  data->owned[channel] = true;
  prior_mode = an517_get_mode(data, channel);
  ret = prior_mode < 0 ? prior_mode : 0;
  if (!ret && mode == 3) {
   target = an517_get_target(data, channel);
   ret = target < 0 ? target : 0;
   if (!ret && (prior_mode != 3 || target < min_fan_percent))
    ret = an517_set_target(data, channel, 100);
  }
  if (!ret && prior_mode != mode)
   ret = an517_set_mode(data, channel, mode);
  if (!ret && mode == 1)
   data->owned[channel] = false;
 }
 if (ret)
  an517_restore_auto(data);
 data->valid = false;
 done:
 mutex_unlock(&data->lock);
 return ret;
}

static int an517_read_string(struct device *dev, enum hwmon_sensor_types type,
			     u32 attr, int channel, const char **str)
{
	static const char * const labels[] = { "CPU Fan", "GPU Fan" };

	if (type != hwmon_fan || attr != hwmon_fan_label ||
	    channel < 0 || channel >= FAN_COUNT)
		return -EOPNOTSUPP;
	*str = labels[channel];
	return 0;
}

static const struct hwmon_ops an517_ops = {
	.is_visible = an517_is_visible,
	.read = an517_read,
	.write = an517_write,
	.read_string = an517_read_string,
};

static const struct hwmon_channel_info * const an517_info[] = {
	HWMON_CHANNEL_INFO(fan, HWMON_F_INPUT | HWMON_F_LABEL,
			  HWMON_F_INPUT | HWMON_F_LABEL),
	HWMON_CHANNEL_INFO(pwm, HWMON_PWM_INPUT | HWMON_PWM_ENABLE,
			  HWMON_PWM_INPUT | HWMON_PWM_ENABLE),
	NULL
};

static const struct hwmon_chip_info an517_chip = {
	.ops = &an517_ops,
	.info = an517_info,
};

static int an517_reboot(struct notifier_block *nb, unsigned long action, void *unused)
{
 struct an517_data *data = container_of(nb, struct an517_data, reboot);

 mutex_lock(&data->lock);
 data->suspended = true;
 an517_restore_auto(data);
 mutex_unlock(&data->lock);
 return NOTIFY_DONE;
}

static int an517_suspend(struct device *dev)
{
 struct an517_data *data = dev_get_drvdata(dev);
 int ret;

 mutex_lock(&data->lock);
 ret = an517_restore_auto(data);
 if (!ret)
  data->suspended = true;
 mutex_unlock(&data->lock);
 return ret;
}

static int an517_resume(struct device *dev)
{
 struct an517_data *data = dev_get_drvdata(dev);

 mutex_lock(&data->lock);
 data->valid = false;
 data->suspended = false;
 mutex_unlock(&data->lock);
 return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(an517_pm, an517_suspend, an517_resume);

static void an517_remove(struct wmi_device *wdev)
{
 struct an517_data *data = dev_get_drvdata(&wdev->dev);

 unregister_reboot_notifier(&data->reboot);
 mutex_lock(&data->lock);
 data->suspended = true;
 an517_restore_auto(data);
 mutex_unlock(&data->lock);
}

static int an517_probe(struct wmi_device *wdev, const void *context)
{
	struct an517_data *data;
	struct device *hwmon;
	int i, ret;

	if (min_fan_percent > 100)
		return dev_err_probe(&wdev->dev, -EINVAL,
			"min_fan_percent must be between 0 and 100\n");
	if (!dmi_check_system(supported_models))
		return -ENODEV;
	data = devm_kzalloc(&wdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->wdev = wdev;
	mutex_init(&data->lock);
	dev_set_drvdata(&wdev->dev, data);
	an517_update_locked(data);
	if (data->error[0] || data->error[1])
		return dev_err_probe(&wdev->dev, -EIO,
				     "Both fan sensor queries must succeed\n");
	data->control = dmi_match(DMI_BIOS_VERSION, "V1.14");
	for (i = 0; data->control && i < FAN_COUNT; i++)
		if (an517_get_mode(data, i) < 0 || an517_get_target(data, i) < 0)
			data->control = false;
	hwmon = devm_hwmon_device_register_with_info(&wdev->dev,
						    "acer_an517", data,
						    &an517_chip, NULL);
	if (IS_ERR(hwmon))
		return PTR_ERR(hwmon);
	data->reboot.notifier_call = an517_reboot;
	ret = register_reboot_notifier(&data->reboot);
	if (ret)
		return ret;
	dev_info(&wdev->dev, "CPU/GPU fan sensors registered; control=%d\n", data->control);
	return 0;
}

static const struct wmi_device_id an517_wmi_ids[] = {
	{ ACER_GAMING_GUID, NULL },
	{ }
};
MODULE_DEVICE_TABLE(wmi, an517_wmi_ids);

static struct wmi_driver an517_driver = {
	.driver = { .name = "acer_an517_hwmon", .pm = pm_sleep_ptr(&an517_pm) },
	.id_table = an517_wmi_ids,
	.probe = an517_probe,
	.remove = an517_remove,
};
module_wmi_driver(an517_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Fan RPM and WMI PWM control for Acer Nitro AN517-41");
MODULE_VERSION("0.2.1");
