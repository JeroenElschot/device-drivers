#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>

#include <asm/uaccess.h>
#include "randomizer.h"

static int __init
main_init(void)
{
	int reg;

	reg = randomizer_init_module();

	return reg;
}


static void __exit
main_exit(void)
{
	randomizer_cleanup_module();
}

module_init(main_init);
module_exit(main_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jeroen Elschot & Richard Olthuis");
MODULE_DESCRIPTION("\"Random data generator!\" minimal module");
MODULE_VERSION("dev");


