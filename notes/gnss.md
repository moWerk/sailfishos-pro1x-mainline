# GNSS and AGPS

Status 2026-10-30: **the modem's GNSS engine answers and streams, nothing feeds SailfishOS
location yet.** AGPS is a first-class goal of this port (time to first fix was the oldest
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

## What SailfishOS offers

- geoclue 0.12 (Jolla's) with providers; Jolla's own GPS provider is
  `geoclue-provider-hybris-binder` (Android location HAL, not usable on mainline).
- In the jolla repository: `gpsd` 3.26.1 (+ clients, python3-gpsd), `geoclue-provider-mlsdb`
  (offline cell location database), `-gsmloc`, `-hostip`, `-manual`.
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
