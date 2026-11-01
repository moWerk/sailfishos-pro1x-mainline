// SPDX-License-Identifier: GPL-2.0-only
/*
 * Debug probe: the PMI632 QG keeps sampling while the SoC sleeps. Restart its
 * FIFO at suspend and print the FIFO averages at resume = the sleep current.
 * Register layout as the downstream qpnp-qg (qg-reg.h).
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/suspend.h>
#include <linux/timekeeping.h>

#define B		0x4800
#define STATUS2		(B + 0x09)
#define STATUS3		(B + 0x0a)
#define DATA_CTL1	(B + 0x41)
#define MODE_CTL1	(B + 0x43)
#define S2_CTL2		(B + 0x51)
#define S2_CTL3		(B + 0x52)
#define S3_IBAT_CTL1	(B + 0x5d)
#define S3_ENTRY	(B + 0x5e)
#define S3_EXIT		(B + 0x5f)
#define V_ACC		(B + 0x88)
#define I_ACC		(B + 0x8b)
#define ACC_CNT		(B + 0x8e)
#define V_FIFO		(B + 0x90)
#define I_FIFO		(B + 0xa0)

static struct regmap *map;
static u64 t_in;
static struct notifier_block nb;

static unsigned int rd(unsigned int r)
{
	unsigned int v = 0;

	regmap_read(map, r, &v);
	return v;
}

static void hold(bool on)
{
	regmap_update_bits(map, DATA_CTL1, BIT(0), 0);
	if (on)
		regmap_update_bits(map, DATA_CTL1, BIT(0), BIT(0));
}

static s64 i_ua(s32 raw) { return div_s64(152588LL * raw, 1000); }

static void report(void)
{
	unsigned int n, i, cnt, ctl2 = rd(S2_CTL2), ivl = rd(S2_CTL3) * 10;
	u8 v[2], c[2], a[3];
	s64 sum = 0, ua;
	u64 slept = ktime_get_boottime_ns() - t_in;

	hold(true);
	n = rd(STATUS3) & 0xf;
	for (i = 0; i < n; i++) {
		regmap_bulk_read(map, V_FIFO + 2 * i, v, 2);
		regmap_bulk_read(map, I_FIFO + 2 * i, c, 2);
		ua = i_ua(sign_extend32(c[0] | c[1] << 8, 15));
		sum += ua;
		pr_info("qgsleep fifo %u: %u uV %lld uA\n", i,
			(unsigned int)div_u64((u64)(v[0] | v[1] << 8) * 194637, 1000), ua);
	}
	cnt = rd(ACC_CNT);
	regmap_bulk_read(map, I_ACC, a, 3);
	hold(false);
	pr_info("qgsleep slept %llu ms: fifo %u entries x %u samples x %u ms, mean %lld uA; acc %u samples, mean %lld uA; status2 0x%02x\n",
		div_u64(slept, NSEC_PER_MSEC), n, 1u << ((ctl2 & 7) + 1), ivl,
		n ? div_s64(sum, n) : 0, cnt,
		cnt ? div_s64(i_ua(sign_extend32(a[0] | a[1] << 8 | a[2] << 16, 23)), cnt) : 0,
		rd(STATUS2));
}

static int pm_cb(struct notifier_block *b, unsigned long ev, void *p)
{
	if (ev == PM_SUSPEND_PREPARE) {
		hold(true);	/* 0 -> 1 clears and restarts the FIFO, as downstream */
		hold(false);
		t_in = ktime_get_boottime_ns();
	} else if (ev == PM_POST_SUSPEND) {
		report();
	}
	return NOTIFY_DONE;
}

static int __init qgsleep_init(void)
{
	struct device_node *np = of_find_compatible_node(NULL, NULL, "fxtec,pro1x-adc-battery");
	struct platform_device *pdev;

	if (!np)
		return -ENODEV;
	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev)
		return -ENODEV;
	map = dev_get_regmap(pdev->dev.parent, NULL);
	put_device(&pdev->dev);
	if (!map)
		return -ENODEV;
	pr_info("qgsleep cfg: status2 0x%02x status3 0x%02x data_ctl1 0x%02x mode_ctl1 0x%02x s2_ctl2 0x%02x s2_ctl3 0x%02x s3_ibat_ctl1 0x%02x s3_entry 0x%02x s3_exit 0x%02x acc_cnt %u\n",
		rd(STATUS2), rd(STATUS3), rd(DATA_CTL1), rd(MODE_CTL1), rd(S2_CTL2),
		rd(S2_CTL3), rd(S3_IBAT_CTL1), rd(S3_ENTRY), rd(S3_EXIT), rd(ACC_CNT));
	t_in = ktime_get_boottime_ns() - 20ULL * NSEC_PER_SEC;
	report();	/* what the FIFO held while awake (since the last clear) */
	nb.notifier_call = pm_cb;
	nb.priority = -100;
	t_in = ktime_get_boottime_ns();
	return register_pm_notifier(&nb);
}

static void __exit qgsleep_exit(void)
{
	unregister_pm_notifier(&nb);
}
module_init(qgsleep_init);
module_exit(qgsleep_exit);
MODULE_DESCRIPTION("PMI632 QG sleep current probe");
MODULE_LICENSE("GPL");
