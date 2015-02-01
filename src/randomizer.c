#include "randomizer.h"
\
static int randomizer_major = 235;
static int randomizer_minor = 11;
static int randomizer_bufsize = 256;
static int randomizer_chunklimit = 0;

static struct cdev randomizer_cdev;
static struct class *randomizer_class;
static struct file_operations randomizer_fops;

struct device *randomizer_device;

static inline void switch_bytes(u8 *firstByte, u8 *secondByte)
{
	u8 switchByte; 
	switchByte = *firstByte; 
	*firstByte = *secondByte;      
	*secondByte = switchByte;
}

static void init_rand_state(struct randomizer_state *state, int seedflag)
{
	unsigned int i, j, k;
	u8 *S;
	u8 *seed = state->buf;

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

	unregister_chrdev_region(MKDEV(randomizer_major, randomizer_minor), 1);
	cdev_del(&randomizer_cdev);
	device_destroy(randomizer_class, MKDEV(randomizer_major, randomizer_minor));
	class_destroy(randomizer_class);

}

int randomizer_init(void)
{
	int result;
	randomizer_class = class_create(THIS_MODULE, "fastrng");
	cdev_init(&randomizer_cdev, &randomizer_fops);
	randomizer_cdev.owner = THIS_MODULE;
	result = cdev_add(&randomizer_cdev, MKDEV(randomizer_major, randomizer_minor), 1);
	result = register_chrdev_region(MKDEV(randomizer_major, randomizer_minor), 1, "/dev/randomizer");

	randomizer_device = device_create(randomizer_class, NULL, MKDEV(randomizer_major, randomizer_minor), NULL, "randomizer");
	
	
	if(result != 0){
		randomizer_exit();
		return result;
	}
	return 0;
}

