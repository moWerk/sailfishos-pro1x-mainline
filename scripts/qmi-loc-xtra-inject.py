#!/usr/bin/env python3
# Inject UTC time, then an XTRA file (0xA7, 1 KiB parts), then query validity (0x37).
# Needs the GNSS engine unlocked first: qmicli -d qrtr://0 --loc-set-engine-lock=none
import os, ctypes, select, struct, subprocess, sys, time
libc = ctypes.CDLL(None, use_errno=True)
class SA(ctypes.Structure):
    _fields_ = [("family", ctypes.c_ushort), ("node", ctypes.c_uint32), ("port", ctypes.c_uint32)]
for line in subprocess.run(["qrtr-lookup"], capture_output=True, text=True).stdout.splitlines():
    f = line.split()
    if len(f) >= 5 and f[0] == "16":
        node, port = int(f[3]), int(f[4])
fd = libc.socket(42, 2, 0); dst = SA(42, node, port); txn = 0
def tlv(t, v): return struct.pack("<BH", t, len(v)) + v
def send(mid, body=b""):
    global txn; txn += 1
    b = struct.pack("<BHHH", 0, txn, mid, len(body)) + body
    libc.sendto(fd, b, len(b), 0, ctypes.byref(dst), ctypes.sizeof(dst))
def tlvs(b):
    d, i = {}, 0
    while i + 3 <= len(b):
        t, n = struct.unpack_from("<BH", b, i); d[t] = b[i+3:i+3+n]; i += 3 + n
    return d
def wait(mid, sec=5):
    end = time.time() + sec
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], max(0, end - time.time()))
        if not r: break
        buf = ctypes.create_string_buffer(65536); src = SA(); sl = ctypes.c_uint32(ctypes.sizeof(src))
        libc.recvfrom(fd, buf, 65536, 0, ctypes.byref(src), ctypes.byref(sl))
        typ, t, m, ln = struct.unpack_from("<BHHH", buf.raw, 0)
        d = tlvs(buf.raw[7:7+ln])
        if typ == 2 and d.get(0x02, b"\0\0")[:2] != b"\0\0":
            print("resp error", hex(m), d[0x02].hex()); return None
        if typ == 4 and m == mid:
            return d
    return None
data = open(sys.argv[1], "rb").read()
import os
if os.environ.get("SESSION"):
    send(0x21, tlv(0x01, struct.pack("<Q", (1<<4)|(1<<5)|(1<<7)))); wait(0x21, 1)
    send(0x22, tlv(0x01, b"") + tlv(0x10, struct.pack("<I", 1)) + tlv(0x12, struct.pack("<I", 2)) + tlv(0x13, struct.pack("<I", 1000)))
    end = time.time() + 8
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 1)
        if r:
            buf = ctypes.create_string_buffer(65536); src = SA(); sl = ctypes.c_uint32(ctypes.sizeof(src))
            libc.recvfrom(fd, buf, 65536, 0, ctypes.byref(src), ctypes.byref(sl))
            typ, t, m, ln = struct.unpack_from("<BHHH", buf.raw, 0)
            print("ind", typ, hex(m), {hex(k): v.hex()[:60] for k, v in tlvs(buf.raw[7:7+ln]).items()})
send(0x38, tlv(0x01, struct.pack("<Q", int(time.time() * 1000))) + tlv(0x02, struct.pack("<I", 1000)))
r = wait(0x38); print("time inject status", r and r[0x01].hex())
PART = 1024
parts = (len(data) + PART - 1) // PART
t0 = time.time()
for n in range(parts):
    chunk = data[n * PART:(n + 1) * PART]
    send(0xA7, tlv(0x01, struct.pack("<I", len(data))) + tlv(0x02, struct.pack("<H", parts)) +
         tlv(0x03, struct.pack("<H", n + 1)) + tlv(0x04, struct.pack("<H", len(chunk)) + chunk))
    r = wait(0xA7)
    st = r and r[0x01].hex()
    if os.environ.get("VERBOSE"): print("part", n + 1, "->", r and {hex(k): v.hex() for k, v in r.items()})
    if st != "00000000":
        print("part", n + 1, "of", parts, "status", st, r and {hex(k): v.hex() for k, v in r.items()}); break
else:
    print("injected", parts, "parts in %.1f s" % (time.time() - t0))
send(0x37); r = wait(0x37)
print("validity", r and {hex(k): v.hex() for k, v in r.items()})
if r and 0x10 in r:
    ts, hrs = struct.unpack("<QH", r[0x10])
    print("valid from", time.strftime("%Y-%m-%d %H:%M UTC", time.gmtime(ts)), "for", hrs, "h")
if os.environ.get("SESSION"):
    send(0x23, tlv(0x01, b"")); wait(0x23, 2)
