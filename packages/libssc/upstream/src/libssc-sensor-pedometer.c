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

#include "ssc-common.pb-c.h"
#include "ssc-sensor-pedometer.pb-c.h"
#include "libssc-client-private.h"
#include "libssc-common-private.h"
#include "libssc-sensor-pedometer.h"

/* Qualcomm sns_pedometer.proto: SNS_PEDOMETER_MSGID_SNS_STEP_EVENT_CONFIG and _SNS_STEP_EVENT */
#define SSC_MSG_REPORT_PEDOMETER_CONFIG	775
#define SSC_MSG_REPORT_PEDOMETER_STEP	1028

enum {
	SIGNAL_MEASUREMENT,
	N_SIGNALS
};
static guint signals[N_SIGNALS];

typedef struct _SSCSensorPedometerPrivate {
	guint report_id;
} SSCSensorPedometerPrivate;

G_DEFINE_TYPE_WITH_CODE (SSCSensorPedometer, ssc_sensor_pedometer, SSC_TYPE_SENSOR,
			 G_ADD_PRIVATE (SSCSensorPedometer))

typedef struct {
	SSCSensorPedometer *sensor;
	guint step_count;
} SignalContext;

static void
signal_context_free (SignalContext *ctx)
{
	g_slice_free (SignalContext, ctx);
}

static gboolean
emit_signal (gpointer user_data) {
	SignalContext *ctx = user_data;

	g_signal_emit (ctx->sensor, signals[SIGNAL_MEASUREMENT], 0, ctx->step_count);

	return G_SOURCE_REMOVE;
}

static void
report_received (SSCClient *self, guint32 msg_id, guint64 uid_high, guint64 uid_low, GArray *buf, gpointer user_data)
{
	SSCSensorPedometer *sensor = SSC_SENSOR_PEDOMETER (user_data);
	SignalContext *ctx = NULL;
	SscPedometerResponse *msg = NULL;
	guint64 sensor_uid_low;
	guint64 sensor_uid_high;

	g_object_get (SSC_SENSOR (sensor),
		      SSC_SENSOR_UID_HIGH, &sensor_uid_high,
		      SSC_SENSOR_UID_LOW, &sensor_uid_low,
		      NULL);

	if (sensor_uid_high != uid_high || sensor_uid_low != uid_low)
		return;

	/* Both carry the running total: the config event once when enabled, the step event on every step */
	if (msg_id != SSC_MSG_REPORT_PEDOMETER_STEP && msg_id != SSC_MSG_REPORT_PEDOMETER_CONFIG)
		return;

	msg = ssc_pedometer_response__unpack (NULL, buf->len, (const uint8_t *) buf->data);
	if (msg == NULL) {
		g_warning ("Failed to unpack pedometer measurement message");
		return;
	}

	/* Emit signal in main context instead of thread's context */
	ctx = g_slice_new0 (SignalContext);
	ctx->sensor = sensor;
	ctx->step_count = msg->step_count;
	ssc_pedometer_response__free_unpacked (msg, NULL);
	g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, emit_signal, ctx, (GDestroyNotify)signal_context_free);
}

