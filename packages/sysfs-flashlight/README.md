# sysfs-flashlight

The flashlight provider for the top-menu toggle on a native (non-hybris) SailfishOS port.

SailfishOS 5.2 splits the flashlight in two: `jolla-settings-system-flashlight` is only the
toggle (QML), and it requires a `sailfish-flashlight-provider`: a session D-Bus service
`org.sailfish.flashlight.provider` (`toggleFlashlight`, `flashlightOn`,
`flashlightOnChanged`). On hybris devices that is `droidmedia-flashlight`, which turns on
the torch through the Android camera service.

This package is Jolla's `droidmedia-flashlight` (sailfishos/droidmedia `flashlight/`, commit
in `SOURCE`, Apache-2.0) with that one backend call replaced: the torch LEDs listed in
`/etc/sysfs-flashlight.conf` are switched through their LED class `brightness` files. The
resource policy handling (RearFlashlightType) and the two-minute idle exit are unchanged.

```ini
[Torch]
Leds=white:flash-0
Brightness=255
```

The user session must be able to write those files (on the Pro1-X a udev rule gives the
`video` group write access).
