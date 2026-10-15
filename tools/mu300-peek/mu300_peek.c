// Dump or poke physical memory / MMIO from dmesg. This kernel has no CONFIG_DEVMEM, so there is no /dev/mem
// and no other way to look at the AGDSP's reserved region or at the syscon registers the audio drivers use.
//   insmod mu300_peek.ko addr=0xafa00000 len=64            DDR
//   insmod mu300_peek.ko addr=0x64910b80 len=48 mmio=1     registers
//   insmod mu300_peek.ko addr=0x64900000 mmio=1 set=0x1000 poke a bit (reads back after)
// init returns an error on purpose: the work is done by then, and failing to load means the next insmod does
// not have to rmmod first.
//
// WARNING, measured the hard way: touching an address inside the AGDSP/AGCP domain - the VBC block at
// 0x56510000, MCDT, or the AGCP AHB syscon - while that domain is powered down hangs the bus, and the
// watchdog takes the board with it. On this device that also costs slot B its boot trial, so LK rolls back to
// Android and it has to be re-armed. Hold a PCM open first, which is what powers the domain, and only then
// read or poke those addresses. DDR and the always-on syscons are safe at any time.
#include <linux/module.h>
#include <linux/io.h>

static unsigned long addr = 0xafa00000;
static unsigned int len = 64;
static int mmio;
static unsigned int set, clr;
module_param(addr, ulong, 0644);
module_param(len, uint, 0644);
module_param(mmio, int, 0644);
module_param(set, uint, 0644);
module_param(clr, uint, 0644);

static int __init peek_init(void)
{
	void __iomem *io = NULL;
	void *p = NULL;
	const void *src;

	if (len > 512)
		len = 512;
	if (mmio) {
		io = ioremap(addr, len < 4 ? 4 : len);
		src = (const void __force *)io;
	} else {
		p = memremap(addr, len, MEMREMAP_WB);
		src = p;
	}
	if (!src) {
		pr_err("mu300_peek: map %#lx failed\n", addr);
		return -ENOMEM;
	}

	if (mmio && (set || clr)) {
		u32 before = readl_relaxed(io);
		u32 after = (before | set) & ~clr;

		writel_relaxed(after, io);
		after = readl_relaxed(io);
		pr_info("mu300_peek: %#lx was %#x, set %#x clr %#x, now %#x\n",
			addr, before, set, clr, after);
		goto out;
	}

	pr_info("mu300_peek: %#lx +%u %s\n", addr, len, mmio ? "mmio" : "ddr");
	print_hex_dump(KERN_INFO, "mu300_peek: ", DUMP_PREFIX_OFFSET, 16, 4, src, len, false);
out:
	if (io)
		iounmap(io);
	if (p)
		memunmap(p);
	return -EAGAIN;
}
module_init(peek_init);
MODULE_LICENSE("GPL");
