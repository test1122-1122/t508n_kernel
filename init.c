#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <dirent.h>
#define __NR_t508n_log 441

/* 用户态 log 调用 -> 内核渲染文本到 fb */
static long tlog(const char *m) {
	register long x8 asm("x8") = __NR_t508n_log;
	register long x0 asm("x0") = (long)m, x1 asm("x1") = 0, x2 asm("x2") = 0;
	register long x3 asm("x3") = 0, x4 asm("x4") = 0, x5 asm("x5") = 0;
	asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8) : "memory");
	return x0;
}
static void km(const char *m) {
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
	if (fd >= 0) { write(fd, m, strlen(m)); write(fd, "\n", 1); close(fd); }
}
static int load(const char *p) {
	if (access(p, R_OK)) return -1;
	pid_t pid = fork();
	if (pid == 0) {
		int devnull = open("/dev/null", O_RDWR);
		int ef = open("/tmp/l.err", O_WRONLY | O_CREAT | O_TRUNC, 0600);
		if (devnull >= 0) dup2(devnull, 1);
		if (ef >= 0) dup2(ef, 2); else if (devnull >= 0) dup2(devnull, 2);
		execl("/bin/busybox", "busybox", "insmod", p, (char *)NULL);
		_exit(127);
	}
	int st = -1;
	if (pid > 0) waitpid(pid, &st, 0);
	if (WIFEXITED(st) && WEXITSTATUS(st) == 0) return 0;
	/* 失败: 输出 insmod stderr 尾部 60 字符 (含真实 errno 文本) */
	{
		char eb2[256];
		int fd = open("/tmp/l.err", O_RDONLY);
		ssize_t nn2 = fd >= 0 ? read(fd, eb2, sizeof(eb2) - 1) : 0;
		if (fd >= 0) close(fd);
		if (nn2 <= 0) return -1;
		eb2[nn2] = 0;
		eb2[strcspn(eb2, "\n")] = 0;
		if (strstr(eb2, "File exists")) return 0;	/* 已加载 = 成功 */
		{
			char tail[64];
			size_t L = strlen(eb2);
			const char *p = eb2 + (L > 60 ? L - 60 : 0);
			strncpy(tail, p, 63);
			tail[63] = 0;
		}
	}
	return -1;
}
static int exists(const char *d) { return access(d, R_OK) == 0; }

/* /proc/partitions 里所有 mmcblk 行 -> log (截断到 20 字符) */
struct piece { char name[48]; unsigned long long mb; };
static int pcmp(const void *a, const void *b) {
	const struct piece *x = a, *y = b;
	return (x->mb < y->mb) - (x->mb > y->mb);
}
/* 从 sysfs 枚举 mmcblk0p*: PARTNAME + 大小(以M计), 按大小降序只显示前 8 + 总数 */
static void log_partitions(void) {
	struct piece ps[96];
	int n = 0, i;
	DIR *d = opendir("/sys/block/mmcblk0");
	if (!d) { tlog("no mmcblk0 in sysfs"); return; }
	char t[64];
	struct dirent *de;
	while ((de = readdir(d)) && n < 96) {
		char up[256], ln[256], nm[40] = "";
		unsigned long long sz = 0;
		int fd;
		if (strncmp(de->d_name, "mmcblk0p", 8)) continue;
		snprintf(up, sizeof up, "/sys/block/mmcblk0/%s/uevent", de->d_name);
		FILE *f = fopen(up, "r");
		if (f) {
			while (fgets(ln, sizeof ln, f))
				if (!strncmp(ln, "PARTNAME=", 9)) {
					strncpy(nm, ln + 9, 39);
					nm[39] = 0;
					nm[strcspn(nm, "\n")] = 0;
				}
			fclose(f);
		}
		snprintf(up, sizeof up, "/sys/block/mmcblk0/%s/size", de->d_name);
		fd = open(up, O_RDONLY);
		if (fd >= 0) {
			char sb[32];
			ssize_t rn = read(fd, sb, 31);
			if (rn > 0) { sb[rn] = 0; sz = strtoull(sb, NULL, 10); }
			close(fd);
		}
		snprintf(ps[n].name, sizeof ps[n].name, "%s %s", de->d_name, nm);
		ps[n].mb = sz / 2048;
		n++;
	}
	closedir(d);
	qsort(ps, n, sizeof(ps[0]), pcmp);
}
static int has_mmc(void) {
	FILE *f = fopen("/proc/partitions", "r");
	char buf[256]; int ok = 0;
	if (!f) return 0;
	while (fgets(buf, sizeof buf, f)) if (strstr(buf, "mmcblk")) ok = 1;
	fclose(f); return ok;
}

