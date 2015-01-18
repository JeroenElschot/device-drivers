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

	printk("Initializing Randomizer...\n");
	reg = randomizer_init();
	
	return reg;
}


static void __exit
main_exit(void)
{
	printk("Shutting down Randomizer...\n");
	randomizer_exit();
}

module_init(main_init);
module_exit(main_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jeroen Elschot & Richard Olthuis");
MODULE_DESCRIPTION("Random data genarator");
MODULE_VERSION("dev");


