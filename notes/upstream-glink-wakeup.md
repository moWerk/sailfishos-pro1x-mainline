# Upstream prep: glink-smem edge wakeup (0058-0060)

Status 2026-11-02 22:50: written, **nothing sent**. Needs the author's read-back and
Signed-off-by before anything goes out.

## The series

| | Patch | checkpatch --strict |
|---|---|---|
| 1/3 | `dt-bindings: remoteproc: qcom,glink-edge: Allow wakeup-source` (0059) | only the missing Signed-off-by and the long Disclosure line |
| 2/3 | `rpmsg: glink: smem: Let an edge wake the system from suspend-to-idle` (0058) | same |
| 3/3 | `arm64: dts: qcom: sm6115-fxtec-pro1x: Wake up on messages from the modem` (0060) | same |

Binding first, driver, then the DT user (usual order). The DT patch depends on the board
DT having the system power domain (0050), which is not upstream; without it the board never
reaches the state where the wakeup matters. The series can go without 3/3, or with 3/3 as
the example user and a note that the benefit shows only with a system-level idle state.

get_maintainer: Bjorn Andersson, Mathieu Poirier (remoteproc/rpmsg), Rob Herring, Krzysztof
Kozlowski, Conor Dooley (binding), Konrad Dybcio (qcom DT); linux-arm-msm,
linux-remoteproc, devicetree, linux-kernel.

## Review history to answer

- 2024-06-03, Deepak Kumar Singh: "[PATCH V1] rpmsg: glink: Make glink smem interrupt wakeup
  capable" (https://www.mail-archive.com/linux-kernel@vger.kernel.org/msg2569356.html).
  Every edge, unconditionally: dropped IRQF_NO_SUSPEND, `device_init_wakeup(dev, true)` and
  `dev_pm_set_wake_irq()` on the edge device. Use case: charger plug-in, the DSP sends a
  pmic-glink message.
- 2024-06-18, Bjorn Andersson's reply
  (https://www.mail-archive.com/linux-kernel@vger.kernel.org/msg2570387.html): against
  unconditional wakeup, "we don't want a laptop-like device to wake up in someones backpack
  and overheat"; points at gaps in ModemManager; leans towards making it configurable rather
  than always on, possibly per channel. Not merged.

How this series answers it:
- Opt-in per edge in DT (`wakeup-source`), off for every existing board. A laptop DT simply
  does not set it; a phone sets it on the modem edge only (here: mpss, not adsp/cdsp).
- Userspace can still turn it off: the wakeup source is visible in
  `/sys/kernel/debug/wakeup_sources` (no sysfs `power/wakeup` knob, see below; a reviewer
  may ask for one).
- Expected objection: **per channel, not per edge.** On the modem edge, QRTR carries every
  QMI service (NAS, WDS, voice, WMS, IMS); a per-channel switch would need the rpmsg client
  (qrtr) to declare it, which is what downstream does (qrtr wakeup source). Our measurement
  for the cost of per-edge: 2 modem-edge wakeups per hour idle, sleep current unchanged
  (40.0 mA / 60 min against the same without). Have that number ready.
- Also likely: "use `dev_pm_set_wake_irq()`" (as the 2024 patch). Answer: that makes the edge
  device wakeup capable, which registers a wakeup class device as a child of the edge, and
  `qcom_glink_native_remove()` unregisters every child of the edge: on every remoteproc
  recovery the class device is freed and then unregistered again (oops in `device_del`). Our
  first version did exactly that and was caught by a modem crash on the device; the 2024
  patch would very likely have hit the same on SSR. A standalone wakeup source avoids it.
  Worth saying in the cover letter, it shows SSR was tested.

## Cover letter draft (the author edits; facts only from the device logs)

```
Subject: [PATCH 0/3] rpmsg: glink: smem: Opt-in wakeup from an edge

On phones whose modem talks to the host over glink-smem, an incoming SMS
or call during suspend-to-idle is only seen at the next unrelated wakeup:
the edge interrupt is IRQF_NO_SUSPEND, so it is handled, but nothing
reports a wakeup and the CPU goes straight back to sleep.

A 2024 patch made every edge wakeup capable and was not taken, because a
laptop should not wake up in a bag. This series makes it opt-in per edge
with the standard wakeup-source property, so only a board that wants it
(here: a phone, modem edge only) gets it.

The wakeup source is standalone rather than the edge device's own: glink
removes every child of the edge device on remoteproc stop, which would
include a wakeup class device and oops in device_del() on every modem
recovery.

Tested on the F(x)tec Pro1-X (SM6115) with a system-level idle state:
an SMS sent while suspended on battery showed up within 6-20 s (before:
at the next RTC alarm, minutes later); a call rang before the caller
heard the ringback. Idle cost: about 2 extra wakeups per hour from the
modem edge, no change in sleep current within measurement noise. Modem
recovery (remoteproc debugfs crash) completes.
```

Plus the Disclosure/LLMGD trailers per patch (the long Disclosure line trips checkpatch's
75-column warning; wrap it or accept the warning, the author's call).

## Before sending (checklist)

- [ ] Author read-back of all three patches and the cover letter.
- [ ] Signed-off-by from the author on each patch.
- [ ] Rebase on the current rpmsg-next / linux-next (written against 7.3), re-run
      checkpatch, `dt_binding_check`, `dtbs_check` for the Pro1-X dtb.
- [ ] Decide on 3/3 (send now, or after 0050 is upstream).
- [ ] Lengthy SSR test with the SIM in (the silent 900e after recovery,
      `modem-recovery-crash.md`, is not caused by 0058, but a reviewer will ask about SSR:
      say what is and is not known).
- [ ] b4 prep / send with the author's identity.
