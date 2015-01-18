#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h> 
#include <linux/fs.h> 
#include <linux/errno.h>
#include <linux/types.h> 
#include <linux/random.h>

#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/device.h>

#define INTERNAL_SEED 0
#define EXTERNAL_SEED 1

#define RANDOMIZER_MAJOR 235
#define RANDOMIZER_MINOR 11 
#define ERANDOM_MINOR 12 

static struct file_operations randomizer_fops;

static int erandom_seeded = 0;

static int randomizer_major = RANDOMIZER_MAJOR;
static int randomizer_minor = RANDOMIZER_MINOR;
static int erandom_minor = ERANDOM_MINOR;
static int randomizer_bufsize = 256;
static int randomizer_chunklimit = 0;

static struct cdev randomizer_cdev;
static struct cdev erandom_cdev;
static struct class *randomizer_class;
struct device *randomizer_device;
struct device *erandom_device;

module_param(randomizer_major, int, 0); /* for usage in /dev/<thedevicename> as majorn number */
module_param(randomizer_minor, int, 0);
module_param(erandom_minor, int, 0);
module_param(randomizer_bufsize, int, 0);
module_param(randomizer_chunklimit, int, 0);

MODULE_PARM_DESC(randomizer_major,"Major number of /dev/randomizer and /dev/erandom");
MODULE_PARM_DESC(randomizer_minor,"Minor number of /dev/randomizer");
MODULE_PARM_DESC(erandom_minor,"Minor number of /dev/erandom");
MODULE_PARM_DESC(randomizer_bufsize,"Internal buffer size in bytes. Default is 256. Must be >= 256");
MODULE_PARM_DESC(randomizer_chunklimit,"Limit for read() blocks size. 0 (default) is unlimited, otherwise must be >= 256");

struct randomizer_state
{
	struct semaphore sem;

	u8 S[256];
	u8 i;        
	u8 j;

	char *buf;
};

static struct randomizer_state *erandom_state;

static inline void swap_byte(u8 *a, u8 *b)
{
	u8 swapByte; 
  
	swapByte = *a; 
	*a = *b;      
	*b = swapByte;
}

static void init_rand_state(struct randomizer_state *state, int seedflag);

void erandom_get_random_bytes(char *buf, size_t count)
{
	struct randomizer_state *state = erandom_state;
	int k;

	unsigned int i;
	unsigned int j;
	u8 *S;

	if (down_interruptible(&state->sem)) {
		get_random_bytes(buf, count);
		return;
	}

	printk("\n the count given to erandom get random bytes: %zu", count);
	printk("\n");

	if (!erandom_seeded) {
		erandom_seeded = 1;
		init_rand_state(state, EXTERNAL_SEED);
		printk(KERN_INFO "randomizer: Seeded global generator now (used by erandom)\n");
	}

	i = state->i;     
	j = state->j;
	S = state->S;  

	for (k=0; k<count; k++) {
		i = (i + 1) & 0xff;
		j = (j + S[i]) & 0xff;
		swap_byte(&S[i], &S[j]);
		*buf++ = S[(S[i] + S[j]) & 0xff];
	}
 
	state->i = i;     
	state->j = j;

	up(&state->sem);
}

static void init_rand_state(struct randomizer_state *state, int seedflag)
{
	unsigned int i, j, k;
	u8 *S;
	u8 *seed = state->buf;

	if (seedflag == INTERNAL_SEED)
		erandom_get_random_bytes(seed, 256);
	else
		get_random_bytes(seed, 256);

	S = state->S;
	for (i=0; i<256; i++)
		*S++=i;

	j=0;
	S = state->S;

	for (i=0; i<256; i++) {
		j = (j + S[i] + *seed++) & 0xff;
		swap_byte(&S[i], &S[j]);
	}

	i=0; j=0;
	for (k=0; k<256; k++) {
		i = (i + 1) & 0xff;
		j = (j + S[i]) & 0xff;
		swap_byte(&S[i], &S[j]);
	}

	state->i = i; /* Save state */
	state->j = j;


	printk("\n the new state: %d", i);
	printk("\n");
}