/* 读 /dev/kmsg: usb 类保留前 4 条(加载时序), mmc 类保留后 4 条 */
static void dump_kmsg(void) {
	char ul[4][80], ml[4][80];
	int i, nu = 0, nm = 0;
	int fd = open("/dev/kmsg", O_RDONLY | O_NONBLOCK);
	if (fd < 0) { tlog("kmsg open FAIL"); return; }
	for (i = 0; i < 4; i++) { ul[i][0] = 0; ml[i][0] = 0; }
	for (;;) {
		char buf[8192];
		ssize_t n = read(fd, buf, sizeof(buf) - 1);
		if (n <= 0) break;
		buf[n] = 0;
		char *msg = strchr(buf, ';');
		if (!msg) continue;
		msg++;
		msg[strcspn(msg, "\n")] = 0;
		if (strncmp(msg, "t508n:", 6) == 0) continue;
		if (strstr(msg, "returned -517")) continue; /* defer 重试刷屏 */
		if (strstr(msg, "calling ") || strstr(msg, "initcall "))
			continue; /* initcall 追踪噪声 */
		if (strstr(msg, "Error 0x") || strstr(msg, "sprd-sdhci +"))
			continue;
		if (strcasestr(msg, "usb") || strcasestr(msg, "dwc3") ||
		    strcasestr(msg, "typec") || strcasestr(msg, "extcon") ||
		    strcasestr(msg, "bc1p2") || strcasestr(msg, "charger") ||
		    strcasestr(msg, "musb") || strcasestr(msg, "hsphy") ||
		    strcasestr(msg, "regulator")) {
			memmove(ul[1], ul[0], 4 * 80 - 80);
			memmove(ul[2], ul[1], 4 * 80 - 2 * 80);
			memmove(ul[3], ul[2], 4 * 80 - 3 * 80);
			snprintf(ul[0], 80, "%.40s", msg);
		} else if (strcasestr(msg, "mmc") || strcasestr(msg, "sdhci") ||
			   strcasestr(msg, "sdio")) {
			memmove(ml[1], ml[0], 4 * 80 - 80);
			memmove(ml[2], ml[1], 4 * 80 - 2 * 80);
			memmove(ml[3], ml[2], 4 * 80 - 3 * 80);
			snprintf(ml[0], 80, "%.40s", msg);
		}
	}
	close(fd);
	for (i = 3; i >= 0; i--)
		if (ul[i][0]) tlog(ul[i]);
	for (i = 3; i >= 0; i--)
		if (ml[i][0]) tlog(ml[i]);
}
/* 原厂式装载: busybox modprobe 按 modules.load 顺序 (依赖自动解析) */
static int modprobe_one(const char *name) {
	pid_t pid = fork();
	if (pid == 0) {
		int devnull = open("/dev/null", O_RDWR);
		int ef = open("/tmp/l.err", O_WRONLY | O_CREAT | O_TRUNC, 0600);
		if (devnull >= 0) dup2(devnull, 1);
		if (ef >= 0) dup2(ef, 2); else if (devnull >= 0) dup2(devnull, 2);
		execl("/bin/busybox", "busybox", "modprobe", name, (char *)NULL);
		_exit(127);
	}
	int st = -1;
	if (pid > 0) waitpid(pid, &st, 0);
	if (WIFEXITED(st) && WEXITSTATUS(st) == 0) return 0;
	{
		char eb[64];
		int fd = open("/tmp/l.err", O_RDONLY);
		ssize_t nn = fd >= 0 ? read(fd, eb, sizeof(eb) - 1) : 0;
		if (fd >= 0) close(fd);
		if (nn > 0) { eb[nn] = 0; eb[strcspn(eb, "\n")] = 0; tlog(eb); }
	}
	return -1;
}
/* 原厂 .ko 逐模块顺序加载 (顺序 = 已验证依赖序) */
static const char *mods[] = {
	"sprd_sip_svc.ko",
	"pinctrl-sprd.ko",
	"pinctrl-sprd-qogirn6pro.ko",
	"clk-sprd.ko",
	"ums9620-clk.ko",
	"spi-sprd-adi.ko",
	"sprd-ump96xx-spi.ko",
	"ump9620-regulator.ko",
	"gpio-eic-sprd.ko",
	"gpio-sprd.ko",
	"gpio-pmic-eic-sprd.ko",
	"i2c-sprd.ko",
	"i2c-sprd-hw.ko",
	"sprd_hwspinlock.ko",
	"nvmem-sc27xx-efuse.ko",
	"nvmem_sprd_efuse.ko",
	"trusty.ko",
	"trusty-pm.ko",
	"trusty-log.ko",
	"trusty-irq.ko",
	"trusty-ipc.ko",
	"trusty-virtio.ko",
	"sprd_shm.ko",
	"unisoc-mailbox.ko",
	"sprd_power_manager.ko",
	"sipc-core.ko",
	"spipe.ko",
	"spool.ko",
	"sipx.ko",
	"seth.ko",
	"sipa-sys.ko",
	"sipa-core.ko",
	"sipa-dele.ko",
	"sipa_eth.ko",
	"sipa_usb.ko",
	"sprd_usbm.ko",
	"sprd-bc1p2.ko",
	"phy-sprd-commonphy.ko",
	"phy-sprd-dummy.ko",
	"ptn38003a-i2c.ko",
	"sprd_tcpm.ko",
	"sc27xx_typec.ko",
	"extcon-usb-gpio.ko",
	"phy-sprd-usb31-qogirn6pro.ko",
	"dwc3-sprd-core.ko",
	"dwc3-sprd.ko",
	"musb_hdrc.ko",
	"phy-sprd-qogirn6pro.ko",
	"musb_sprd.ko",
	"mmc_swcq.ko",
	"mmc_hsq.ko",
	"sdhci-sprd.ko",
	"sprd_pmic_syscon.ko",
	NULL
};
static int load_module_set(void) {
	int fails = 0, n = 0, i;
	char path[200];
	for (i = 0; mods[i]; i++) {
		snprintf(path, sizeof path, "/lib/modules/%s/%s",
			"5.4.210-android12-9-ga1d5c173d86b-ab1", mods[i]);
		n++;
		if (load(path)) {
			fails++;
			tlog("load fail: ");
			tlog(mods[i]);
			{
				char rb[128];
				int fd7 = open("/tmp/l.err", O_RDONLY);
				ssize_t rn7 = fd7 >= 0 ? read(fd7, rb, 127) : -1;
				if (fd7 >= 0) close(fd7);
				if (rn7 > 0) {
					rb[rn7] = 0;
					rb[strcspn(rb, "\n")] = 0;
					{
						size_t L7 = strlen(rb);
						const char *p7 = rb + (L7 > 55 ? L7 - 55 : 0);
						tlog(p7);
					}
				}
			}
		}
	}
	km("mods done\n");
	return fails ? -fails : n;
}