static void
pedometer_close_ready (SSCSensor *sensor, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	g_autoptr (GError) error = NULL;

	if (!SSC_SENSOR_CLASS (ssc_sensor_pedometer_parent_class)->close_finish (sensor, result, &error)) {
		g_task_return_boolean (task, FALSE);
		g_object_unref (task);
		return;
	}

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

gboolean
ssc_sensor_pedometer_close_finish (SSCSensorPedometer *self, GAsyncResult *result, GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

void
ssc_sensor_pedometer_close (SSCSensorPedometer *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;
	SSCClient *client = NULL;
	SSCSensorPedometerPrivate *priv = NULL;

	g_assert (SSC_SENSOR_CLASS (ssc_sensor_pedometer_parent_class)->close &&
		  SSC_SENSOR_CLASS (ssc_sensor_pedometer_parent_class)->close_finish);

	task = g_task_new (self, cancellable, callback, user_data);
	priv = ssc_sensor_pedometer_get_instance_private (self);

	/* Stop listening for reports */
	g_object_get (SSC_SENSOR (self),
		      SSC_SENSOR_CLIENT, &client,
		      NULL);
	if (priv->report_id)
		g_signal_handler_disconnect (client, priv->report_id);

	/* Close sensor */
	SSC_SENSOR_CLASS (ssc_sensor_pedometer_parent_class)->close (SSC_SENSOR (self), cancellable, (GAsyncReadyCallback)pedometer_close_ready, task);
}

gboolean
ssc_sensor_pedometer_close_sync (SSCSensorPedometer *self, GCancellable *cancellable, GError **error)
{
	gboolean success = FALSE;
	SyncContext ctx;

	ssc_common_init_sync_context (&ctx);
	ssc_sensor_pedometer_close (self, cancellable, ssc_common_callback_sync_context, &ctx);
	ssc_common_wait_sync_context (&ctx);
	success = ssc_sensor_pedometer_close_finish (self, ctx.result, error);
	ssc_common_clear_sync_context (&ctx);

	return success;
}

/*****************************************************************************/

static void
pedometer_open_ready (SSCSensor *sensor, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	g_autoptr (GError) error = NULL;

	if (!SSC_SENSOR_CLASS (ssc_sensor_pedometer_parent_class)->open_finish (sensor, result, &error)) {
		g_task_return_boolean (task, FALSE);
		g_object_unref (task);
		return;
	}

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

gboolean
ssc_sensor_pedometer_open_finish (SSCSensorPedometer *self, GAsyncResult *result, GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

void
ssc_sensor_pedometer_open (SSCSensorPedometer *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	SSCClient *client = NULL;
	SSCSensorPedometerPrivate *priv = NULL;
	GTask *task = NULL;

	g_assert (SSC_SENSOR_CLASS (ssc_sensor_pedometer_parent_class)->open &&
		  SSC_SENSOR_CLASS (ssc_sensor_pedometer_parent_class)->open_finish);

	priv = ssc_sensor_pedometer_get_instance_private (self);

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
	SSC_SENSOR_CLASS (ssc_sensor_pedometer_parent_class)->open (SSC_SENSOR (self), cancellable, (GAsyncReadyCallback)pedometer_open_ready, task);
}

gboolean
ssc_sensor_pedometer_open_sync (SSCSensorPedometer *self, GCancellable *cancellable, GError **error)
{
	gboolean success = FALSE;
	SyncContext ctx;

	ssc_common_init_sync_context (&ctx);
	ssc_sensor_pedometer_open (self, cancellable, ssc_common_callback_sync_context, &ctx);
	ssc_common_wait_sync_context (&ctx);
	success = ssc_sensor_pedometer_open_finish (self, ctx.result, error);
	ssc_common_clear_sync_context (&ctx);

	return success;
}

/*****************************************************************************/

static void
ssc_sensor_pedometer_class_init (SSCSensorPedometerClass *klass)
{
	signals[SIGNAL_MEASUREMENT] = g_signal_new ("measurement",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE,
		1, G_TYPE_UINT);
}

static void
ssc_sensor_pedometer_init (SSCSensorPedometer *self)
{
}

SSCSensorPedometer *
ssc_sensor_pedometer_new_finish (GAsyncResult *result, GError **error)
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
	return SSC_SENSOR_PEDOMETER (sensor);
}

void
ssc_sensor_pedometer_new (GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	g_async_initable_new_async (
			SSC_TYPE_SENSOR_PEDOMETER,
			G_PRIORITY_DEFAULT,
			cancellable,
			callback,
			user_data,
			SSC_SENSOR_DATA_TYPE, "pedometer",
			NULL);
}

SSCSensorPedometer *
ssc_sensor_pedometer_new_sync (GCancellable *cancellable, GError **error)
{
	SSCSensorPedometer *self = NULL;
	SyncContext ctx;

	ssc_common_init_sync_context (&ctx);
	ssc_sensor_pedometer_new (cancellable, ssc_common_callback_sync_context, &ctx);
	ssc_common_wait_sync_context (&ctx);
	self = ssc_sensor_pedometer_new_finish (ctx.result, error);
	ssc_common_clear_sync_context (&ctx);

	return self;
}
