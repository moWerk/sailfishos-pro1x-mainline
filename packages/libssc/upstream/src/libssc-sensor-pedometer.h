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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _LIBSSC_SENSOR_PEDOMETER_H_
#define _LIBSSC_SENSOR_PEDOMETER_H_

#include <glib.h>
#include <gio/gio.h>
#include "libssc-sensor.h"

#define SSC_TYPE_SENSOR_PEDOMETER (ssc_sensor_pedometer_get_type())

typedef struct _SSCSensorPedometer {
	SSCSensor parent;
} SSCSensorPedometer;

G_DECLARE_FINAL_TYPE (SSCSensorPedometer, ssc_sensor_pedometer, SSC, SENSOR_PEDOMETER, SSCSensor);

void		 	 ssc_sensor_pedometer_new (GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);
SSCSensorPedometer 	*ssc_sensor_pedometer_new_finish (GAsyncResult *result, GError **error);
SSCSensorPedometer 	*ssc_sensor_pedometer_new_sync (GCancellable *cancellable, GError **error);
void			 ssc_sensor_pedometer_open (SSCSensorPedometer *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);
gboolean		 ssc_sensor_pedometer_open_finish (SSCSensorPedometer *self, GAsyncResult *result, GError **error);
gboolean		 ssc_sensor_pedometer_open_sync (SSCSensorPedometer *self, GCancellable *cancellable, GError **error);
void			 ssc_sensor_pedometer_close (SSCSensorPedometer *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);
gboolean		 ssc_sensor_pedometer_close_finish (SSCSensorPedometer *self, GAsyncResult *result, GError **error);
gboolean		 ssc_sensor_pedometer_close_sync (SSCSensorPedometer *self, GCancellable *cancellable, GError **error);

#endif /* _LIBSSC_SENSOR_PEDOMETER_H_ */
