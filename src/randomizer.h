#ifndef RANDOMIZER_H_
#define RANDOMIZER_H_
#include <linux/compiler.h> 
#include <linux/slab.h> 
#include <linux/fs.h> 
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/random.h>
#include <asm/uaccess.h>

__must_check int randomizer_init(void);

void randomizer_exit(void); 

#endif //RANDOMIZER_H_