static const char *find_part(const char *name, const char *node) {
	DIR *d = opendir("/sys/block/mmcblk0");
	if (!d) return NULL;
	char found[40] = "";
	struct dirent *de;
	static char part_dev[32];
	while ((de = readdir(d))) {
		char up[256], ln[256], nm[40] = "";
		FILE *f;
		if (strncmp(de->d_name, "mmcblk0p", 8)) continue;
		snprintf(up, sizeof up, "/sys/block/mmcblk0/%s/uevent", de->d_name);
		f = fopen(up, "r");
		if (!f) continue;
		while (fgets(ln, sizeof ln, f))
			if (!strncmp(ln, "PARTNAME=", 9)) {
				strncpy(nm, ln + 9, 39);
				nm[39] = 0;
				nm[strcspn(nm, "\n")] = 0;
			}
		fclose(f);
		if (!strcmp(nm, name)) { snprintf(found, sizeof found, "%s", de->d_name); break; }
	}
	closedir(d);
	if (!found[0]) return NULL;
	{
		char dv[256], nb[32];
		unsigned int ma = 0, mi = 0;
		int fd = open(dv, O_RDONLY);
		snprintf(dv, sizeof dv, "/sys/block/mmcblk0/%s/dev", found);
		fd = open(dv, O_RDONLY);
		if (fd >= 0) {
			ssize_t rn = read(fd, nb, 31);
			if (rn > 0) { nb[rn] = 0; sscanf(nb, "%u:%u", &ma, &mi); }
			close(fd);
		}
		if (!ma) return NULL;
		snprintf(part_dev, sizeof part_dev, "/dev/%s", node);
		mknod(part_dev, S_IFBLK | 0600, makedev(ma, mi));
		return part_dev;
	}
}

