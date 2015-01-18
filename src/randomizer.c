#include "randomizer.h"

static int erandom_seeded = 0;
static int randomizer_major = 235;
static int randomizer_minor = 11;
static int erandom_minor = 12;
static int randomizer_bufsize = 256;
static int randomizer_chunklimit = 0;

static struct cdev randomizer_cdev;
static struct cdev erandom_cdev;
static struct class *randomizer_class;
static struct file_operations randomizer_fops;

struct device *randomizer_device;
struct device *erandom_device;

static struct randomizer_state *erandom_state;
static inline void switch_bytes(u8 *firstByte, u8 *secondByte)
{
	u8 switchByte; 
	switchByte = *firstByte; 
	*firstByte = *secondByte;      
	*secondByte = switchByte;
}

static void init_rand_state(struct randomizer_state *state, int seedflag);
void erandom_get_random_bytes(char *buf, size_t count)
{
	struct randomizer_state *state = erandom_state;
	int k;

	unsigned int i;
	unsigned int j;
	u8 *S;

	if (!erandom_seeded) {
		erandom_seeded = 1;
		init_rand_state(state, EXTERNAL_SEED);
	}

	i = state->i;     
	j = state->j;
	S = state->S;  

	for (k=0; k<count; k++) {
		i = (i + 1) & 0xff;
		j = (j + S[i]) & 0xff;
		switch_bytes(&S[i], &S[j]);
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
		switch_bytes(&S[i], &S[j]);
	}

	i=0; j=0;
	for (k=0; k<256; k++) {
		i = (i + 1) & 0xff;
		j = (j + S[i]) & 0xff;
		switch_bytes(&S[i], &S[j]);
	}

	state->i = i; 
	state->j = j;
}

static int randomizer_open(struct inode *inode, struct file *filp)
{
  
	struct randomizer_state *state;

	int num = iminor(inode);
  
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
			switch_bytes(&S[i], &S[j]);
			*localbuf++ = S[(S[i] + S[j]) & 0xff];
		}
 
		if (copy_to_user(buf, state->buf, dobytes)) {
			state->i = i;     
			state->j = j;
			ret = -EFAULT;
			break;
		}

		buf += dobytes;
		count -= dobytes;
	}

	up(&state->sem);
	return ret;
}

static struct file_operations randomizer_fops = {
	read:       randomizer_read,
	open:       randomizer_open,
	release:    randomizer_release,
};

void randomizer_exit(void) {
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

int randomizer_init(void)
{
	int result;
	erandom_seeded = 0;

	erandom_state = kmalloc(sizeof(struct randomizer_state), GFP_KERNEL);
	erandom_state->buf = kmalloc(256, GFP_KERNEL);

	sema_init(&erandom_state->sem, 1);

	randomizer_class = class_create(THIS_MODULE, "fastrng");
	cdev_init(&randomizer_cdev, &randomizer_fops);
	randomizer_cdev.owner = THIS_MODULE;
	result = cdev_add(&randomizer_cdev, MKDEV(randomizer_major, randomizer_minor), 1);
	result = register_chrdev_region(MKDEV(randomizer_major, randomizer_minor), 1, "/dev/randomizer");

	randomizer_device = device_create(randomizer_class, NULL, MKDEV(randomizer_major, randomizer_minor), NULL, "randomizer");
	cdev_init(&erandom_cdev, &randomizer_fops);
	erandom_cdev.owner = THIS_MODULE;
	
	result = cdev_add(&erandom_cdev, MKDEV(randomizer_major, erandom_minor), 1);
	result = register_chrdev_region(MKDEV(randomizer_major, erandom_minor), 1, "/dev/erandom");
	erandom_device = device_create(randomizer_class, NULL, MKDEV(randomizer_major, erandom_minor), NULL, "erandom");

	if(result != 0){
		randomizer_exit();
		return result;
	}
	return 0;
}

EXPORT_SYMBOL(erandom_get_random_bytes);