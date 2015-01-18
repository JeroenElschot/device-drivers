#ifndef RANDOMIZER_H_
#define RANDOMIZER_H_
#include <linux/compiler.h> 

__must_check int randomizer_init_module(void);

void randomizer_cleanup_module(void); 

#endif //RANDOMIZER_H_

