#!/usr/bin/env python3
# Minimal QMI LOC client over QRTR: register events, start a periodic session,
# log NMEA and position indications, optionally feed NMEA to gpsd through a pty.
# Usage: qmi-loc-gpsd-bridge.py OUTDIR MINUTES [PTYLINK]  (gpsd then reads the pty that PTYLINK points to)
import ctypes, os, select, signal, struct, subprocess, sys, time

AF_QIPCRTR, SOCK_DGRAM = 42, 2
LOC_SERVICE = 16
REG_EVENTS, START, STOP = 0x21, 0x22, 0x23
IND_POS, IND_SV, IND_NMEA, IND_ENGINE, IND_FIXSESS = 0x24, 0x25, 0x26, 0x2B, 0x2C

libc = ctypes.CDLL(None, use_errno=True)

class SockaddrQrtr(ctypes.Structure):
    _fields_ = [("family", ctypes.c_ushort), ("node", ctypes.c_uint32), ("port", ctypes.c_uint32)]

def lookup():
    out = subprocess.run(["qrtr-lookup"], capture_output=True, text=True).stdout
    for line in out.splitlines():
        f = line.split()
        if len(f) >= 5 and f[0] == str(LOC_SERVICE):
            return int(f[3]), int(f[4])
    raise SystemExit("no LOC service")

out = sys.argv[1]
minutes = float(sys.argv[2])
pty_master = None
if len(sys.argv) > 3:
    import fcntl, tty
    pty_master, slave = os.openpty()
    tty.setraw(slave)
    fcntl.fcntl(pty_master, fcntl.F_SETFL, fcntl.fcntl(pty_master, fcntl.F_GETFL) | os.O_NONBLOCK)
    link = sys.argv[3]
    if os.path.lexists(link):
        os.unlink(link)
    os.symlink(os.ttyname(slave), link)
# NMEA types reset with the modem; they are global, so set them before START
subprocess.run(["qmicli", "-d", "qrtr://0", "--loc-set-nmea-types=gga|rmc|gsv|gsa|vtg"],
               capture_output=True)
os.makedirs(out, exist_ok=True)
log = open(os.path.join(out, "loc.log"), "a", buffering=1)
nmea = open(os.path.join(out, "nmea.txt"), "a", buffering=1)

def L(*a):
    log.write(time.strftime("%H:%M:%S ") + " ".join(str(x) for x in a) + "\n")

node, port = lookup()
fd = libc.socket(AF_QIPCRTR, SOCK_DGRAM, 0)
if fd < 0:
    raise SystemExit("socket: errno %d" % ctypes.get_errno())
dst = SockaddrQrtr(AF_QIPCRTR, node, port)
txn = 0

def tlv(t, v):
    return struct.pack("<BH", t, len(v)) + v

def send(msg_id, tlvs):
    global txn
    txn += 1
    body = b"".join(tlvs)
    buf = struct.pack("<BHHH", 0, txn, msg_id, len(body)) + body
    r = libc.sendto(fd, buf, len(buf), 0, ctypes.byref(dst), ctypes.sizeof(dst))
    L("send", hex(msg_id), "txn", txn, "->", r if r >= 0 else "errno %d" % ctypes.get_errno())

def parse_tlvs(b):
    d, i = {}, 0
    while i + 3 <= len(b):
        t, n = struct.unpack_from("<BH", b, i)
        d[t] = b[i + 3:i + 3 + n]
        i += 3 + n
    return d

def recv(timeout):
    r, _, _ = select.select([fd], [], [], timeout)
    if not r:
        return None
    buf = ctypes.create_string_buffer(65536)
    src = SockaddrQrtr()
    sl = ctypes.c_uint32(ctypes.sizeof(src))
    n = libc.recvfrom(fd, buf, 65536, 0, ctypes.byref(src), ctypes.byref(sl))
    if n < 7 or src.port != port:
        return ()
    typ, t, mid, ln = struct.unpack_from("<BHHH", buf.raw, 0)
    return typ, t, mid, parse_tlvs(buf.raw[7:7 + ln])

stop = False
def on_term(*_):
    global stop
    stop = True
signal.signal(signal.SIGTERM, on_term)
signal.signal(signal.SIGINT, on_term)

# events: position report, SV info, NMEA, engine state, fix session state
mask = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 7) | (1 << 8)
send(REG_EVENTS, [tlv(0x01, struct.pack("<Q", mask))])
# session 1, periodic fixes, intermediate reports on, 1 s interval
send(START, [tlv(0x01, b"\x01"), tlv(0x10, struct.pack("<I", 1)),
             tlv(0x12, struct.pack("<I", 1)), tlv(0x13, struct.pack("<I", 1000))])

t0 = time.time()
first_fix = None
last_sv = 0
last_pos = [None, 0]
while not stop and time.time() - t0 < minutes * 60:
    m = recv(5)
    if not m:
        continue
    typ, t, mid, d = m
    if typ == 2:
        res = d.get(0x02, b"")
        L("resp", hex(mid), "result", struct.unpack("<HH", res) if len(res) == 4 else res.hex())
    elif mid == IND_NMEA and 0x01 in d:
        s = d[0x01].rstrip(b"\0").decode(errors="replace").strip()
        nmea.write("%.1f %s\n" % (time.time() - t0, s))
        if pty_master is not None:
            for line in s.splitlines():
                try:
                    os.write(pty_master, line.strip().encode() + b"\r\n")
                except BlockingIOError:
                    pass  # nobody reads the pty; drop rather than block the session
        if first_fix is None and (s.startswith("$GPGGA") or s.startswith("$GNGGA")):
            f = s.split(",")
            if len(f) > 6 and f[6] not in ("", "0"):
                first_fix = time.time() - t0
                L("FIRST FIX after %.0f s:" % first_fix, s)
    elif mid == IND_POS:
        # 0x01 session status (u32): 0 success, 1 in progress, ...
        st = struct.unpack("<I", d[0x01])[0] if len(d.get(0x01, b"")) == 4 else None
        lat = struct.unpack("<d", d[0x10])[0] if len(d.get(0x10, b"")) == 8 else None
        lon = struct.unpack("<d", d[0x11])[0] if len(d.get(0x11, b"")) == 8 else None
        unc = struct.unpack("<f", d[0x13])[0] if len(d.get(0x13, b"")) == 4 else None
        if st != last_pos[0] or time.time() - last_pos[1] > 30:
            last_pos[:] = [st, time.time()]
            L("pos status", st, "hunc_m", None if unc is None else round(unc), "tlvs", len(d))
        if lat is not None:
            with open(os.path.join(out, "fix.txt"), "a") as f:
                f.write("%.1f %s %.6f %.6f %s\n" % (time.time() - t0, st, lat, lon, unc))
    elif mid == IND_SV:
        if time.time() - last_sv > 30:
            last_sv = time.time()
            sv = d.get(0x10, b"")
            L("sv info", "list bytes", len(sv), "n", sv[0] if sv else 0)
    else:
        L("ind", hex(mid), {hex(k): v.hex()[:40] for k, v in d.items()})

send(STOP, [tlv(0x01, b"\x01")])
recv(2)
L("end, first fix", first_fix)
