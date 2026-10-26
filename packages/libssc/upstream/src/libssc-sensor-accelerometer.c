/*
 * libssc: Library to expose Qualcomm Sensor Core sensors
 * Copyright (C) 2022-2026 Dylan Van Assche
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "libssc-client-private.h"
#include "libssc-common-private.h"
#include "ssc-common.pb-c.h"
#include "ssc-sensor-accelerometer.pb-c.h"
#include "libssc-sensor-accelerometer.h"

enum {
	SIGNAL_MEASUREMENT,
	N_SIGNALS
};
static guint signals[N_SIGNALS];

typedef struct _SSCSensorAccelerometerPrivate {
	guint report_id;
} SSCSensorAccelerometerPrivate;

G_DEFINE_TYPE_WITH_CODE (SSCSensorAccelerometer, ssc_sensor_accelerometer, SSC_TYPE_SENSOR,
			 G_ADD_PRIVATE (SSCSensorAccelerometer))

typedef struct {
	SSCSensorAccelerometer *sensor;
	gfloat x;
	gfloat y;
	gfloat z;
} SignalContext;

static void
signal_context_free (SignalContext *ctx)
{
	g_slice_free (SignalContext, ctx);
}

/*****************************************************************************/

static gboolean
emit_signal (gpointer user_data) {
	SignalContext *ctx = user_data;

	g_signal_emit (ctx->sensor, signals[SIGNAL_MEASUREMENT], 0, ctx->x, ctx->y, ctx->z);

	return G_SOURCE_REMOVE;
}

static void
report_received (SSCClient *self, guint32 msg_id, guint64 uid_high, guint64 uid_low, GArray *buf, gpointer user_data)
{
	SscAccelerometerResponse *msg = NULL;
	SSCSensorAccelerometer *sensor = SSC_SENSOR_ACCELEROMETER (user_data);
	SignalContext *ctx = NULL;
	guint64 sensor_uid_low;
	guint64 sensor_uid_high;
	gfloat (*mount_matrix)[3];
	gfloat x;
	gfloat y;
	gfloat z;

	g_object_get (sensor,
		      SSC_SENSOR_UID_HIGH, &sensor_uid_high,
		      SSC_SENSOR_UID_LOW, &sensor_uid_low,
		      SSC_SENSOR_MOUNT_MATRIX, &mount_matrix,
		      NULL);

	if (sensor_uid_high == uid_high && sensor_uid_low == uid_low && msg_id == SSC_MSG_REPORT_MEASUREMENT) {
		msg = ssc_accelerometer_response__unpack (NULL, buf->len, (const uint8_t *) buf->data);

		if (msg == NULL) {
			g_warning ("Failed to unpack accelerometer measurement message");
			return;
		}

		if (msg->n_acceleration >= 3) {
			x = msg->acceleration[0];
			y = msg->acceleration[1];
			z = msg->acceleration[2];

			/* Emit signal in main context instead of thread's context */
			ctx = g_slice_new0 (SignalContext);
			ctx->sensor = sensor;

			/* Apply mount matrix specified in https://www.kernel.org/doc/Documentation/devicetree/bindings/iio/mount-matrix.txt */
			ctx->x = mount_matrix[0][0] * x + mount_matrix[0][1] * y + mount_matrix[0][2] * z;
			ctx->y = mount_matrix[1][0] * x + mount_matrix[1][1] * y + mount_matrix[1][2] * z;
			ctx->z = mount_matrix[2][0] * x + mount_matrix[2][1] * y + mount_matrix[2][2] * z;

			g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, emit_signal, ctx, (GDestroyNotify)signal_context_free);
		}

		ssc_accelerometer_response__free_unpacked (msg, NULL);
	}
}

/*****************************************************************************/

