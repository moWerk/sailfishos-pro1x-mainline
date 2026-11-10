# qsee-supplicant (packaged)

Dawid Wróbel's [qsee-supplicant](https://github.com/wrobelda/qsee-supplicant), commit
36e0668, in `upstream/` unchanged, with an mb2 spec in `rpm/`. It provides the FS (id 10)
and GPFS (id 28672) listener services the Pro1-X fingerprint trusted application uses for
its template store, and the loader that keeps the application resident.

On the device: `qsee-supplicant.service` (state under `/var/lib/qsee-supplicant`, where the
templates land as `data/system/users/0/fpdata/group-0-template-N.data`) and
`qsee-app-loader@mafp.service`, which needs `/lib/firmware/mafp.mbn` from the stock vendor
image (not redistributable, not in this repository) and the qseecomtee driver (kernel
`CONFIG_TEE_QSEECOM=m`, the fp-qseecom series).

State: binaries of this commit ran on the device on 2026-11-09 (static builds by hand);
the RPM is built with mb2 on 2026-11-11, first installed state in `sailfish-fpd-mafp/README.md`.
