/*
 * pps-gmtimer.c -- PPS client driver using OMAP Timers
 *
 * ref1 - http://www.kunen.org/uC/beagle/omap_dmtimer.html
 * ref2 - [kernel] linux/samples/kobject/kobject-example.c
 * ref3 - [kernel] linux/arch/arm/mach-omap2/timer.c
 * ref4 - am335x trm on ti's website
 * ref5 - linux/drivers/pps/clients/pps-gpio.c
 *
 * required config - CONFIG_OF
 *
 * TODO - power management?
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/device.h>

#include <plat/dmtimer.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dan Drown");
MODULE_DESCRIPTION("PPS Client Driver using OMAP Timers");

#define MODULE_NAME "pps-gmtimer"

struct pps_gmtimer_platform_data {
  struct omap_dm_timer *capture_timer;
  const char *timer_name;
  uint32_t frequency;
  unsigned int capture;
  unsigned int overflow;
};

/* kobject *******************/
static ssize_t timer_name_show(struct device *dev, struct device_attribute *attr, char *buf) {
  struct pps_gmtimer_platform_data *pdata = dev->platform_data;
  return sprintf(buf, "%s\n", pdata->timer_name);
}

static DEVICE_ATTR(timer_name, S_IRUGO, timer_name_show, NULL);

static ssize_t stats_show(struct device *dev, struct device_attribute *attr, char *buf) {
  struct pps_gmtimer_platform_data *pdata = dev->platform_data;
  return sprintf(buf, "capture: %u\noverflow: %u\n", pdata->capture, pdata->overflow);
}

static DEVICE_ATTR(stats, S_IRUGO, stats_show, NULL);

static ssize_t capture2_show(struct device *dev, struct device_attribute *attr, char *buf) {
  struct pps_gmtimer_platform_data *pdata = dev->platform_data;
  return sprintf(buf, "%u\n",
      __omap_dm_timer_read(pdata->capture_timer, OMAP_TIMER_CAPTURE2_REG, pdata->capture_timer->posted)
      );
}

static DEVICE_ATTR(capture2, S_IRUGO, capture2_show, NULL);

static ssize_t capture_show(struct device *dev, struct device_attribute *attr, char *buf) {
  struct pps_gmtimer_platform_data *pdata = dev->platform_data;
  return sprintf(buf, "%u\n",
      __omap_dm_timer_read(pdata->capture_timer, OMAP_TIMER_CAPTURE_REG, pdata->capture_timer->posted)
      );
}

static DEVICE_ATTR(capture, S_IRUGO, capture_show, NULL);

static ssize_t ctrlstatus_show(struct device *dev, struct device_attribute *attr, char *buf) {
  struct pps_gmtimer_platform_data *pdata = dev->platform_data;
  return sprintf(buf, "%x\n",
      __omap_dm_timer_read(pdata->capture_timer, OMAP_TIMER_CTRL_REG, pdata->capture_timer->posted)
      );
}

static DEVICE_ATTR(ctrlstatus, S_IRUGO, ctrlstatus_show, NULL);

static ssize_t irqenabled_show(struct device *dev, struct device_attribute *attr, char *buf) {
  struct pps_gmtimer_platform_data *pdata = dev->platform_data;
  return sprintf(buf, "%x\n", __raw_readl(pdata->capture_timer->irq_ena));
}

static DEVICE_ATTR(irqenabled, S_IRUGO, irqenabled_show, NULL);

static ssize_t irqstatus_show(struct device *dev, struct device_attribute *attr, char *buf) {
  struct pps_gmtimer_platform_data *pdata = dev->platform_data;
  return sprintf(buf, "%x\n", omap_dm_timer_read_status(pdata->capture_timer));
}

static DEVICE_ATTR(irqstatus, S_IRUGO, irqstatus_show, NULL);

static ssize_t timer_counter_show(struct device *dev, struct device_attribute *attr, char *buf) {
  struct pps_gmtimer_platform_data *pdata = dev->platform_data;
  unsigned int current_count = 0;

  current_count = omap_dm_timer_read_counter(pdata->capture_timer);
  return sprintf(buf, "%u\n", current_count);
}

static DEVICE_ATTR(timer_counter, S_IRUGO, timer_counter_show, NULL);

static struct attribute *attrs[] = {
   &dev_attr_timer_counter.attr,
   &dev_attr_irqstatus.attr,
   &dev_attr_irqenabled.attr,
   &dev_attr_ctrlstatus.attr,
   &dev_attr_capture.attr,
   &dev_attr_capture2.attr,
   &dev_attr_stats.attr,
   &dev_attr_timer_name.attr,
   NULL,
};

static struct attribute_group attr_group = {
   .attrs = attrs,
};

/* timers ********************/
static irqreturn_t pps_gmtimer_interrupt(int irq, void *data) {
  struct pps_gmtimer_platform_data *pdata;

  pdata = data;

  if(pdata->capture_timer) {
    unsigned int irq_status;

    irq_status = omap_dm_timer_read_status(pdata->capture_timer);
    if(irq_status & OMAP_TIMER_INT_CAPTURE) {
      pdata->capture++;
      __omap_dm_timer_write_status(pdata->capture_timer, OMAP_TIMER_INT_CAPTURE);
    }
    if(irq_status & OMAP_TIMER_INT_OVERFLOW) {
      pdata->overflow++;
      __omap_dm_timer_write_status(pdata->capture_timer, OMAP_TIMER_INT_OVERFLOW);
    }
  }

  return IRQ_HANDLED; // TODO: shared interrupts?
}

