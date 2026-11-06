# GNSS and AGPS

Status 2026-11-06 19:20: **mainline gets a fix and geoclue serves it** (patio, open sky, no
SIM, no assistance injected): route 1 below works as a prototype started by hand; not yet
packaged or started at boot. AGPS is a first-class goal of this port (time to first fix was the oldest
complaint about SailfishOS devices), so it is designed in from the start, not added later.

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

## AGPS plan after the 2026-11-06 research (nothing tested yet)

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
