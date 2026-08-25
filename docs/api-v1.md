# TPB9000 local status API v1

The operator panel exposes a read-only HTTP API on port 80. Version 1 of the
schema is additive: existing fields will retain their meaning, and future
sensor integrations will populate the existing `null` fields instead of
renaming them.

Responses use `Content-Type: application/json`, disable caching, and allow
cross-origin reads for the unauthenticated local dashboard.

## `GET /api/info`

Returns device identity, build information, and installed capabilities.

```json
{
  "schema_version": "1.0",
  "device": "TPB9000 Operator Panel",
  "board": "Waveshare ESP32-S3-Touch-LCD-4.3B-BOX",
  "firmware_version": "9d01e8b-dirty",
  "project_name": "tpb9000_operator_panel",
  "build_date": "Aug 20 2026",
  "build_time": "10:00:00",
  "esp_idf_version": "v6.1-dev",
  "capabilities": {
    "environment": true,
    "power": true,
    "ambient_light": false,
    "operator_presence": false,
    "hopper": true,
    "buzzer": false
  }
}
```

## `GET /api/status`

Returns the most recent live snapshot. A missing, failed, or not-yet-installed
sensor is represented by `available: false` and `null` measurements. Numeric
zero is reserved for a real measured zero.

```json
{
  "schema_version": "1.0",
  "machine": {
    "online": true,
    "uptime_seconds": 123.456,
    "wifi_rssi_dbm": -61
  },
  "environment": {
    "available": true,
    "temperature_f": 74.25,
    "humidity_percent": 49.50,
    "pressure_hpa": 986.10,
    "ambient_lux": null,
    "updated_ms": 123400
  },
  "power": {
    "available": false,
    "bus_voltage_v": null,
    "current_a": null,
    "power_w": null,
    "updated_ms": null
  },
  "hopper": {
    "available": true,
    "percent": 71.3,
    "distance_cm": 15.4,
    "bars": 5,
    "status": "Normal",
    "updated_ms": 123405
  },
  "display": {
    "brightness_percent": 100,
    "automatic_brightness": false,
    "operator_present": null,
    "last_refresh_ms": 123410
  }
}
```

All `updated_ms` fields and `last_refresh_ms` are milliseconds since the
current boot, not wall-clock timestamps.

Hopper `status` is `Normal` for 4-6 bars, `Low` for 2-3 bars, `Critical` for
one bar, `Empty` for zero bars, and `Unavailable` when no recent valid UART
frame exists. The reported distance is a rolling median. Percentage uses the
configured full/empty calibration distances rather than a fixed bar mapping.
