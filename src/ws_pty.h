/**
 * @file ws_pty.h
 * @author your name (you@domain.com)
 * @brief based on mongoose-websocket to login bash on the embed system
 * @version 0.1
 * @date 2021-04-03
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#pragma once

#define _GNU_SOURCE
#include <features.h>

#include "mongoose/mongoose.h"

#include <stdint.h>

#define MAX_USERNAME_LENGTH           48


/**
 * @brief 运行websocket-pty服务器
 * 
 */
void pty_ws_run();