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
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>

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
  struct kobject *pps_gmtimer_kobj;
};

struct pps_gmtimer_platform_data *kobj_pdata = NULL; // TODO: multi-dir kobj

/* kobject *******************/
static ssize_t timer_name_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
  if(!kobj_pdata) {
    return 0;
  }
  return sprintf(buf, "%s\n", kobj_pdata->timer_name);
}

static struct kobj_attribute timer_name_attr = __ATTR(timer_name, 0600, timer_name_show, NULL);

static ssize_t stats_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
  if(!kobj_pdata) {
    return 0;
  }
  return sprintf(buf, "capture: %u\noverflow: %u\n", kobj_pdata->capture, kobj_pdata->overflow);
}

static struct kobj_attribute stats_attr = __ATTR(stats, 0600, stats_show, NULL);

static ssize_t capture2_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
  if(!kobj_pdata) {
    return 0;
  }
  return sprintf(buf, "%u\n",
      __omap_dm_timer_read(kobj_pdata->capture_timer, OMAP_TIMER_CAPTURE2_REG, kobj_pdata->capture_timer->posted)
      );
}

static struct kobj_attribute capture2_attr = __ATTR(capture2, 0600, capture2_show, NULL);

static ssize_t capture_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
  if(!kobj_pdata) {
    return 0;
  }
  return sprintf(buf, "%u\n",
      __omap_dm_timer_read(kobj_pdata->capture_timer, OMAP_TIMER_CAPTURE_REG, kobj_pdata->capture_timer->posted)
      );
}

static struct kobj_attribute capture_attr = __ATTR(capture, 0600, capture_show, NULL);

static ssize_t ctrlstatus_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
  if(!kobj_pdata) {
    return 0;
  }
  return sprintf(buf, "%x\n",
      __omap_dm_timer_read(kobj_pdata->capture_timer, OMAP_TIMER_CTRL_REG, kobj_pdata->capture_timer->posted)
      );
}

static struct kobj_attribute ctrlstatus_attr = __ATTR(ctrlstatus, 0600, ctrlstatus_show, NULL);

static ssize_t irqenabled_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
  if(!kobj_pdata) {
    return 0;
  }
  return sprintf(buf, "%x\n", __raw_readl(kobj_pdata->capture_timer->irq_ena));
}

static struct kobj_attribute irqenabled_attr = __ATTR(irqenabled, 0600, irqenabled_show, NULL);

static ssize_t irqstatus_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
  if(!kobj_pdata) {
    return 0;
  }
  return sprintf(buf, "%x\n", omap_dm_timer_read_status(kobj_pdata->capture_timer));
}

static struct kobj_attribute irqstatus_attr = __ATTR(irqstatus, 0600, irqstatus_show, NULL);

static ssize_t timer_counter_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
  unsigned int current_count = 0;
  if(!kobj_pdata) {
    return 0;
  }

  current_count = omap_dm_timer_read_counter(kobj_pdata->capture_timer);
  return sprintf(buf, "%u\n", current_count);
}

static struct kobj_attribute timer_counter_attr = __ATTR(timer_counter, 0600, timer_counter_show, NULL);

static struct attribute *attrs[] = {
   &timer_counter_attr.attr,
   &irqstatus_attr.attr,
   &irqenabled_attr.attr,
   &ctrlstatus_attr.attr,
   &capture_attr.attr,
   &capture2_attr.attr,
   &stats_attr.attr,
   &timer_name_attr.attr,
   NULL,
};

static struct attribute_group attr_group = {
   .attrs = attrs,
};

static int pps_gmtimer_init_kobject(struct pps_gmtimer_platform_data *pdata) {
  int retval;

  if(kobj_pdata) { // only one at a time for now
    pdata->pps_gmtimer_kobj = NULL;
    return 0;
  }

  pdata->pps_gmtimer_kobj = kobject_create_and_add(MODULE_NAME, kernel_kobj);
  if (!pdata->pps_gmtimer_kobj) {
    printk(KERN_INFO MODULE_NAME ": kobject_create_and_add failed\n");
  } else {
    retval = sysfs_create_group(pdata->pps_gmtimer_kobj, &attr_group);
    if (retval) {
      printk(KERN_INFO MODULE_NAME ": sysfs_create_group failed: %d\n", retval);
      kobject_put(pdata->pps_gmtimer_kobj);
      pdata->pps_gmtimer_kobj = NULL;
    } else {
      kobj_pdata = pdata;
    }
  }

  return 0;
}

static void pps_gmtimer_cleanup_kobject(struct pps_gmtimer_platform_data *pdata) {
  if(kobj_pdata == pdata) {
    if(pdata->pps_gmtimer_kobj) {
      kobject_put(pdata->pps_gmtimer_kobj);
      pdata->pps_gmtimer_kobj = NULL;
    } else {
      printk(KERN_ERR MODULE_NAME ": this device is marked as the kobj, but has a null pps_gmtimer_kobj\n");
    }
    kobj_pdata = NULL;
  }
}

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

  if(pps_gmtimer_init_kobject(pdata) < 0) {
    goto fail;
  }

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
    printk(KERN_ERR MODULE_NAME ": of_match_device failed?\n");
  }
  pdata = pdev->dev.platform_data;

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
    pps_gmtimer_cleanup_kobject(pdata);
    devm_kfree(&pdev->dev, pdata);
    pdev->dev.platform_data = NULL;
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
