#!/usr/bin/env python3
# Read-only XTRA queries: predicted orbits source (0x36), validity (0x37), XTRA info (0xB0)
import ctypes, select, struct, subprocess, time
libc = ctypes.CDLL(None, use_errno=True)
class SA(ctypes.Structure):
    _fields_ = [("family", ctypes.c_ushort), ("node", ctypes.c_uint32), ("port", ctypes.c_uint32)]
node = port = None
for line in subprocess.run(["qrtr-lookup"], capture_output=True, text=True).stdout.splitlines():
    f = line.split()
    if len(f) >= 5 and f[0] == "16":
        node, port = int(f[3]), int(f[4])
fd = libc.socket(42, 2, 0)
dst = SA(42, node, port)
txn = 0
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
def drain(sec):
    end = time.time() + sec
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], max(0, end - time.time()))
        if not r: break
        buf = ctypes.create_string_buffer(65536); src = SA(); sl = ctypes.c_uint32(ctypes.sizeof(src))
        n = libc.recvfrom(fd, buf, 65536, 0, ctypes.byref(src), ctypes.byref(sl))
        typ, t, mid, ln = struct.unpack_from("<BHHH", buf.raw, 0)
        d = tlvs(buf.raw[7:7+ln])
        print("type", typ, "msg", hex(mid), {hex(k): v.hex() for k, v in d.items()})
        if mid == 0x36 and typ == 4 and 0x11 in d:
            s = d[0x11]; cnt = s[0]; i = 1
            for _ in range(cnt):
                l = s[i]; print("  server:", s[i+1:i+1+l].decode(errors="replace")); i += 1 + l
send(0x36); drain(3)
send(0x37); drain(3)
send(0xB0, tlv(0x01, struct.pack("<I", 1))); drain(3)
