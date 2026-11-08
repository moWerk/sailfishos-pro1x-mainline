# GNSS and AGPS

Status 2026-11-07 01:15: **native QMI provider installed** (`packages/geoclue-provider-hybris-qmi`,
Jolla's geoclue-hybris with a QMI LOC backend in place of the Android HAL); lifecycle tested
indoors, a fix through it still to be seen outdoors. Route 1 (gpsd) is removed from the
phone. The modem gets a fix under open sky (2026-11-06, 1 Hz all through a drive).

## What is proven (on the device, headless, indoors)

- The modem offers the QMI location service: service 16 (LOC v2) on QRTR node 0, port 68
  (`qrtr-lookup`).
- `qmicli` 1.34 (libqmi-utils) talks to it: operation mode reads `standalone`, NMEA types
  were `none` and can be set (`--loc-set-nmea-types="gga|gsa|gsv|rmc|vtg"`, now
  `gga, rmc, gsv, gsa, vtg`). A session started by one qmicli call ends with that process:
  over QRTR a QMI client is a socket of its own, so `--client-no-release-cid` does not
  carry over, and qmicli takes only one LOC action per call. A start plus follow therefore
  needs one process.
- `scripts/qmi-loc-probe.py` is that process (python3 with ctypes, no libqmi bindings
  needed): REG_EVENTS (0x21) with position, SV info and NMEA, START (0x22, periodic, 1 s),
  STOP (0x23). In 3 minutes the engine sent, at 1 Hz: NMEA indications (0x26: GPGGA, GPRMC,
  GPGSA, GPVTG, all without a fix), position reports (0x24), GNSS SV info (0x25) and three
  0x2d indications. Usage: `python3 qmi-loc-probe.py 0 68 180` (node and port from
  `qrtr-lookup`, they can change after a modem restart).
- The NMEA types do not survive a modem restart: after the 2026-10-30 reboots they read
  `none` again and a started session sent no NMEA at all until they were set. Whatever runs
  the session (the bridge below) has to set them before every START.
- SV info carried no satellite list after 3 minutes indoors (only TLV 0x01). That says
  nothing yet about the antenna: an unassisted cold start takes minutes under open sky.

Next on the device: the same probe by a window and outside, cold and with assistance
(below), timing the first satellites and the first fix.

## Sky test 2026-10-30: mainline hears no satellites, hybris gets a fix

Both Pro1-X side by side on the patio with open sky:

- **Mainline (this port):** 50 min of a periodic session (`scripts/qmi-loc-log.py`), NMEA
  types set, engine lock set to none after 45 min: not one satellite. No GSV sentence, SV
  info indications every second with no satellite list, position reports 1 Hz with status
  "in progress" and only a cell-based position (technology mask CELLID, uncertainty about
  180 km). The modem is online, has valid UTC time and the session is accepted.
- **Hybris (SFOS 5.0 halium, Android LOC HAL):** fix in Pure Maps within minutes at the
  same spot, XTRA download failing (DNS), so unassisted apart from Wi-Fi AP injection.
- So the hardware and the antenna can do it; something the Android side does is missing.
  What its HAL does before START (logcat): SET_ENGINE_LOCK, REG_EVENTS, SET_OPERATION_MODE,
  then START; the peripheral manager "votes" for the modem for GPS (the subsystem stays up,
  which remoteproc does anyway); the GPS service is registered in the modem's root PD
  (`msm/modem/root_pd/gps/gps_service`); Wi-Fi AP data injected every few seconds.
- Next: read the hybris device tree (`/proc/device-tree`) and GPIO state for an AP-side
  GNSS LNA supply or enable line that mainline's DT lacks; then repeat the HAL's exact
  request sequence (operation mode, event mask) on mainline.

## Cause found: the modem had no MCFG (2026-10-30)

At every start the modem asks over QRTR TFTP for
`/readonly/firmware/image/modem_pr/mcfg/configs/mcfg_hw/mbn_hw.dig` and `mcfg_sw/mbn_sw.dig`.
tqftpserv looks for them next to modem.mbn and rejected both ("unable to open ..., reject"):
the modem then runs on built-in defaults. Calls and data work that way; GNSS does not hear a
single satellite. Android mounts the modem partition (`modem_a`, vfat) at
`/vendor/firmware_mnt`; the port now does the same with `qx1050-modem-pr.service`, which
mounts it read-only and binds `image/modem_pr` next to modem.mbn before tqftpserv starts.

After a reboot with the files served, same patio: the first satellite after about 60 s, then one
at a time (GPS 21 at 23-30 dB-Hz, later GPS 30 at 18-25), no fix in 12 min. Hybris got a fix at the
same spot, so the signal level is the next question (all constellations enabled? the other
Pro1-X's antenna?), together with assistance (XTRA, time, position).

## Hot start versus cold start; rmtfs now keeps the EFS (2026-10-30)

GPSInfo on the hybris Pro1-X: 5 of 35 satellites within seconds, fix in 10-20 s. That is a
hot start: its modem keeps orbits, almanac and last position in its EFS (modemst1/2) across
boots. Mainline ran rmtfs read-only (`-r -P`), so the modem's EFS writes lived only in rmtfs's
memory and every boot was a true cold start; with this antenna that means 1-2 satellites at a
time (best GLONASS 5 at 34 dB-Hz off the charger, the charger seemed to cost a few dB) and no
fix in 50 minutes. rmtfs now serves files in /var/lib/rmtfs (root only), seeded once from
the partitions, which stay read-only (`droid-config-qx1050/sparse/etc/systemd/system/rmtfs.service.d/`,
`qx1050-rmtfs-seed.sh`).

Assistance on this modem (MPSS.HA.1.1-00140):
- INJECT PREDICTED ORBITS DATA (0x35): QMI error 94, not supported. INJECT XTRA DATA (0xA7):
  every part accepted, the last part's indication reports status 1 (general failure) with the
  version check off or XTRA3, and GET PREDICTED ORBITS DATA VALIDITY then says nothing is valid.
  The hybris HAL's own XTRA download fails too (DNS), so XTRA is not what makes hybris fast.
  Still open.
- INJECT UTC TIME (0x38) unrequested right after the client starts: status 1. The modem has
  not sent an INJECT TIME REQUEST in any session (it gets time from the network).
- DELETE ASSIST DATA (0x44) and DELETE GNSS SERVICE DATA (0xA6) with "delete all": QMI error
  0x73 on the hybris modem while its HAL runs; a cold-start comparison there is still open.
- `scripts/qmi-loc-assist.py` is the session tool (assistance on the modem's requests,
  satellites per system with SNR); `scripts/qmi-loc-coldstart.py` finds the LOC service without
  qrtr-lookup and tries the deletes.

## First fix on mainline (2026-11-06)

pro1x#2 on the patio, open sky, Wi-Fi only (no SIM), kernel 7.3 with 0061/0062, MCFG served and
rmtfs keeping the EFS since 2026-10-30. A one-process LOC client (REG_EVENTS position, SV info,
NMEA, engine state, fix session state; START periodic 1 s with intermediate reports):

- START at 18:59:33; position reports "in progress" with no satellite list at first; the first
  report with session status 0 (success) by 19:01:02, horizontal uncertainty 52 m, then 3-5 m
  within 30 s; SV info lists 39 satellites (all systems, used and tracked).
- Not a true cold start: the EFS kept since 2026-10-30 likely still held almanac and the last
  position (the last sessions on this phone were the 10-30 patio tests). A cold start with
  the assistance data deleted is still to be timed.
- No NMEA indications came in this session: the NMEA types were not set after the boot (see
  above, they reset with the modem). Position reports carry the fix without NMEA.
- An earlier qmicli `--loc-start` the same evening left no session behind (it ends with the
  process), as documented above.

## Route 1 works: LOC -> pty -> gpsd -> geoclue (2026-11-06, prototype)

- `scripts/qmi-loc-gpsd-bridge.py OUTDIR MINUTES /run/qx1050-gnss` (root): sets the NMEA types
  with qmicli (global, reset by a modem restart), runs one LOC session (REG_EVENTS, START
  periodic 1 s) and writes each NMEA sentence with CRLF to a pty whose slave it links at
  `/run/qx1050-gnss` (raw mode, master non-blocking so a missing reader never stalls the
  session).
- `gpsd -N -n -F /run/gpsd.sock /run/qx1050-gnss` (jolla repo gpsd 3.26.1): TPV mode 3 at 1 Hz.
- `geoclue-provider-gpsd3` 0.2 from nemo:devel:hw:native-common (needs `libgps` from the jolla
  repo, libgps.so.31, same gpsd 3.26) installed with `--no-gpg-checks`; geoclue-master must be
  restarted (it reads the provider list once). Then, as defaultuser on the session bus,
  Master.Create, SetRequirements(6, 0, true, 1023), PositionStart: the master picks "GPSd3
  Provider" and GetPosition returns latitude, longitude and altitude with accuracy level 6
  (detailed), horizontal 24 m. Apps that held a client of the old master need a restart.
- **gpsd's `udp://` source does not work for this:** gpsd reads the UDP socket blocking, so
  between packets it sits in `read()` and never answers a client on port 2947 (connect works,
  no VERSION banner; strace shows only reads and the D-Bus fix signals). The pty works.
- Still to do: a systemd unit (gpsd and the bridge, the session running only on demand, or
  the native provider below), the status bar GPS indicator check, packaging in
  droid-config-qx1050, power cost of a running session, a measured cold start.

## Drive test and the native provider (2026-11-06/07)

- Drive with route 1 (19:2x-21:52): the modem reported a fix every second all the way (8898 of
  9601 reports success; 300 of 300 per 5 min at up to 140 km/h; one gap of about 10 min
  leaving the house). Pure Maps moved only about once a minute. Replaying the recorded NMEA
  through gpsd into a QML PositionSource gave 1 update per second, so gpsd and geoclue were
  not dropping fixes; the speed reached Qt as invalid and the accuracy was the provider's
  fixed 24 m. geoclue-provider-gpsd3 also stops for good when gpsd restarts (`gpsd_poll`
  returns FALSE on a read error and never reconnects). Pure Maps' own handling remains the
  suspect for the once-a-minute updates; it can only be rechecked on a drive now.
- `packages/geoclue-provider-hybris-qmi`: mer-hybris/geoclue-providers-hybris at b126f585
  plus `qmi/qmilocationbackend.{h,cpp}` behind the existing `HybrisLocationBackend`
  interface, so Settings' hybris switch, the GPS status icon, NTP time and the position
  injection from other providers work unchanged. The backend:
  - one QRTR socket (no privileges needed, so no suid bit); NEW_LOOKUP for service 16 stays
    registered, and DEL_SERVER/NEW_SERVER from the name service handle a modem restart
    (registration and session are sent again); socket errors reopen the socket.
  - REG_EVENTS position, SV info, inject time request, engine state, fix session state; START
    session 1, periodic, intermediate reports off, interval from the provider (min 1 s); STOP
    when the provider stops.
  - position report 0x24 only with session status success: latitude/longitude, altitude above
    the ellipsoid (0x1A), speed (0x18, m/s to knots as the provider expects), heading (0x20),
    vertical speed, circular horizontal uncertainty (0x12; the earlier scripts printed 0x13,
    the elliptical minor axis), vertical uncertainty (0x1C), UTC timestamp (0x25). Satellites
    from 0x25 (28-byte entries; QMI's ids already use the per-system ranges geoclue-hybris
    uses), used ones from 0x24 TLV 0x2C.
  - time injection 0x38 (uncertainty at least 1000 ms) when the modem asks; position
    injection 0x39 (at least 1000 m) from the provider's existing injection path. XTRA,
    aiding data delete, SUPL: stubs for now.
- Tested 2026-11-07 indoors (no fix there): geoclue-master spawns it, the LOC service is
  found, a client starts the session (engine on), the client leaving stops it (engine off),
  the provider exits after 30 s idle. Not yet tested: a fix through it, a modem restart while
  a session runs, time/position injection replies, the status bar icon, Pure Maps on a drive.

## What SailfishOS offers

- geoclue 0.12 (Jolla's) with providers; Jolla's own GPS provider is
  `geoclue-provider-hybris-binder` (Android location HAL, not usable on mainline).
- In the jolla repository: `gpsd` 3.26.1 (+ clients, python3-gpsd), `geoclue-provider-mlsdb`
  (offline cell location database), `-gsmloc`, `-hostip`, `-manual`.
- Without any provider the Settings page shows only the location switch: the accuracy modes
  are visible when `locationProviders` is not empty. The port now installs
  geoclue-provider-mlsdb (offline cell/Wi-Fi); the status bar GPS icon needs a GPS provider.
- `geoclue-provider-gpsd3` is not in the configured repositories; nemo:devel:hw:native-common
  ships it (the PinePhone ports use it).

## Bridge to geoclue: two designs

1. **gpsd route (fast to a first fix):** a small daemon runs the LOC session and writes the
   NMEA sentences to a pty; gpsd reads the pty; geoclue-provider-gpsd3 reads gpsd.
   Everything except the daemon is packaged. Drawback: the session runs whenever gpsd
   runs, not only when an app asks for location, which costs power, and assistance
   injection still needs our daemon.
2. **Native geoclue provider (the end state):** a provider that speaks QMI LOC itself,
   like hybris-binder does with the Android HAL: it starts the session when geoclue has
   clients and stops it after, reports position, velocity and satellites from the
   indications directly (no NMEA parsing), and owns the assistance. Recommended once
   route 1 has proven fixes outside.

## AGPS plan after the 2026-11-06 research (steps 1-4 tested 2026-11-07, see below)

Research only (libqmi `qmi-service-loc.json`, ModemManager `mm-shared-qmi.c`, Qualcomm's
`location-apis-qcom` `location_service_v02.h` / `LocApiV02.cpp`, Quectel's RG50xQ GNSS
application note). Findings:
- Our INJECT XTRA DATA (0xA7) is built like ModemManager's (0x01 total size u32, 0x02 total
  parts u16, 0x03 part number u16 from 1, 0x04 data, at most 1024 bytes per part; 0x35 first,
  0xA7 on error 94). So the final status 1 is the modem refusing content, time or
  configuration, not a framing error.
- Corrections to the notes above: 0xA8 is INJECT XTRA PCID; the XTRA status query is
  QUERY XTRA INFO 0xB0. The 0x2D indications seen on 10-30 are WIFI_REQ (the modem asks for
  a Wi-Fi position).

Order to try, each from a forced cold start (0xA6 delete) so the gain is measurable:
1. **QUERY XTRA INFO (0xB0)**, TLV 0x01 xtraConfig u32 = 0. Its indication's report mask says
   whether XTRA is disabled in the modem (0x80), whether the modem wants time (0x10 with
   timeRequest), and lists XTRA and NTP servers; also file version, generation time and
   remaining validity. Separates the three hypotheses below in one call. Also register event
   bit 45 and watch indication 0xD6 (Qualcomm's HAL registers it next to the predicted orbits
   request).
2. **INJECT UTC TIME (0x38) before XTRA:** 0x01 u64 ms since the Unix epoch (UTC), 0x02 u32
   uncertainty ms (ModemManager 1000, Quectel 3500; ours was 500), 0x10 time source u32
   (1 AP, 2 NTP; ours had none). Quectel's order: time, then XTRA, engine off.
3. **XTRA (0xA7)** with optional 0x10 formatType u32 0 and 0x11 fileInfo (u32 type 1, u32
   interval min); part size from GET PREDICTED ORBITS DATA SOURCE (0x36), never above 1024;
   decode the last indication fully (0x10 part number, 0x11 accepted constellation mask, sent
   only on the last part and only if something was accepted). File from the URL that 0x36
   names; known variants xtra3grc.bin / xtra3grcej.bin (xtrapath1-6.izatcloud.net),
   xtra3Mgrbeji.bin (path1.xtracloud.net, NTP time.xtracloud.net). XTRA 3 files are signed.
4. **GET PREDICTED ORBITS DATA VALIDITY (0x37)** to confirm; recheck whether its start time is
   UTC or GPS time.
5. **INJECT POSITION (0x39)** on the modem's 0x2A/0x2D requests or at START, seeded from mlsdb
   or the last fix: 0x10/0x11 lat/lon double, 0x12 horizontal uncertainty float (at least
   1000 m, as ModemManager advises), 0x13 confidence u8 68, 0x1B timestamp ms UTC, 0x1D source
   u32 6 (other; our script did not send 0x1D).
6. SUPL without a SIM over Wi-Fi: the IDL has a WLAN bearer for location server connections,
   but nothing implements it. Last.

Why the 10-30 XTRA failed, ranked guesses: (1) no valid GPS time without a SIM, so an XTRA 3
file's signature or window cannot be checked: test by injecting right after a fix, when the
engine has exact GPS time; (2) XTRA disabled in the modem configuration (0xB0 bit 0x80; no
QMI message enables it, so that would be NV/EFS); (3) wrong file variant for this firmware.
Baseline still missing: whether the stock hybris HAL can inject XTRA on this firmware at all
(its download failed on DNS); a hosts-file fix on the hybris Pro1-X answers that.

## Stock 2.1.7 image and the first AGPS steps (2026-11-07, indoors, no fix)

Source: F(x)tec's last stock release, `Release_SM6115_S701_gms_V2.1.7_20230417-1049_userdebug`,
extracted read-only on the e15 into `~/sailfish-build/stock-2.1.7/` (target_files zip,
NON-HLOS, boot, dtbo, vmlinux; persist.img left in the archive). The Qualcomm files stay there,
none are committed here.
- The stock modem is `MPSS.HA.1.1-00140-KD_ALL_PACK-1`, the same build as
  `/lib/firmware/qcom/sm6115/Fxtec/QX1050/modem.mbn` on the device. Results with that
  firmware carry over to stock.
- Stock `gps.conf` names no XTRA or SUPL server: only `NTP_SERVER=time.xtracloud.net`,
  `SUPL_VER=0x10000`, `CAPABILITIES=0x17`, `INTERMEDIATE_POS=0`. The `xtra-daemon` has no
  built-in URLs either. The modem supplies them (below). `izat.conf` sets `GTP_MODE=DISABLED`
  and `SAP=MODEM_DEFAULT`; nothing in it is needed without the Qualcomm daemons.

Measured on pro1x#2 (`scripts/qmi-loc-xtra-query.py`, `scripts/qmi-loc-xtra-inject.py`
with `SESSION=1` / `VERBOSE=1`; plain QRTR, no fix since the day before):
1. **GET PREDICTED ORBITS DATA SOURCE (0x36)** works. It lists
   `https://path{1,2,3}.xtracloud.net/xtra3grcej.bin` and no allowed-size TLV.
2. **QUERY XTRA INFO (0xB0)** works. Report mask 0x2c: XTRA server list, NTP server list
   (`time.xtracloud.net` three times) and preferred valid age 168 h. Bit 0x80 (XTRA disabled)
   is clear and bit 0x01 (data age) is clear, so XTRA is enabled and no data is loaded. This
   rules out guess (2) above.
3. **INJECT UTC TIME (0x38)** is accepted at the response level, but its indication says
   general failure every time: uncertainty 1000, 500, 100, 30 and 0 ms; source TLV 0x10 = 1
   and 2; with and without a running session. Leaving out 0x02 gives response error 0x11.
4. **INJECT PREDICTED ORBITS DATA (0x35)** is refused with error 94 (not supported), so
   **0xA7** is the message on this firmware.
5. **INJECT XTRA DATA (0xA7)** in 1024-byte parts: every part up to the second to last gets
   status 0 with its own part number in 0x10. The last part gets status 1 (general failure)
   with 0x10 = 2 and no 0x11. The result is the same for `xtra3grcej.bin` (the modem's own
   choice, 47 KB), `xtra2.bin` and `xtra3gr.bin`, with and without a session. The modem
   assembles the file and rejects it as a whole; the final 0x10 looks like an error code, not
   a part number.
6. **GET PREDICTED ORBITS DATA VALIDITY (0x37)** stays at general failure: no data.

So framing, file variant and the XTRA switch are fine.

**Cause found 2026-11-08: the engine lock.** The lead came from the Guix port
(mlaparie/fxtec-pro1x-guix 93a9b7f, taken from nyymi609's NixOS port bb65696). It sets the
lock to none at every boot, because Android's location HAL does the same.
- **GET ENGINE LOCK (0x3B)** answers in the newer format: no lock type TLV 0x10, but 0x12, a
  u64 lock-client mask, set to 1. That is enough to block every injection while mobile-initiated
  fixes still work, which is why our fixes never showed the lock.
- **SET ENGINE LOCK (0x3A)**, TLV 0x01 u32: libqmi's `QmiLocLockType` starts at 1 (NONE=1,
  MI=2, MT=3, ALL=4). The value 0 gives INVALID_ARG (0x30). `qmicli -d qrtr://0
  --loc-set-engine-lock=none` sends 1 and is accepted. The mask reads 0 afterwards.
- **After the unlock, indoors and with no fix:**
  - INJECT UTC TIME gets status 0 for source 1 and for source 2.
  - The same `xtra3grcej.bin` is accepted.
  - GET PREDICTED ORBITS DATA VALIDITY now has TLV 0x10: u64 start in **UTC seconds**
    (2026-11-07 09:00, the file's generation time) and u16 duration of 168 h.
- **Persistence:** rmtfs keeps the EFS writable here (`-o /var/lib/rmtfs`), so the unlock should
  survive a reboot. This is not checked yet. The Guix and NixOS ports run rmtfs read-only and
  repeat the unlock at every boot.

Done since: the unlock runs in `qx1050-modem-prep.sh` at every modem start. The provider
downloads XTRA from `/etc/gps_xtra.ini` (the URLs 0x36 names) and injects it part by part.

## Time to first fix with and without XTRA (2026-11-08, patio, native provider)

Setup:
- Kernel kbd, provider 0.2.24-2, and a QML PositionSource client (`scripts/gnss-test-session.sh`
  logic, A/B in a script on the phone).
- Each run starts with `qmicli --loc-delete-assistance-data`, a cold start. That also deletes
  the XTRA data, the time and the position.
- The provider injects NTP time when the modem asks, in both kinds of run.
- "No XTRA" means the provider ran without `/etc/gps_xtra.ini`. Whether a run really had XTRA
  comes from the provider's journal ("XTRA injected"), not from the script's label (see the
  pitfall below).
- Screen kept on (mce stay-on), on a power bank. Up to 43 satellites were visible.

| XTRA injected | first fix (s) |
|---|---|
| yes | 12, 13, 14, 133 |
| no (time only) | 73, 97, 183, 318, none in 600, none in 720 |

Median about 13 s with XTRA against about 4 min without. Without XTRA, 2 of 6 runs did not get a
fix in 10-12 min. XTRA arrives about 1 s after the start (47 KB over Wi-Fi), well before the
first fix.

Pitfalls found while measuring (all in the test setup, not the provider):
- **The QML client's environment.** Copying lipstick's environment copies its
  `QT_QPA_PLATFORM=eglfs`; the client then dies at start ("OpenGL windows cannot be mixed").
  geoclue-master drops the provider within milliseconds, which looks exactly like "engine on,
  engine off" in the provider log. The client needs `QT_QPA_PLATFORM=wayland`.
- **A D-Bus poller starts the provider.** `GetSatellite` on the provider's bus name activates
  it, and the provider reads `gps_xtra.ini` only at startup. A poller running between runs
  therefore decides which configuration the next run gets.
- **Suspend during a session.** With the screen off, autosleep suspended the phone during
  GPS sessions; minutes went by between wakes. Nothing holds the system awake while a session
  runs. Open: the provider (or geoclue-master's client) should hold a keepalive while
  positioning, as the Android HAL holds its wakelock.
- The modem sends satellite reports (0x25) only to the client that owns the session; a second
  QMI client that registers only for them receives nothing.

## AGPS options (all through QMI LOC; names as in libqmi's `qmi-service-loc.json`,
message IDs to be checked there before use)

- **XTRA predicted orbits:** the modem names its download servers (GET PREDICTED ORBITS
  DATA SOURCE); the AP downloads the file (xtra3grc.bin style, a few hundred kB, valid for
  days) and injects it in chunks (INJECT PREDICTED ORBITS DATA). Refresh daily over Wi-Fi
  or data. This is the largest gain for a cold start and needs no operator support.
- **Time injection** (INJECT UTC TIME) from the AP clock after NTP: cheap, helps every
  start.
- **Coarse position injection** (INJECT POSITION) from the mlsdb cell location or the last
  fix: cuts the search further.
- **SUPL (MS-based/MS-assisted)** via SET SERVER and operation mode MSB: the modem needs a
  data path to the SUPL server and asks the AP for it (location server connection
  indications), so it needs WDS work; worth it later, XTRA plus time plus position first.
- Delete assistance data (`--loc-delete-assistance-data`) gives a reproducible cold start
  for measuring each step.

## Related

- The modem DOG crash (`notes/modem-dog-crash.md`) delays the whole modem, LOC included, by
  40 s per failed start.
