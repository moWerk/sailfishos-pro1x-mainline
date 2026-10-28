# GPU performance (Adreno 610, freedreno, Mesa 25.1.8)

How changes are measured: Benchymark (harbour-benchymark 2.2.0) graphics stages at their
calibrated fixed load (`SFOS_SELFTEST_FIXED=1`), 3 runs each, after a fresh boot, screen at
50 %, phone on USB developer mode. The helper samples the GPU frequency and the qnm_gpu
interconnect vote every 200 ms. Higher scores are better.

Stages: 0 Benchy large, 3 Shader light, 4 Shader heavy, 8 Overdraw, 10 Icon storm.

## 2026-10-28: GPU memory bandwidth vote (kernel patch 0021)

sm6115.dtsi gave the GPU no interconnect path and no per-OPP bandwidth: qnm_gpu voted 0/0,
DDR ran at the CPU's and display's votes (ebi 1.68 GB/s avg, 2.5 GB/s peak). Patch 0021
adds GFX3D -> EBI with agatti's values. Under load the GPU now votes 6.88 GB/s peak.

| Stage | without (fresh boot) | with | change |
|---|---|---|---|
| Benchy large | 15577 / 12569 / 12022 | 16490 / 16851 / 16727 | +25 %, no drop after run 1 |
| Shader light | 61.9 / 64.8 / 62.3 | 67.5 / 69.2 / 68.3 | +8 % |
| Shader heavy | 3.3 / 3.1 / 3.1 | 2.8 / 2.9 / 3.0 | none (ALU bound) |
| Overdraw | 1364 / 1300 / 1200 | 1751 / 1759 / 1798 | +37 % |
| Icon storm | 30375 / 30198 / 30486 | 31813 / 32987 / 32139 | +6 % |

A second baseline earlier the same day (after hours of uptime) gave the same picture
(Benchy large 15804 / 11927 / 12251, Overdraw 1331 / 1343 / 1366).

GPU frequency was not the limit: devfreq (simple_ondemand, 320-950 MHz) sits at 950 MHz
under load in both cases. UBWC is configured (UBWC 1.0, highest bank bit 14).

## 2026-10-28: Mesa sysmem / GMEM choice (FD_MESA_DEBUG, with patch 0021)

Freedreno decides per batch between tiled rendering in GMEM and direct (sysmem) rendering.
Forcing either, 2 runs each:

| Stage | default | FD_MESA_DEBUG=sysmem | FD_MESA_DEBUG=gmem |
|---|---|---|---|
| Benchy large | 16452 / 16302 | 2298 / 2304 | 16821 / 16798 |
| Overdraw | 1835 / 1778 | 896 / 926 | 1958 / 2008 (+10 %) |
| Icon storm | 31866 / 32490 | 30171 / 30226 | 35571 / 34745 (+9 %) |
| Cascade | 114211 / 114036 | | 96376 / 92985 (-17 %) |
| Benchy small, Glow, Shader light/heavy, Tables | | | equal within noise |

Not adopted: forced GMEM wins on overdraw-heavy scenes and loses on Cascade. Worth a report
to the freedreno developers (A610: small GMEM, slow LPDDR4X), with Benchymark as a
reproducer. Sysmem alone is far slower, so the default heuristic is mostly right.

## Still to look at

- GPU frequency ramp at the start of UI animations (devfreq up-threshold / boost), which
  Benchymark's off-screen stages do not show.
- Mesa options (FD_MESA_DEBUG) as A/B with the same helper.
- Lipstick composition path (eglfs/KMS, buffer formats).
