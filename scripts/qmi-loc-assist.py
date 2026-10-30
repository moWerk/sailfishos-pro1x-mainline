#!/usr/bin/env python3
# QMI LOC session with assistance, for measuring time to first fix on mainline.
#   qmi-loc-assist.py <node> <port> <seconds> <log> [--time] [--xtra FILE] [--pos LAT LON UNC_M]
#     --time  inject UTC time from the AP clock (NTP-synced) when the modem asks for it
#     --now   inject the given assistance right away as well, before START
#     --xtra  inject an XTRA file (predicted orbits; download it from the servers the modem names
#             with --sources first)
#     --xtra-check N  set the XTRA version check first (0 off, 1 auto, 3 XTRA3)
#     --pos   inject a coarse position (degrees, uncertainty in metres)
#   qmi-loc-assist.py <node> <port> --sources   print the XTRA servers and allowed sizes
#   qmi-loc-assist.py <node> <port> --validity  how long the injected XTRA data is valid
# Sets the NMEA types itself (they reset on a modem restart). Logs NMEA, the satellites per
# system from the SV info indication (with SNR), and the first fix.
import ctypes, os, select, socket, struct, sys, time

node, port = int(sys.argv[1]), int(sys.argv[2])
libc = ctypes.CDLL(None, use_errno=True)
class SockaddrQrtr(ctypes.Structure):
    _fields_ = [("family", ctypes.c_ushort), ("node", ctypes.c_uint), ("port", ctypes.c_uint)]
fd = libc.socket(42, socket.SOCK_DGRAM, 0)
dest = SockaddrQrtr(42, node, port)
txn = 1
tlv = lambda t, d: struct.pack("<BH", t, len(d)) + d

def req(mid, tlvs=b""):
    global txn
    pkt = struct.pack("<BHHH", 0, txn, mid, len(tlvs)) + tlvs
    txn += 1
    buf = ctypes.create_string_buffer(pkt, len(pkt))
    if libc.sendto(fd, buf, len(pkt), 0, ctypes.byref(dest), ctypes.sizeof(dest)) < 0:
        raise OSError(ctypes.get_errno(), "sendto")

def recv(timeout=1.0):
    if not select.select([fd], [], [], timeout)[0]:
        return None
    pkt = os.read(fd, 65536)
    typ, tid, mid, ln = struct.unpack_from("<BHHH", pkt)
    body, tl, i = pkt[7:7 + ln], {}, 0
    while i + 3 <= len(body):
        t, l = struct.unpack_from("<BH", body, i); tl[t] = body[i + 3:i + 3 + l]; i += 3 + l
    return typ, mid, tl

def call(mid, tlvs=b"", wait_ind=True, timeout=5.0):
    """Send a request; return (response result/error, indication TLVs or None)."""
    req(mid, tlvs)
    res, ind, end = None, None, time.time() + timeout
    while time.time() < end and (res is None or (wait_ind and ind is None)):
        r = recv(0.5)
        if r and r[1] == mid:
            if r[0] == 2 and 0x02 in r[2]:
                res = struct.unpack_from("<HH", r[2][0x02])
                if res[0] != 0:
                    break
            elif r[0] == 4:
                ind = r[2]
    return res, ind

def ind_status(ind):
    return struct.unpack_from("<I", ind[0x01])[0] if ind and 0x01 in ind else None

if "--validity" in sys.argv:
    res, ind = call(0x37)
    print("response", res, "status", ind_status(ind), {k: v.hex() for k, v in (ind or {}).items()})
    if ind and 0x10 in ind:
        start, hours = struct.unpack_from("<QH", ind[0x10])
        print("xtra valid from GPS time %d s for %d h" % (start, hours))
    sys.exit(0)

if "--sources" in sys.argv:
    res, ind = call(0x36)
    print("response", res)
    if ind:
        if 0x10 in ind:
            print("max file size %d, max part size %d" % struct.unpack_from("<II", ind[0x10]))
        if 0x11 in ind:
            b, n, i = ind[0x11], ind[0x11][0], 1
            for _ in range(n):
                l = b[i]; print("server", b[i + 1:i + 1 + l].decode()); i += 1 + l
    sys.exit(0)

secs, logp = int(sys.argv[3]), sys.argv[4]
a = sys.argv[5:]
log = open(logp, "a", buffering=1)
t0 = time.time()
note = lambda s: log.write("# %.1f %s\n" % (time.time() - t0, s))
note("start %s, args %s" % (time.strftime("%F %T"), " ".join(a)))

# NMEA types: GGA RMC GSV GSA VTG (libqmi QmiLocNmeaType bits 0-4)
note("set nmea types %s" % (call(0x3E, tlv(0x01, struct.pack("<I", 0x1f)))[0],))
def inject_time():
    ms = int(time.time() * 1000)
    res, ind = call(0x38, tlv(0x01, struct.pack("<Q", ms)) + tlv(0x02, struct.pack("<I", 500)))
    note("inject utc time: response %s, status %s" % (res, ind_status(ind)))