static int run_efs(const char *bin, const char *a1, const char *a2, const char *a3, const char *a4, const char *a5) {
	pid_t pid = fork();
	if (pid == 0) {
		char *argv[9];
		int n = 0;
		char p[64];
		snprintf(p, sizeof p, "/e2fs/%s", bin);
		argv[n++] = p;
		if (a1) argv[n++] = (char *)a1;
		if (a2) argv[n++] = (char *)a2;
		if (a3) argv[n++] = (char *)a3;
		if (a4) argv[n++] = (char *)a4;
		if (a5) argv[n++] = (char *)a5;
		argv[n] = NULL;
		char *env[] = { "LD_LIBRARY_PATH=/e2fs", "MKE2FS_CONFIG=/e2fs/mke2fs.conf", NULL };
		execve(p, argv, env);
		_exit(127);
	}
	int st = -1;
	if (pid > 0) waitpid(pid, &st, 0);
	return WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 0 : -1;
}

static int run_tar(const char *dir) {
	pid_t pid = fork();
	if (pid == 0) {
		execl("/bin/busybox", "busybox", "tar", "-xzf", "/alpine.tar.gz", "-C", dir, (char *)NULL);
		_exit(127);
	}
	int st = -1;
	if (pid > 0) waitpid(pid, &st, 0);
	return WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 0 : -1;
}

static void copy_file(const char *src, const char *dst) {
	int in = open(src, O_RDONLY), out = -1;
	char buf[4096];
	ssize_t n;
	if (in < 0) return;
	out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0755);
	while (out >= 0 && (n = read(in, buf, sizeof buf)) > 0)
		if (write(out, buf, n) != n) break;
	if (in >= 0) close(in);
	if (out >= 0) close(out);
}

