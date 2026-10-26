/*
 * Sensor registry property interface - method definitions
 *
 * This file is part of HexagonRPC.
 *
 * HexagonRPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdint.h>
#include <libhexagonrpc/hexagonrpc.h>

#include "sns_registry.h"

/*
 * get_property(in sequence<char> name, rout sequence<char> value)
 *
 * Observed on a SM6115 ADSP (Android 11 firmware): the sensor framework asks for
 * "hw_platform" with a 200 byte value buffer. The input words are
 * { name length including NUL, value buffer length }.
 */
static struct hrpc_arg_def_interp4 sns_registry_get_property_args[] = {
	{ HRPC_ARG_BLOB_SEQ, sizeof(char) },
	{ HRPC_ARG_OUT_BLOB_SEQ, sizeof(char) },
};

const struct hrpc_method_def_interp4 sns_registry_get_property_def = {
	.msg_id = 0,
	.n_args = HRPC_ARRAY_SIZE(sns_registry_get_property_args),
	.args = sns_registry_get_property_args,
	.n_inner_types = 0,
	.inner_types = NULL,
};
