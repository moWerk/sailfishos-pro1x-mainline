# mce-plugin-libhybris: multicolor LED backend

Applies to mer-hybris/mce-plugin-libhybris tag 1.14.5 (the version SailfishOS 5.2 ships,
built without libhybris). Built in the Platform SDK as `1.14.5+qx1050.3` (spec Version only)
so it replaces the installed package.

The Pro1-X notification LED is one multicolor LED (`rgb:status`, PMI632 LPG). The patch adds
a `multicolor` backend: color to `multi_intensity`, on/off through `brightness`, blinking with
its own timer (the LPG's hardware blink shows a color sweep for multicolor LEDs, see
`notes/upstream.md`), breathing through mce's sw breathing. Selected in
`sparse/etc/mce/93-qx1050-notification-led.ini`.
