#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>

#include <asm/uaccess.h>
#include "frandom.h"

static int __init
main_init(void)
{
	int reg;

	reg = frandom_init_module();

	return reg;
}


static void __exit
main_exit(void)
{
	frandom_cleanup_module();
}

module_init(main_init);
module_exit(main_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jeroen Elschot & Richard Olthuis");
MODULE_DESCRIPTION("\"Random data generator!\" minimal module");
MODULE_VERSION("dev");

