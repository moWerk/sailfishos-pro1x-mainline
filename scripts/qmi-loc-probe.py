#!/usr/bin/env python3
# Minimal QMI LOC client over QRTR: register events, start a session, print NMEA and
# indication counts for a while, stop. usage: qloc.py <node> <port> <seconds>
import socket, struct, sys, time

node, port, secs = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
import ctypes, select, os
libc = ctypes.CDLL(None, use_errno=True)
AF_QIPCRTR = 42

class SockaddrQrtr(ctypes.Structure):
    _fields_ = [("family", ctypes.c_ushort), ("node", ctypes.c_uint), ("port", ctypes.c_uint)]

fd = libc.socket(AF_QIPCRTR, socket.SOCK_DGRAM, 0)
if fd < 0:
    raise OSError(ctypes.get_errno(), "socket")
dest = SockaddrQrtr(AF_QIPCRTR, node, port)
txn = 1

class S:
    @staticmethod
    def sendto(pkt, _addr):
        buf = ctypes.create_string_buffer(pkt, len(pkt))
        r = libc.sendto(fd, buf, len(pkt), 0, ctypes.byref(dest), ctypes.sizeof(dest))
        if r < 0:
            raise OSError(ctypes.get_errno(), "sendto")
    @staticmethod
    def recv(n):
        if not select.select([fd], [], [], 1.0)[0]:
            raise socket.timeout()
        return os.read(fd, n)
s = S()

def tlv(t, data):
    return struct.pack("<BH", t, len(data)) + data

def req(msg_id, tlvs=b""):
    global txn
    pkt = struct.pack("<BHHH", 0, txn, msg_id, len(tlvs)) + tlvs
    txn += 1
    s.sendto(pkt, (node, port))

def parse(pkt):
    typ, tid, mid, ln = struct.unpack_from("<BHHH", pkt)
    body, tl = pkt[7:7 + ln], {}
    i = 0
    while i + 3 <= len(body):
        t, l = struct.unpack_from("<BH", body, i)
        tl[t] = body[i + 3:i + 3 + l]
        i += 3 + l
    return typ, mid, tl

# QMI_LOC_REG_EVENTS 0x21: position report 0x1, GNSS SV info 0x2, NMEA 0x4, engine state 0x80000? (fix session state 0x40)
req(0x21, tlv(0x01, struct.pack("<Q", 0x1 | 0x2 | 0x4 | 0x40 | 0x200)))
# QMI_LOC_START 0x22: session id 1; fix recurrence periodic (TLV 0x10 = 1); min interval 1000 ms (TLV 0x13)
req(0x22, tlv(0x01, b"\x01") + tlv(0x10, struct.pack("<I", 1)) + tlv(0x13, struct.pack("<I", 1000)))

counts, nmea, first = {}, [], {}
t0 = time.time()
while time.time() - t0 < secs:
    try:
        pkt = s.recv(65536)
    except socket.timeout:
        continue
    typ, mid, tl = parse(pkt)
    key = ("resp" if typ == 2 else "ind" if typ == 4 else str(typ)) + " 0x%02x" % mid
    counts[key] = counts.get(key, 0) + 1
    first.setdefault(key, round(time.time() - t0, 1))
    if typ == 2 and 0x02 in tl:
        res, err = struct.unpack_from("<HH", tl[0x02])
        print("response 0x%02x result=%d error=%d" % (mid, res, err))
    if typ == 4 and mid == 0x26 and 0x01 in tl:
        line = tl[0x01].split(b"\0")[0].decode(errors="replace").strip()
        nmea.append(line)
    if typ == 4 and mid == 0x25:
        print("sv-info ind at %.1fs, tlvs %s" % (time.time() - t0, {k: len(v) for k, v in tl.items()}))

req(0x23, tlv(0x01, b"\x01"))     # QMI_LOC_STOP
time.sleep(0.5)
print("counts:", counts, "first seen (s):", first)
kinds = {}
for l in nmea:
    k = l[:6]
    kinds[k] = kinds.get(k, 0) + 1
print("nmea sentences:", kinds)
for k in sorted(kinds):
    print("last", [l for l in nmea if l.startswith(k)][-1][:120])