static int pps_gmtimer_init_timer(struct device_node *timer_dn, struct pps_gmtimer_platform_data *pdata) {
  unsigned int current_count = 0;
  struct clk *gt_fclk;

  of_property_read_string_index(timer_dn, "ti,hwmods", 0, &pdata->timer_name);
  if (!pdata->timer_name) {
    printk(KERN_ERR MODULE_NAME ": ti,hwmods property missing?\n");
    return -ENODEV;
  }

  pdata->capture_timer = omap_dm_timer_request_by_node(timer_dn);
  if(!pdata->capture_timer) {
    printk(KERN_ERR MODULE_NAME ": request_by_node failed\n");
    return -ENODEV;
  }

  // TODO: use devm_request_irq?
  if(request_irq(pdata->capture_timer->irq, pps_gmtimer_interrupt, IRQF_TIMER, MODULE_NAME, pdata)) {
    printk(KERN_ERR MODULE_NAME ": cannot register IRQ %d\n", pdata->capture_timer->irq);
    return -EIO;
  }

  omap_dm_timer_set_source(pdata->capture_timer, OMAP_TIMER_SRC_SYS_CLK);
  omap_dm_timer_set_prescaler(pdata->capture_timer, 0);
  omap_dm_timer_set_load_start(pdata->capture_timer, 1, 0);

  __omap_dm_timer_int_enable(pdata->capture_timer, OMAP_TIMER_INT_CAPTURE|OMAP_TIMER_INT_OVERFLOW);

  gt_fclk = omap_dm_timer_get_fclk(pdata->capture_timer);
  pdata->frequency = clk_get_rate(gt_fclk);
  current_count = omap_dm_timer_read_counter(pdata->capture_timer);
  printk(KERN_INFO MODULE_NAME ": timer name=%s cc=%u rate=%u\n", pdata->timer_name, current_count, pdata->frequency);

  return 0;
}

static void pps_gmtimer_cleanup_timer(struct pps_gmtimer_platform_data *pdata) {
  unsigned int current_count = 0;

  if(pdata->capture_timer) {
    omap_dm_timer_set_int_disable(pdata->capture_timer, OMAP_TIMER_INT_CAPTURE|OMAP_TIMER_INT_OVERFLOW);
    free_irq(pdata->capture_timer->irq, pdata);
    current_count = omap_dm_timer_read_counter(pdata->capture_timer);
    omap_dm_timer_stop(pdata->capture_timer);
    omap_dm_timer_free(pdata->capture_timer);
    pdata->capture_timer = NULL;
    printk(KERN_INFO MODULE_NAME ": Exiting. count=%u\n", current_count);
  }
}

/* module ********************/
static struct pps_gmtimer_platform_data *of_get_pps_gmtimer_pdata(struct platform_device *pdev) {
  struct device_node *np = pdev->dev.of_node, *timer_dn;
  struct pps_gmtimer_platform_data *pdata;
  const __be32 *timer_phandle;

  pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
  if (!pdata)
    return NULL;

  timer_phandle = of_get_property(np, "timer", NULL);
  if(!timer_phandle) {
    printk(KERN_ERR MODULE_NAME ": timer property in devicetree null\n");
    goto fail;
  }

  timer_dn = of_find_node_by_phandle(be32_to_cpup(timer_phandle));
  if(!timer_dn) {
    printk(KERN_ERR MODULE_NAME ": find_node_by_phandle failed\n");
    goto fail;
  }

  if(pps_gmtimer_init_timer(timer_dn, pdata) < 0) {
    goto fail2;
  }

  of_node_put(timer_dn);

  return pdata;

fail2:
  of_node_put(timer_dn);
fail:
  devm_kfree(&pdev->dev, pdata);
  return NULL;
}

static const struct of_device_id pps_gmtimer_dt_ids[] = {
	{ .compatible = "pps-gmtimer", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, pps_gmtimer_dt_ids);

static int pps_gmtimer_probe(struct platform_device *pdev) {
  const struct of_device_id *match;
  struct pps_gmtimer_platform_data *pdata;

  match = of_match_device(pps_gmtimer_dt_ids, &pdev->dev);
  if (match) {
    pdev->dev.platform_data = of_get_pps_gmtimer_pdata(pdev);
  } else {
    printk(KERN_ERR MODULE_NAME ": of_match_device failed\n");
  }
  pdata = pdev->dev.platform_data;

  if(sysfs_create_group(&pdev->dev.kobj, &attr_group)) {
    printk(KERN_ERR MODULE_NAME ": sysfs_create_group failed\n");
  }

  if(!pdata)
    return -ENODEV;

  //TODO: pinctl, pps
  return 0;
}

static int pps_gmtimer_remove(struct platform_device *pdev) {
  struct pps_gmtimer_platform_data *pdata;
  pdata = pdev->dev.platform_data;

  if(pdata) {
    pps_gmtimer_cleanup_timer(pdata);
    devm_kfree(&pdev->dev, pdata);
    pdev->dev.platform_data = NULL;

    sysfs_remove_group(&pdev->dev.kobj, &attr_group);
  }

  platform_set_drvdata(pdev, NULL);

  return 0;
}

static struct platform_driver pps_gmtimer_driver = {
	.probe		= pps_gmtimer_probe,
	.remove		= pps_gmtimer_remove,
	.driver		= {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
		.of_match_table	= of_match_ptr(pps_gmtimer_dt_ids),
	},
};

module_platform_driver(pps_gmtimer_driver);
