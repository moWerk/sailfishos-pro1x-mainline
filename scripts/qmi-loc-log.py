#!/usr/bin/env python3
# Long GNSS run over QMI LOC: same session as qmi-loc-probe.py, but every NMEA sentence goes
# to <log> as it arrives (elapsed seconds first), and the first fix and the satellites in
# view are noted. Restarts the session if indications stop for 60 s.
# usage: qmi-loc-log.py <node> <port> <seconds> <log>
import ctypes, os, select, socket, struct, sys, time
node, port, secs, logp = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
libc = ctypes.CDLL(None, use_errno=True)
class SockaddrQrtr(ctypes.Structure):
    _fields_ = [("family", ctypes.c_ushort), ("node", ctypes.c_uint), ("port", ctypes.c_uint)]
fd = libc.socket(42, socket.SOCK_DGRAM, 0)
dest = SockaddrQrtr(42, node, port)
txn = 1
def req(mid, tlvs=b""):
    global txn
    pkt = struct.pack("<BHHH", 0, txn, mid, len(tlvs)) + tlvs
    txn += 1
    buf = ctypes.create_string_buffer(pkt, len(pkt))
    if libc.sendto(fd, buf, len(pkt), 0, ctypes.byref(dest), ctypes.sizeof(dest)) < 0:
        raise OSError(ctypes.get_errno(), "sendto")
tlv = lambda t, d: struct.pack("<BH", t, len(d)) + d
def start():
    req(0x21, tlv(0x01, struct.pack("<Q", 0x1 | 0x2 | 0x4 | 0x40 | 0x200)))
    req(0x22, tlv(0x01, b"\x01") + tlv(0x10, struct.pack("<I", 1)) + tlv(0x13, struct.pack("<I", 1000)))
log = open(logp, "a", buffering=1)
t0 = time.time(); last = t0; fix = None; maxsv = 0
log.write("# start %s\n" % time.strftime("%F %T"))
start()
while time.time() - t0 < secs:
    if not select.select([fd], [], [], 1.0)[0]:
        if time.time() - last > 60:
            log.write("# %.0f no indications for 60 s, restarting session\n" % (time.time() - t0))
            start(); last = time.time()
        continue
    pkt = os.read(fd, 65536); last = time.time()
    typ, tid, mid, ln = struct.unpack_from("<BHHH", pkt)
    body, tl, i = pkt[7:7 + ln], {}, 0
    while i + 3 <= len(body):
        t, l = struct.unpack_from("<BH", body, i); tl[t] = body[i + 3:i + 3 + l]; i += 3 + l
    if typ == 2 and 0x02 in tl:
        log.write("# %.0f response 0x%02x result/error %s\n" % (last - t0, mid, struct.unpack_from("<HH", tl[0x02])))
    if typ == 4 and mid == 0x26 and 0x01 in tl:
        s = tl[0x01].split(b"\0")[0].decode(errors="replace").strip()
        log.write("%.1f %s\n" % (last - t0, s))
        f = s.split(",")
        if f[0].endswith("GSV") and len(f) > 3 and f[3].isdigit():
            maxsv = max(maxsv, int(f[3]))
        if f[0].endswith("GGA") and len(f) > 7 and f[6] not in ("", "0") and fix is None:
            fix = last - t0
            log.write("# FIRST FIX after %.0f s, quality %s, %s satellites used\n" % (fix, f[6], f[7]))
req(0x23, tlv(0x01, b"\x01"))
log.write("# end: first fix %s, max satellites in view %d\n" % (fix, maxsv))
