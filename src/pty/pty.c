/**
 * @file pty.c
 * @author your name (you@domain.com)
 * @brief for some os that don't have a pty.h defalut.
 * @version 0.1
 * @date 2021-04-03
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#define _GNU_SOURCE
#include <features.h>

#include <fcntl.h>