def inject_pos():
    i = a.index("--pos"); lat, lon, unc = float(a[i + 1]), float(a[i + 2]), float(a[i + 3])
    res, ind = call(0x39, tlv(0x10, struct.pack("<d", lat)) + tlv(0x11, struct.pack("<d", lon))
                    + tlv(0x12, struct.pack("<f", unc)) + tlv(0x13, struct.pack("<B", 68))
                    + tlv(0x1B, struct.pack("<Q", int(time.time() * 1000))))
    note("inject position (uncertainty %.0f m): response %s, status %s" % (unc, res, ind_status(ind)))

def inject_xtra():
    if "--xtra-check" in a:
        # SET XTRA VERSION CHECK (0x85): 0 off, 1 auto, 2 XTRA2, 3 XTRA3, 4 XTRA3.1
        mode = int(a[a.index("--xtra-check") + 1])
        res, ind = call(0x85, tlv(0x01, struct.pack("<I", mode)))
        note("xtra version check %d: response %s, status %s" % (mode, res, ind_status(ind)))
    data = open(a[a.index("--xtra") + 1], "rb").read()
    part = 1024
    parts = (len(data) + part - 1) // part
    ok, ind = 0, None
    for n in range(parts):
        chunk = data[n * part:(n + 1) * part]
        # INJECT XTRA DATA (0xA7); this modem answers the older INJECT PREDICTED ORBITS DATA
        # (0x35) with QMI error 94, not supported
        res, ind = call(0xA7, tlv(0x01, struct.pack("<I", len(data))) + tlv(0x02, struct.pack("<H", parts))
                        + tlv(0x03, struct.pack("<H", n + 1)) + tlv(0x04, struct.pack("<H", len(chunk)) + chunk))
        if res is None or res[0] != 0 or ind_status(ind) not in (0, None):
            note("xtra part %d/%d failed: response %s, status %s" % (n + 1, parts, res, ind_status(ind)))
            break
        ok += 1
    note("xtra injected %d/%d parts (%d bytes), last status %s" % (ok, parts, len(data), ind_status(ind)))

# Events (libqmi QmiLocEventRegistrationFlag): position 0, SV info 1, NMEA 2, inject time
# request 4, predicted orbits request 5, position request 6, engine state 7, fix session 8.
# The modem asks for time, orbits and position when it wants them; Android's HAL answers those
# requests, and an unrequested injection right after start was refused (status 1).
req(0x21, tlv(0x01, struct.pack("<Q", 0x1 | 0x2 | 0x4 | 0x10 | 0x20 | 0x40 | 0x80 | 0x100)))
if "--now" in a:
    if "--time" in a: inject_time()
    if "--pos" in a: inject_pos()
    if "--xtra" in a: inject_xtra()
req(0x22, tlv(0x01, b"\x01") + tlv(0x10, struct.pack("<I", 1)) + tlv(0x11, struct.pack("<I", 3))
    + tlv(0x12, struct.pack("<I", 1)) + tlv(0x13, struct.pack("<I", 1000)))
SYS = {1: "GPS", 2: "GAL", 3: "SBAS", 4: "BDS", 5: "GLO", 6: "BDS", 7: "QZSS"}
fix, best, last_sv, tstart = None, {}, None, time.time()
while time.time() - tstart < secs:
    r = recv(1.0)
    if not r:
        continue
    typ, mid, tl = r
    if typ == 4 and mid in (0x28, 0x29, 0x2A, 0x2B, 0x2C):
        note("indication 0x%02x %s" % (mid, {k: v.hex()[:24] for k, v in tl.items()}))
        if mid == 0x28 and "--time" in a: inject_time()
        if mid == 0x29 and "--xtra" in a: inject_xtra()
        if mid == 0x2A and "--pos" in a: inject_pos()
        continue
    if typ == 4 and mid == 0x26 and 0x01 in tl:
        s = tl[0x01].split(b"\0")[0].decode(errors="replace").strip()
        log.write("%.1f %s\n" % (time.time() - t0, s))
        f = s.split(",")
        if f[0].endswith("GGA") and len(f) > 7 and f[6] not in ("", "0") and fix is None:
            fix = time.time() - tstart
            note("FIRST FIX after %.0f s from START, quality %s, %s satellites used" % (fix, f[6], f[7]))
    if typ == 4 and mid == 0x25 and 0x10 in tl:
        b, n = tl[0x10], tl[0x10][0]
        svs = []
        for k in range(n):
            e = b[1 + 28 * k:1 + 28 * (k + 1)]
            if len(e) < 28:
                break
            valid, system, svid, health, status, nav, el, az, snr = struct.unpack("<IIHBIBfff", e)
            svs.append("%s%d:%.0f" % (SYS.get(system, system), svid, snr))
            key = SYS.get(system, system)
            best[key] = max(best.get(key, 0), snr)
        line = " ".join(svs)
        if line != last_sv:
            log.write("%.1f SV %d %s\n" % (time.time() - t0, len(svs), line))
            last_sv = line
req(0x23, tlv(0x01, b"\x01"))
note("end: first fix %s, best SNR per system %s" % (fix, best))