static void
accelerometer_close_ready (SSCSensor *sensor, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	g_autoptr (GError) error = NULL;

	if (!SSC_SENSOR_CLASS (ssc_sensor_accelerometer_parent_class)->close_finish (sensor, result, &error)) {
		g_task_return_boolean (task, FALSE);
		g_object_unref (task);
		return;
	}

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

gboolean
ssc_sensor_accelerometer_close_finish (SSCSensorAccelerometer *self, GAsyncResult *result, GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

void
ssc_sensor_accelerometer_close (SSCSensorAccelerometer *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;
	SSCClient *client = NULL;
	SSCSensorAccelerometerPrivate *priv = NULL;

	g_assert (SSC_SENSOR_CLASS (ssc_sensor_accelerometer_parent_class)->close &&
		  SSC_SENSOR_CLASS (ssc_sensor_accelerometer_parent_class)->close_finish);

	task = g_task_new (self, cancellable, callback, user_data);
	priv = ssc_sensor_accelerometer_get_instance_private (self);

	/* Stop listening for reports */
	g_object_get (SSC_SENSOR (self),
		      SSC_SENSOR_CLIENT, &client,
		      NULL);

	if (priv->report_id)
		g_signal_handler_disconnect (client, priv->report_id);

	/* Close sensor */
	SSC_SENSOR_CLASS (ssc_sensor_accelerometer_parent_class)->close (SSC_SENSOR (self), cancellable, (GAsyncReadyCallback)accelerometer_close_ready, task);
}

gboolean
ssc_sensor_accelerometer_close_sync (SSCSensorAccelerometer *self, GCancellable *cancellable, GError **error)
{
	gboolean success = FALSE;
	SyncContext ctx;

	ssc_common_init_sync_context (&ctx);
	ssc_sensor_accelerometer_close (self, cancellable, ssc_common_callback_sync_context, &ctx);
	ssc_common_wait_sync_context (&ctx);
	success = ssc_sensor_accelerometer_close_finish (self, ctx.result, error);
	ssc_common_clear_sync_context (&ctx);

	return success;
}

/*****************************************************************************/

static void
accelerometer_open_ready (SSCSensor *sensor, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	g_autoptr (GError) error = NULL;

	if (!SSC_SENSOR_CLASS (ssc_sensor_accelerometer_parent_class)->open_finish (sensor, result, &error)) {
		g_task_return_boolean (task, FALSE);
		g_object_unref (task);
		return;
	}

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

gboolean
ssc_sensor_accelerometer_open_finish (SSCSensorAccelerometer *self, GAsyncResult *result, GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

void
ssc_sensor_accelerometer_open (SSCSensorAccelerometer *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	SSCClient *client = NULL;
	SSCSensorAccelerometerPrivate *priv = NULL;
	GTask *task = NULL;

	g_assert (SSC_SENSOR_CLASS (ssc_sensor_accelerometer_parent_class)->open &&
		  SSC_SENSOR_CLASS (ssc_sensor_accelerometer_parent_class)->open_finish);

	priv = ssc_sensor_accelerometer_get_instance_private (SSC_SENSOR_ACCELEROMETER (self));

	task = g_task_new (self, cancellable, callback, user_data);

	/* Start listening for reports before opening sensor so we don't miss the first measurement */
	g_object_get (SSC_SENSOR (self),
		      SSC_SENSOR_CLIENT, &client,
		      NULL);
	priv->report_id = g_signal_connect (client,
					    "report",
					    G_CALLBACK (report_received),
					    self);

	/* Open sensor */
	SSC_SENSOR_CLASS (ssc_sensor_accelerometer_parent_class)->open (SSC_SENSOR (self), cancellable, (GAsyncReadyCallback)accelerometer_open_ready, task);
}

gboolean
ssc_sensor_accelerometer_open_sync (SSCSensorAccelerometer *self, GCancellable *cancellable, GError **error)
{
	gboolean success = FALSE;
	SyncContext ctx;

	ssc_common_init_sync_context (&ctx);
	ssc_sensor_accelerometer_open (self, cancellable, ssc_common_callback_sync_context, &ctx);
	ssc_common_wait_sync_context (&ctx);
	success = ssc_sensor_accelerometer_open_finish (self, ctx.result, error);
	ssc_common_clear_sync_context (&ctx);

	return success;
}

/*****************************************************************************/

static void
ssc_sensor_accelerometer_class_init (SSCSensorAccelerometerClass *klass)
{
	signals[SIGNAL_MEASUREMENT] = g_signal_new ("measurement",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE,
		3, G_TYPE_FLOAT, G_TYPE_FLOAT, G_TYPE_FLOAT);
}

static void
ssc_sensor_accelerometer_init (SSCSensorAccelerometer *self)
{
}

SSCSensorAccelerometer *
ssc_sensor_accelerometer_new_finish (GAsyncResult *result, GError **error)
{
	GObject *sensor;
	GObject *source;

	source = g_async_result_get_source_object (result);
	sensor = g_async_initable_new_finish (G_ASYNC_INITABLE (source), result, error);

	if (!sensor) {
		g_object_unref (source);
		return NULL;
	}

	g_object_unref (source);
	return SSC_SENSOR_ACCELEROMETER (sensor);
}

void
ssc_sensor_accelerometer_new (GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	g_async_initable_new_async (
			SSC_TYPE_SENSOR_ACCELEROMETER,
			G_PRIORITY_DEFAULT,
			cancellable,
			callback,
			user_data,
			SSC_SENSOR_DATA_TYPE, "accel",
			NULL);
}

SSCSensorAccelerometer *
ssc_sensor_accelerometer_new_sync (GCancellable *cancellable, GError **error)
{
	SSCSensorAccelerometer *self = NULL;
	SyncContext ctx;

	ssc_common_init_sync_context (&ctx);
	ssc_sensor_accelerometer_new (cancellable, ssc_common_callback_sync_context, &ctx);
	ssc_common_wait_sync_context (&ctx);
	self = ssc_sensor_accelerometer_new_finish (ctx.result, error);
	ssc_common_clear_sync_context (&ctx);

	return self;
}
