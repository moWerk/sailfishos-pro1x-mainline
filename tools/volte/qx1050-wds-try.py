#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""VoLTE step 2 diagnostic: one QMI WDS client in one process (on QRTR a client lives as long as
its socket, so qmicli cannot do bind + start across calls). Binds to a mux, sets the IP family,
sends START NETWORK with the given TLVs and prints the result with the call end reasons that
oFono does not read. If the network grants the PDN, it is stopped again at once.

usage: qx1050-wds-try.py <mux-id> <family 4|6> [profile=N] [apn=X] [auth=none|pap|chap]
"""
import ctypes, os, select, socket, struct, sys, time

libc = ctypes.CDLL(None, use_errno=True)
class SA(ctypes.Structure):
    _fields_ = [("family", ctypes.c_ushort), ("node", ctypes.c_uint), ("port", ctypes.c_uint)]
fd = libc.socket(42, socket.SOCK_DGRAM, 0)
me = SA(); ln = ctypes.c_uint(ctypes.sizeof(me)); libc.getsockname(fd, ctypes.byref(me), ctypes.byref(ln))
def send(node, port, pkt):
    d = SA(42, node, port); b = ctypes.create_string_buffer(pkt, len(pkt))
    if libc.sendto(fd, b, len(pkt), 0, ctypes.byref(d), ctypes.sizeof(d)) < 0:
        raise OSError(ctypes.get_errno(), "sendto")
tlv = lambda t, d: struct.pack("<BH", t, len(d)) + d
def tlvs(body):
    out, i = {}, 0
    while i + 3 <= len(body):
        t, l = struct.unpack_from("<BH", body, i); out[t] = body[i + 3:i + 3 + l]; i += 3 + l
    return out

send(me.node, 0xfffffffe, struct.pack("<5I", 10, 1, 0, 0, 0))   # lookup WDS
wds, end = None, time.time() + 3
while time.time() < end and select.select([fd], [], [], 0.5)[0]:
    cmd, svc, inst, node, port = struct.unpack_from("<5I", os.read(fd, 4096))
    if cmd == 4 and svc == 1 and wds is None: wds = (node, port)
    if cmd == 4 and svc == 0: break
if not wds: sys.exit("no WDS service")

TXN = [0]
def req(mid, body, timeout=30):
    TXN[0] += 1
    send(wds[0], wds[1], struct.pack("<BHHH", 0, TXN[0], mid, len(body)) + body)
    end = time.time() + timeout
    while time.time() < end and select.select([fd], [], [], 1)[0]:
        pkt = os.read(fd, 65536)
        typ, tid, m, l = struct.unpack_from("<BHHH", pkt)
        if typ == 2 and tid == TXN[0] and m == mid:
            t = tlvs(pkt[7:7 + l]); res, err = struct.unpack("<HH", t[2])
            return res, err, t
    return None, None, {}

mux, fam = int(sys.argv[1]), int(sys.argv[2])
opts = dict(a.split("=", 1) for a in sys.argv[3:])
r = req(0xA2, tlv(0x10, struct.pack("<II", 4, 1)) + tlv(0x11, struct.pack("<B", mux)))
print("bind mux %d: result %s error %s" % (mux, r[0], r[1]))
r = req(0x4D, tlv(0x01, struct.pack("<B", fam)))
print("set ip family %d: result %s error %s" % (fam, r[0], r[1]))
body = tlv(0x19, struct.pack("<B", fam))
if "profile" in opts: body += tlv(0x31, struct.pack("<B", int(opts["profile"])))
if "apn" in opts: body += tlv(0x14, opts["apn"].encode())
if "auth" in opts: body += tlv(0x16, struct.pack("<B", {"none": 0, "pap": 1, "chap": 2}[opts["auth"]]))
res, err, t = req(0x20, body, 60)
desc = " ".join(sys.argv[3:]) or "(no profile/apn)"
if res is None:
    print("START %s: no reply in 60 s" % desc); sys.exit(1)
line = "START family %d %s: result %d error %d" % (fam, desc, res, err)
if 0x10 in t: line += " call-end-reason %d" % struct.unpack("<H", t[0x10])[0]
if 0x11 in t:
    vt, vr = struct.unpack("<HH", t[0x11]); line += " verbose type %d reason %d" % (vt, vr)
print(line)
if res == 0 and 0x01 in t:
    h = t[0x01]
    r = req(0x2D, b"")   # GET RUNTIME SETTINGS with no TLVs: only the result, nothing identifying printed
    print("  granted (packet handle %s); stopping" % h.hex())
    r = req(0x21, tlv(0x01, h))
    print("  stop: result %s error %s" % (r[0], r[1]))
