# Fingerprint series (not in the daily kernel)

Applied on top of `patches-7.3/` (0001 to 0068) for the fingerprint kernels; w-73 branch
`fp-qseecom-v2`. Config: `sfos.fragment` plus `CONFIG_TEE_QSEECOM=m`.

| patch | what | for |
|---|---|---|
| 0001 | Bartosz Wróbel's QSEECOM TEE driver as carried by the Guix port (`drivers/tee/qseecom`, the qseecom listener part of `qcom_scm.c`, mdt loader changes) | carried until he upstreams |
| 0002 | tzmem: a firmware status from the SHM bridge call becomes -EIO instead of escaping through `ERR_PTR()` as a pointer (the oops of 2026-11-09) | upstream candidate, Fixes: f86c61498a57 |
| 0003 | qseecom listener loop: bound consecutive failed rounds (16), not served ones (the template store read runs past 256 served rounds with five slots) | Wróbel |
| 0004 | qseecom: copy the request back when the client passes it INOUT (the mafp TA answers in place); replaces the test copy of 2026-11-09 | Wróbel |

Kernels: kfp (2026-11-09, 0001 plus the test copy as a hand-loaded module; three hard
hangs, root cause `reports/pro1x-fingerprint-hang-rootcause-2026-11-11.md` of the memeline
session), kfp2 (2026-11-11, 0001 to 0004, built on the e15 as `out-7.3-kfp2`; on the phone since 00:34, set_active_group with five slots on file clean, enrolment and unlock from the stock UI). Nothing of this
is sent to Wróbel or upstream before the author's read-back.

**Rollback warning (2026-11-11):** the phone's `/root/boot_a.pre-k-kfp2.img` is kernel kfp, without
0002 and 0003. With `sailfish-fpd-mafp` enabled and five slots on file, kfp hangs hard on the
daemon's first set_active_group, on every boot. Roll back to `/root/boot_a.pre-k-kfp.img` (kbd,
no TEE driver: the daemon then fails cleanly) or `systemctl disable sailfish-fpd-mafp` first.
