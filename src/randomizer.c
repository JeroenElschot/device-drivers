#include "randomizer.h"

static int randomizer_buffersize = 256;
static int major_number = 150;
static int minor_number = 20;

static struct cdev randomizer_cdev;
static struct class *randomizer_class;
static struct file_operations randomizer_fops;
static struct crypto_cipher *cry_cip;

struct device *randomizer_device;

static inline void switch_bytes(u8 *a, u8 *b);
static void init_random_state(struct randomizer_state *state);

//exit the device, destroy the randomizer
void randomizer_exit(void) 
{
	unregister_chrdev_region(MKDEV(major_number, minor_number), 1);
	cdev_del(&randomizer_cdev);
	device_destroy(randomizer_class, MKDEV(major_number, minor_number));
	class_destroy(randomizer_class);
}

//initialisation of randomizer
int randomizer_init(void)
{
	int result;

	randomizer_class = class_create(THIS_MODULE, "random");
	cdev_init(&randomizer_cdev, &randomizer_fops);
	randomizer_cdev.owner = THIS_MODULE;

	//registering the character device if there the number is not already in use.
	result = cdev_add(&randomizer_cdev, MKDEV(major_number, minor_number), 1);
	result = register_chrdev_region(MKDEV(major_number, minor_number), 1, "/dev/randomizer");

	randomizer_device = device_create(randomizer_class, NULL, MKDEV(major_number, minor_number), NULL, "randomizer");
	
	if(result != 0)
	{
		randomizer_exit();
		return result;
	}
	return 0;
}

//open the randomizer
static int randomizer_open(struct inode *inode, struct file *f)
{
	int i = 0;
	struct randomizer_state *state;
	u8 key[128] = "NxccCfpaiq6venmlkhglyP078tHdKxLbj4gD5r5LftGGhd7keNi2RrcRJCM9bPC";
	u8 *state_to_encrypt;
	u8 encrypted[256];

	state = kmalloc(sizeof(struct randomizer_state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	state->buffer = kmalloc(randomizer_buffersize, GFP_KERNEL);
	if (!state->buffer) 
	{
		kfree(state);
		return -ENOMEM;
	}

	sema_init(&state->sem, 1);
	//initialize the state struct with random data
	init_random_state(state);

	state_to_encrypt = state->S;
	cry_cip = crypto_alloc_cipher("aes", 0, 256);

    crypto_cipher_setkey(cry_cip, key, 128);
    crypto_cipher_encrypt_one(cry_cip, encrypted, state_to_encrypt);

    //replace the current state with the encrypted state
    for(i = 0; i < 256; i++)
    	state->S[i] = encrypted[i];

	f->private_data = state;

	return 0;
}

//initalize the random state
static void init_random_state(struct randomizer_state *state)
{
	unsigned int i, j;
	u8 *current_state;
	u8 *seed = state->buffer;

	get_random_bytes(seed, 256);
	
	current_state = state->S;
	for (i=0; i<256; i++)
		*current_state++=i;

	j=0;
	current_state = state->S;

	//swapping the bytes one by one
	for (i=0; i<256; i++) 
	{
		j = (j + current_state[i] + *seed++) & 0xff;
		switch_bytes(&current_state[i], &current_state[j]);
	}

	state->i = i;
	state->j = j;
}

//read the given buffer
static ssize_t randomizer_read(struct file *f, char *buffer, size_t count, loff_t *f_pos)
{
	struct randomizer_state *state = f->private_data;
	ssize_t returnvalue;
	unsigned int i, j;
	int k, l;
	char *buff;
	u8 *S;

	if (down_interruptible(&state->sem))
		return -ERESTARTSYS;

	returnvalue = count;

	i = state->i;
	j = state->j;
	S = state->S;

	while (count)
	{
		if (count > randomizer_buffersize)
			l = randomizer_buffersize; // set the bytes to work with to the size of the buffer
		else
			l = count; // if count lower than buffersize, set l to the remaining count

		buff = state->buffer;

		for (k=0; k<l; k++)
		{
			i = (i + 1) & 0xff;
			j = (j + S[i]) & 0xff;
			switch_bytes(&S[i], &S[j]);
			*buff++ = S[(S[i] + S[j]) & 0xff];
		}

		if (copy_to_user(buffer, state->buffer, l)) 
		{
			state->i = i;     
			state->j = j;
			returnvalue = -EFAULT;
			break;
		}
		buffer += l;
		count -= l; // lower the count , eventualy it reaches 0 and the while loop is over
	}
	//operation done
	up(&state->sem);
	return returnvalue;
}

//release the randomizer, function will free allocated memory.
static int randomizer_release(struct inode *inode, struct file *f)
{
	struct randomizer_state *state = f->private_data;
	kfree(state->buffer);
	kfree(state);
	return 0;
}

// file struct
static struct file_operations randomizer_fops = {
	read:       randomizer_read,
	open:       randomizer_open,
	release:    randomizer_release,
};

//switches the given bytes
static inline void switch_bytes(u8 *a, u8 *b)
{
	u8 c = *a; 
	*a = *b;
	*b = c;
}