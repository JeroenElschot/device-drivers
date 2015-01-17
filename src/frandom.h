#ifndef FRANDOM_H_
#define FRANDOM_H_
#include <linux/compiler.h> 

__must_check int frandom_init_module(void); /* 0 if Ok*/

void frandom_cleanup_module(void); 

#endif //FRANDOM_H_

