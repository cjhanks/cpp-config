
#ifndef __DEBUG_HH_
#define __DEBUG_HH_ 

#include <iostream>

#define DEBUG(_msg_) \
    do { std::cerr << _msg_ << std::endl; } while (0)

#endif 
