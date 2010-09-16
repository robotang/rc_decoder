#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xa014127b, "module_layout" },
	{ 0xd95ffc0b, "omap_dm_timer_start" },
	{ 0x5f5c0703, "omap_dm_timer_set_int_enable" },
	{ 0x57342554, "omap_dm_timer_set_load" },
	{ 0xdd11aa34, "clk_get_rate" },
	{ 0x20117eda, "omap_dm_timer_get_fclk" },
	{ 0x859c6dc7, "request_threaded_irq" },
	{ 0xdaeddd35, "omap_dm_timer_get_irq" },
	{ 0x98a95a6, "omap_dm_timer_set_prescaler" },
	{ 0x7507ec41, "omap_dm_timer_set_source" },
	{ 0x74280aea, "omap_dm_timer_request" },
	{ 0x3de97956, "omap_dm_timer_read_status" },
	{ 0xfe95ae0b, "omap_dm_timer_write_status" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
	{ 0xa900a632, "omap_dm_timer_free" },
	{ 0xf20dabd8, "free_irq" },
	{ 0x49a40e02, "omap_dm_timer_stop" },
	{ 0xea147363, "printk" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "3B4486634931CC491935EF4");
