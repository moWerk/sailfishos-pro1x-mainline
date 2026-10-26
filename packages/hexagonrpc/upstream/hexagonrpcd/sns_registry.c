/*
 * Sensor registry property interface
 *
 * The ADSP sensor framework asks the application processor for platform
 * properties (hw_platform, ...) that Android keeps in sysfs or system properties.
 * They are looked up in <root>/properties/<name>, then <root>/socinfo/<name>.
 *
 * This file is part of HexagonRPC.
 *
 * HexagonRPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <libhexagonrpc/error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interface/sns_registry.h"
#include "listener.h"
#include "sns_registry.h"

struct sns_registry_ctx {
	char *prefix;
};

static int read_property(const char *prefix, const char *dir,
			 const char *name, char *out, size_t out_size)
{
	char *path;
	FILE *f;
	size_t n;

	path = malloc(strlen(prefix) + strlen(dir) + strlen(name) + 3);
	if (path == NULL)
		return -1;

	sprintf(path, "%s/%s/%s", prefix, dir, name);

	f = fopen(path, "r");
	free(path);
	if (f == NULL)
		return -1;

	n = fread(out, 1, out_size - 1, f);
	fclose(f);

	while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r'))
		n--;
	out[n] = '\0';

	return 0;
}

static uint32_t sns_registry_get_property(void *data,
					  const struct fastrpc_io_buffer *inbufs,
					  struct fastrpc_io_buffer *outbufs)
{
	struct sns_registry_ctx *ctx = data;
	const char *name = inbufs[1].p;

	if (outbufs[0].s == 0)
		return AEE_EBADPARM;

	memset(outbufs[0].p, 0, outbufs[0].s);

	if (inbufs[1].s == 0 || name[inbufs[1].s - 1] != '\0')
		return AEE_EBADPARM;

	// The name becomes a file name: no directories.
	if (name[0] == '\0' || strchr(name, '/') != NULL || name[0] == '.')
		return AEE_EBADPARM;

	if (read_property(ctx->prefix, "properties", name,
			  outbufs[0].p, outbufs[0].s) == 0
	 || read_property(ctx->prefix, "socinfo", name,
			  outbufs[0].p, outbufs[0].s) == 0) {
		fprintf(stderr, "sns_registry: %s = \"%s\"\n", name,
				(const char *) outbufs[0].p);
		return 0;
	}

	fprintf(stderr, "sns_registry: no property %s\n", name);
	memset(outbufs[0].p, 0, outbufs[0].s);

	return AEE_ERESOURCENOTFOUND;
}

static const struct fastrpc_function_impl sns_registry_procs[] = {
	[0] = { .def = &sns_registry_get_property_def,
		.impl = sns_registry_get_property, },
};

const struct fastrpc_interface sns_registry_interface = {
	.name = "sns_registry",
	.n_procs = 1,
	.procs = sns_registry_procs,
};

struct fastrpc_interface *fastrpc_sns_registry_init(const char *prefix)
{
	struct fastrpc_interface *iface;
	struct sns_registry_ctx *ctx;

	iface = malloc(sizeof(*iface));
	if (iface == NULL)
		return NULL;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL)
		goto err_free_iface;

	ctx->prefix = strdup(prefix);
	if (ctx->prefix == NULL)
		goto err_free_ctx;

	memcpy(iface, &sns_registry_interface, sizeof(*iface));
	iface->data = ctx;

	return iface;

err_free_ctx:
	free(ctx);
err_free_iface:
	free(iface);

	return NULL;
}

void fastrpc_sns_registry_deinit(struct fastrpc_interface *iface)
{
	struct sns_registry_ctx *ctx = iface->data;

	free(ctx->prefix);
	free(ctx);
	free(iface);
}