static void rootfs_boot(void) {
	const char *dev_root = find_part("super", "t508root");
	const char *dev_home = find_part("userdata", "t508home");
	int have = 0;
	if (!dev_root || !dev_home)
		return;
	mkdir("/mnt", 0755);
	if (mount(dev_root, "/mnt", "ext4", MS_RDONLY, NULL) == 0) {
		have = (access("/mnt/etc/rc.t508", F_OK) == 0);
		umount("/mnt");
	}
	if (have) {
		if (mount(dev_root, "/mnt", "ext4", 0, NULL) != 0)
			return;
		copy_file("/alpine-inittab", "/mnt/etc/inittab");
		copy_file("/rc.t508", "/mnt/etc/rc.t508");
	} else {
		if (run_efs("mke2fs", "-t", "ext4", "-q", "-F", dev_root) != 0)
			return;
		if (run_efs("mke2fs", "-t", "ext4", "-q", "-F", dev_home) != 0)
			return;
		if (mount(dev_root, "/mnt", "ext4", 0, NULL) != 0)
			return;
		if (run_tar("/mnt") != 0)
			return;
		copy_file("/alpine-inittab", "/mnt/etc/inittab");
		copy_file("/rc.t508", "/mnt/etc/rc.t508");
	}
	mkdir("/mnt/proc", 0755); mkdir("/mnt/sys", 0755);
	mkdir("/mnt/dev", 0755); mkdir("/mnt/tmp", 0755);
	mkdir("/mnt/home", 0755);
	mount("proc", "/mnt/proc", "proc", 0, NULL);
	mount("sysfs", "/mnt/sys", "sysfs", 0, NULL);
	mount("devtmpfs", "/mnt/dev", "devtmpfs", 0, NULL);
	mount("tmpfs", "/mnt/tmp", "tmpfs", 0, NULL);
	mount(dev_home, "/mnt/home", "ext4", 0, NULL);
	chroot("/mnt");
	chdir("/");
	execl("/sbin/init", "/sbin/init", (char *)NULL);
}

