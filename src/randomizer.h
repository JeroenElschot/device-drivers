#ifndef RANDOMIZER_H_
#define RANDOMIZER_H_

#define INTERNAL_SEED 0
#define EXTERNAL_SEED 1

#include <linux/compiler.h> 
#include <linux/slab.h> 
#include <linux/fs.h> 
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/random.h>
#include <asm/uaccess.h>

__must_check int randomizer_init(void);

void randomizer_exit(void); 

struct randomizer_state
{
	struct semaphore sem;
	u8 S[256];
	u8 i;        
	u8 j;
	char *buf;
};

#endif //RANDOMIZER_H_

