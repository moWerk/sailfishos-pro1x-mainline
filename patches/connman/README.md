# connman (Jolla's 1.40+git26.6, SailfishOS 5.2)

`0001-sailfish_wifi-autoscan-hold-off-as-a-plain-timer.patch`: the Wi-Fi autoscan hold-off was a
wakeup timer and woke a suspended phone about once a minute (notes/sleep.md). Built from
github.com/sailfishos/connman at tag 1.40+git26.6 with the patch applied in the tree, spec
Version 1.40+git26.6, Release 100.qx1050 (above Jolla's), in the 5.1.0.11 SDK target with
libgsupplicant 1.0.31 from the 5.2.0.22 repository put into the target by hand (git26.6 needs
>= 1.0.31, the 5.1 target has 1.0.30). `make check` passes. Not device-tested yet.

This is a SailfishOS-wide fix (any device whose Wi-Fi rescans around a suspend), not a
Pro1-X one: the candidate for an upstream PR to sailfishos/connman after the author's read-back.