static int randomizer_open(struct inode *inode, struct file *filp)
{
  
	struct randomizer_state *state;

	int num = iminor(inode);

	if ((num != randomizer_minor) && (num != erandom_minor)) return -ENODEV;
  
	state = kmalloc(sizeof(struct randomizer_state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	state->buf = kmalloc(randomizer_bufsize, GFP_KERNEL);
	if (!state->buf) {
		kfree(state);
		return -ENOMEM;
	}

	sema_init(&state->sem, 1);

	if (num == randomizer_minor)
		init_rand_state(state, EXTERNAL_SEED);
	else
		init_rand_state(state, INTERNAL_SEED);

	filp->private_data = state;

	return 0;
}

static int randomizer_release(struct inode *inode, struct file *filp)
{

	struct randomizer_state *state = filp->private_data;

	kfree(state->buf);
	kfree(state);

	return 0;
}

static ssize_t randomizer_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	struct randomizer_state *state = filp->private_data;
	ssize_t ret;
	int dobytes, k;
	char *localbuf;

	unsigned int i;
	unsigned int j;
	u8 *S;
  
	if (down_interruptible(&state->sem))
		return -ERESTARTSYS;
  
	if ((randomizer_chunklimit > 0) && (count > randomizer_chunklimit))
		count = randomizer_chunklimit;

	ret = count;
  
	i = state->i;     
	j = state->j;
	S = state->S;  

	while (count) {
		if (count > randomizer_bufsize)
			dobytes = randomizer_bufsize;
		else
			dobytes = count;

		localbuf = state->buf;

		for (k=0; k<dobytes; k++) {
			i = (i + 1) & 0xff;
			j = (j + S[i]) & 0xff;
			swap_byte(&S[i], &S[j]);
			*localbuf++ = S[(S[i] + S[j]) & 0xff];
		}
 
		if (copy_to_user(buf, state->buf, dobytes)) {
			ret = -EFAULT;
			goto out;
		}

		buf += dobytes;
		count -= dobytes;
	}

 out:
	state->i = i;     
	state->j = j;

	up(&state->sem);

	return ret;
}

/* the allowed file operations, read, open and release */
static struct file_operations randomizer_fops = {
	read:       randomizer_read,
	open:       randomizer_open,
	release:    randomizer_release,
};

void randomizer_cleanup_module(void) {
	unregister_chrdev_region(MKDEV(randomizer_major, erandom_minor), 1);
	cdev_del(&erandom_cdev);
	device_destroy(randomizer_class, MKDEV(randomizer_major, erandom_minor));

	unregister_chrdev_region(MKDEV(randomizer_major, randomizer_minor), 1);
	cdev_del(&randomizer_cdev);
	device_destroy(randomizer_class, MKDEV(randomizer_major, randomizer_minor));
	class_destroy(randomizer_class);

	kfree(erandom_state->buf);
	kfree(erandom_state);
}

int randomizer_init_module(void)
{
	int result;
    
	if (randomizer_bufsize < 256) {
		printk(KERN_ERR "randomizer: Refused to load because randomizer_bufsize=%d < 256\n",randomizer_bufsize);
		return -EINVAL;
	}
	if ((randomizer_chunklimit != 0) && (randomizer_chunklimit < 256)) {
		printk(KERN_ERR "randomizer: Refused to load because randomizer_chunklimit=%d < 256 and != 0\n",randomizer_chunklimit);
		return -EINVAL;
	}

	erandom_state = kmalloc(sizeof(struct randomizer_state), GFP_KERNEL);
	if (!erandom_state)
		return -ENOMEM;

	erandom_state->buf = kmalloc(256, GFP_KERNEL);
	if (!erandom_state->buf) {
		kfree(erandom_state);
		return -ENOMEM;
	}

	sema_init(&erandom_state->sem, 1);

	erandom_seeded = 0;

	randomizer_class = class_create(THIS_MODULE, "fastrng");
	if (IS_ERR(randomizer_class)) {
		result = PTR_ERR(randomizer_class);
		printk(KERN_WARNING "randomizer: Failed to register class fastrng\n");
		goto error0;
	}

	cdev_init(&randomizer_cdev, &randomizer_fops);
	randomizer_cdev.owner = THIS_MODULE;
	result = cdev_add(&randomizer_cdev, MKDEV(randomizer_major, randomizer_minor), 1);
	if (result) {
	  printk(KERN_WARNING "randomizer: Failed to add cdev for /dev/randomizer\n");
	  goto error1;
	}

	result = register_chrdev_region(MKDEV(randomizer_major, randomizer_minor), 1, "/dev/randomizer");
	if (result < 0) {
		printk(KERN_WARNING "randomizer: can't get major/minor %d/%d\n", randomizer_major, randomizer_minor);
	  goto error2;
	}

	randomizer_device = device_create(randomizer_class, NULL, MKDEV(randomizer_major, randomizer_minor), NULL, "randomizer");

	if (IS_ERR(randomizer_device)) {
		printk(KERN_WARNING "randomizer: Failed to create randomizer device\n");
		goto error3;
	}

	cdev_init(&erandom_cdev, &randomizer_fops);
	erandom_cdev.owner = THIS_MODULE;
	result = cdev_add(&erandom_cdev, MKDEV(randomizer_major, erandom_minor), 1);
	if (result) {
	  printk(KERN_WARNING "randomizer: Failed to add cdev for /dev/erandom\n");
	  goto error4;
	}

	result = register_chrdev_region(MKDEV(randomizer_major, erandom_minor), 1, "/dev/erandom");
	if (result < 0) {
		printk(KERN_WARNING "randomizer: can't get major/minor %d/%d\n", randomizer_major, erandom_minor);
		goto error5;
	}

	erandom_device = device_create(randomizer_class, NULL, MKDEV(randomizer_major, erandom_minor), NULL, "erandom");

	if (IS_ERR(erandom_device)) {
		printk(KERN_WARNING "randomizer: Failed to create erandom device\n");
		goto error6;
	}
	return 0;

 error6:
	unregister_chrdev_region(MKDEV(randomizer_major, erandom_minor), 1);
 error5:
	cdev_del(&erandom_cdev);
 error4:
	device_destroy(randomizer_class, MKDEV(randomizer_major, randomizer_minor));
 error3:
	unregister_chrdev_region(MKDEV(randomizer_major, randomizer_minor), 1);
 error2:
	cdev_del(&randomizer_cdev);
 error1:
	class_destroy(randomizer_class);
 error0:
	kfree(erandom_state->buf);
	kfree(erandom_state);

	return result;	
}

EXPORT_SYMBOL(erandom_get_random_bytes);