int main(void) {
	tlog("init: enter");
	mount("proc", "/proc", "proc", 0, NULL);
	mount("sysfs", "/sys", "sysfs", 0, NULL);
	mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
	mount("tmpfs", "/tmp", "tmpfs", 0, NULL);
	mkdir("/dev/pts", 0755);
	mount("devpts", "/dev/pts", "devpts", 0, NULL);
	mknod("/dev/kmsg", S_IFCHR | 0666, makedev(1, 11));
	mknod("/dev/console", S_IFCHR | 0666, makedev(5, 1));
	km("mounts ok");
	/* pstore console 回读: 上一轮 panic/oops 原文 */
	{
		FILE *f = fopen("/sys/fs/pstore/console-ramoops-0", "r");
		if (f) {
			char lb[128];
			int m = 0;
			while (fgets(lb, sizeof lb, f) && m < 6) {
				lb[strcspn(lb, "\n")] = 0;
				if (strstr(lb, "T508N") || strstr(lb, "panic") ||
				    strstr(lb, "SError") || strstr(lb, "Unable"))
					tlog(lb);
				m++;
			}
			fclose(f);
		}
	}
	int fails = 0;
	{
		int r = load_module_set();
		if (r < 0) fails = -r;
	}
	if (fails == 0) {
		km("all modules ok");
		tlog("===USB===");
	/* STEP1: 域唤醒 (写 on + 回读 + 摘要) */
	{
		const char *cdevs[] = {
			"/sys/devices/platform/soc/soc:ipa-apb/25100000.usb3/power/control",
			"/sys/devices/platform/soc/soc:ipa-apb/25220000.sipa/power/control",
			"/sys/devices/platform/soc/soc:ipa-apb/sipa-dele/power/control",
			NULL
		};
		int ci, st1ok = 0, st1n = 0;
		for (ci = 0; cdevs[ci]; ci++) {
			FILE *gf = fopen(cdevs[ci], "w");
			if (gf) { fputs("on", gf); fclose(gf); }
		}
		sleep(2);
		for (ci = 0; cdevs[ci]; ci++) {
			char smp[16];
			int fd9 = open(cdevs[ci], O_RDONLY);
			if (fd9 >= 0) {
				ssize_t rn9 = read(fd9, smp, 15);
				close(fd9);
				if (rn9 > 0) { smp[rn9] = 0; if (!strncmp(smp, "on", 2)) st1n++; }
			}
		}
		{
			FILE *gf = fopen("/sys/kernel/debug/pm_genpd/pm_genpd_summary", "r");
			if (gf) {
				char gl9[160];
				while (fgets(gl9, sizeof gl9, gf)) {
					gl9[strcspn(gl9, "\n")] = 0;
					if (strstr(gl9, "25100000.usb3") && strstr(gl9, "active")) st1ok = 1;
				}
				fclose(gf);
			}
		}
		tlog(st1ok ? "STEP1 域唤醒 OK" : (st1n ? "STEP1 域唤醒 PRTL(control=on x%d)" : "STEP1 域唤醒 FAIL"));
	}
	/* STEP2: MUSB 绑定 (bind + driver 回读 + UDC 存在) */
	{
		FILE *bf = fopen("/sys/bus/platform/drivers/musb-sprd/bind", "w");
		if (bf) {
			if (fputs("64a00000.usb", bf) >= 0) tlog("bind w: ok");
			fclose(bf);
		}
		sleep(1);
		{
			char dl[60], drv[80];
			int fd9 = open("/sys/bus/platform/devices/64a00000.usb/driver", O_RDONLY);
			ssize_t rn9;
			int udc9 = access("/sys/class/udc/musb-hdrc.0.auto", F_OK);
			if (fd9 >= 0) {
				rn9 = read(fd9, drv, 79);
				close(fd9);
				if (rn9 > 0) drv[rn9] = 0; else drv[0] = 0;
			} else drv[0] = 0;
			snprintf(dl, sizeof dl, "STEP2 MUSB %s (udc %s)", strstr(drv, "musb-sprd") ? "OK" : "FAIL", udc9 == 0 ? "in" : "no");
			tlog(dl);
		}
	}
	/* STEP3: 连接 (state retry) */
	{
		int try9, st3 = 0;
		for (try9 = 0; try9 < 8; try9++) {
			char st9[80];
			int fd9 = open("/sys/class/udc/musb-hdrc.0.auto/state", O_RDONLY);
			ssize_t rn9 = -1;
			if (fd9 >= 0) { rn9 = read(fd9, st9, 79); close(fd9); if (rn9 > 0) st9[rn9] = 0; }
			if (strstr(st9, "attached")) { st3 = 1; tlog("STEP3 连接 OK (attached)"); break; }
			sleep(1);
		}
		if (!st3) tlog("STEP3 连接 WAIT (not attached)");
	}
	/* gadget 创建: g1 + f_serial + UDC=musb-hdrc.0.auto */
	{
		FILE *f8;
		mkdir("/sys/kernel/config", 0755);
		mount("configfs", "/sys/kernel/config", "configfs", 0, NULL);
		mkdir("/sys/kernel/config/usb_gadget/g1", 0755);
		mkdir("/sys/kernel/config/usb_gadget/g1/strings/0x409", 0755);
		mkdir("/sys/kernel/config/usb_gadget/g1/functions/serial.0", 0755);
		mkdir("/sys/kernel/config/usb_gadget/g1/configs/c.1/strings/0x409", 0755);
		f8 = fopen("/sys/kernel/config/usb_gadget/g1/idVendor", "w");
		if (f8) { fputs("0x1d6b", f8); fclose(f8); }
		f8 = fopen("/sys/kernel/config/usb_gadget/g1/idProduct", "w");
		if (f8) { fputs("0x0104", f8); fclose(f8); }
		f8 = fopen("/sys/kernel/config/usb_gadget/g1/strings/0x409/manufacturer", "w");
		if (f8) { fputs("TCLT508N", f8); fclose(f8); }
		f8 = fopen("/sys/kernel/config/usb_gadget/g1/configs/c.1/MaxPower", "w");
		if (f8) { fputs("100", f8); fclose(f8); }
		mkdir("/sys/kernel/config/usb_gadget/g1/configs/c.1/serial.0", 0);
		symlink("/sys/kernel/config/usb_gadget/g1/functions/serial.0",
			"/sys/kernel/config/usb_gadget/g1/configs/c.1/serial.0");
		f8 = fopen("/sys/kernel/config/usb_gadget/g1/UDC", "w");
		if (f8) {
			if (fputs("musb-hdrc.0.auto", f8) >= 0) tlog("gadget bind: musb-hdrc.0.auto");
			else tlog("gadget bind: UDC write fail");
			fclose(f8);
		} else tlog("gadget bind: no g1");
	}
	/* kmsg 抓 hsphy 行 (probe 失败原因) */
	{
		int fd9 = open("/dev/kmsg", O_RDONLY | O_NONBLOCK);
		if (fd9 >= 0) {
			int n9 = 0;
			for (;;) {
				char kb[1024];
				ssize_t rn9 = read(fd9, kb, sizeof(kb) - 1);
				char *msg9;
				if (rn9 <= 0) break;
				kb[rn9] = 0;
				msg9 = strchr(kb, ';');
				if (!msg9) continue;
				msg9++;
				msg9[strcspn(msg9, "\n")] = 0;
				if ((strstr(msg9, "hsphy") || strstr(msg9, "phy-sprd")) && n9 < 4) {
					tlog(msg9);
					n9++;
				}
			}
			close(fd9);
			if (!n9) tlog("kmsg-hsphy: none");
		}
	}
	/* 64300000.hsphy 节点 compatible + 驱动 modalias */	/* 64300000.hsphy 节点 compatible + 驱动 modalias */
	{
		char cb[200];
		int fd9 = open("/sys/bus/platform/devices/64300000.hsphy/of_node/compatible", O_RDONLY);
		ssize_t rn9 = -1;
		if (fd9 >= 0) { rn9 = read(fd9, cb, 199); close(fd9); if (rn9 > 0) cb[rn9] = 0; else cb[0] = 0; }
		else cb[0] = 0;
		tlog("node-comp: ");
		tlog(cb[0] ? cb : "?");
	}
	/* 真名扫描: platform 设备含 phy/usb2/hsphy 的 */	/* 真名扫描: platform 设备含 phy/usb2/hsphy 的 */
	{
		DIR *pd9 = opendir("/sys/bus/platform/devices");
		if (pd9) {
			struct dirent *pe9;
			int n9 = 0;
			while ((pe9 = readdir(pd9)) && n9 < 12) {
				if (strstr(pe9->d_name, "phy") || strstr(pe9->d_name, "hsphy")) {
					char dl9[12 + 60];
					int fd9;
					char lk[80];
					ssize_t rn9;
					snprintf(dl9, sizeof dl9, "/sys/bus/platform/devices/%s/driver", pe9->d_name);
					fd9 = open(dl9, O_RDONLY);
					if (fd9 >= 0) {
						rn9 = read(fd9, lk, 79);
						close(fd9);
						if (rn9 > 0) lk[rn9] = 0; else lk[0] = 0;
					} else lk[0] = 0;
					tlog(pe9->d_name);
					tlog(lk[0] ? lk : " NODRV");
					n9++;
				}
			}
			closedir(pd9);
			if (!n9) tlog("physcan: none");
		}
	}
	/* 手动绑定 hsphy (跳延期) */	/* 手动绑定 hsphy (跳延期) */
	{
		FILE *bf = fopen("/sys/bus/platform/drivers/sprd-hsphy/bind", "w");
		if (bf) {
			if (fputs("64300000.hsphy", bf) >= 0) tlog("hsphy bind: OK");
			else tlog("hsphy bind: fail");
			fclose(bf);
		} else tlog("hsphy bind: no drv dir");
	}
	/* 517 判定: 300000.phy 的 driver + devices_deferred */	/* 517 判定: 300000.phy 的 driver + devices_deferred */
	{
		char dl9[80];
		int fd9 = open("/sys/bus/platform/devices/300000.phy/driver", O_RDONLY);
		ssize_t rn9S = -1;
		if (fd9 >= 0) { rn9S = read(fd9, dl9, 79); close(fd9); if (rn9S > 0) dl9[rn9S] = 0; else dl9[0] = 0; }
		else dl9[0] = 0;
		tlog(dl9[0] ? dl9 : "phy-drv: NO-DRIVER");
		{
			mount("debugfs", "/sys/kernel/debug", "debugfs", 0, NULL);
			FILE *df = fopen("/sys/kernel/debug/devices_deferred", "r");
			if (df) {
				int got = 0;
				char dl2[160];
				while (fgets(dl2, sizeof dl2, df)) {
					if (strstr(dl2, "64300000.hsphy") || strstr(dl2, "hsphy")) {
						dl2[strcspn(dl2, "\n")] = 0;
						tlog(dl2);
						got = 1;
					}
				}
				fclose(df);
				if (!got) tlog("defers: no 300000.phy");
			} else tlog("defers: no debugfs");
		}
	}
	/* REG: 遍历找 vddusb33 (重试等晚段探测) */	/* REG: 遍历找 vddusb33 (重试等晚段探测) */
	{
		int tryR;
		for (tryR = 0; tryR < 8; tryR++) {
			DIR *rd = opendir("/sys/class/regulator");
			int found33 = 0;
			if (!rd) break;
			while (1) {
				struct dirent *re = readdir(rd);
				char p3[256], nm3[80], st3[32];
				int fd3;
				ssize_t rn3;
				if (!re) break;
				if (re->d_name[0] == '.') continue;
				snprintf(p3, sizeof p3, "/sys/class/regulator/%s/name", re->d_name);
				fd3 = open(p3, O_RDONLY);
				if (fd3 < 0) continue;
				rn3 = read(fd3, nm3, 79);
				close(fd3);
				if (rn3 <= 0) continue;
				nm3[rn3] = 0;
				if (strstr(nm3, "vddusb")) {
					snprintf(p3, sizeof p3, "/sys/class/regulator/%s/state", re->d_name);
					fd3 = open(p3, O_RDONLY);
					if (fd3 >= 0) {
						rn3 = read(fd3, st3, 31);
						close(fd3);
						if (rn3 > 0) st3[rn3] = 0; else snprintf(st3, sizeof st3, "?");
					} else snprintf(st3, sizeof st3, "?");
					tlog("REG33: ");
					tlog(st3);
					if (strstr(st3, "enabled")) found33 = 1;
				}
			}
			closedir(rd);
			if (found33) break;
			sleep(1);
		}
	}
	/* STEP4: gadget (g1 UDC 回读 + ttyGS0) */	/* STEP4: gadget (g1 UDC 回读 + ttyGS0) */	/* STEP4: gadget (g1 UDC 回读 + ttyGS0) */
	{
		char gudc[80];
		int fd9 = open("/sys/kernel/config/usb_gadget/g1/UDC", O_RDONLY);
		ssize_t rn9;
		int tty9 = access("/dev/ttyGS0", F_OK);
		if (fd9 >= 0) {
			rn9 = read(fd9, gudc, 79);
			close(fd9);
			if (rn9 > 0) gudc[rn9] = 0; else gudc[0] = 0;
		} else gudc[0] = 0;
		tlog(strstr(gudc, "musb-hdrc.0.auto") && tty9 == 0 ? "STEP4 gadget OK (UDC+ttyGS0)" : (gudc[0] ? "STEP4 gadget PRTL" : "STEP4 gadget FAIL"));
	}
	}
	km("v16 done");
	rootfs_boot();
	tlog("@F");
	for (;;) pause();
}
