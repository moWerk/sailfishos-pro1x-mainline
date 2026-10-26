/*
 * Sensor registry property interface
 *
 * This file is part of HexagonRPC.
 *
 * HexagonRPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SNS_REGISTRY_H
#define SNS_REGISTRY_H

#include "listener.h"

struct fastrpc_interface *fastrpc_sns_registry_init(const char *prefix);
void fastrpc_sns_registry_deinit(struct fastrpc_interface *iface);

#endif
