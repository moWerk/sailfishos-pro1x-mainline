#!/usr/bin/env python3
# Force a GNSS cold start for TTFF comparisons: find the QMI LOC service (16) through the QRTR
# name service, then DELETE ASSISTANCE DATA (0x44) with "delete all". Works without qrtr-lookup
# or qmicli (the hybris port has neither). usage: qmi-loc-coldstart.py [--lookup-only]
import ctypes, os, select, socket, struct, sys, time
libc = ctypes.CDLL(None, use_errno=True)
class SockaddrQrtr(ctypes.Structure):
    _fields_ = [("family", ctypes.c_ushort), ("node", ctypes.c_uint), ("port", ctypes.c_uint)]
fd = libc.socket(42, socket.SOCK_DGRAM, 0)
if fd < 0:
    raise OSError(ctypes.get_errno(), "socket")
me = SockaddrQrtr(); ln = ctypes.c_uint(ctypes.sizeof(me))
libc.getsockname(fd, ctypes.byref(me), ctypes.byref(ln))

def send(node, port, pkt):
    d = SockaddrQrtr(42, node, port)
    buf = ctypes.create_string_buffer(pkt, len(pkt))
    if libc.sendto(fd, buf, len(pkt), 0, ctypes.byref(d), ctypes.sizeof(d)) < 0:
        raise OSError(ctypes.get_errno(), "sendto")

# QRTR_TYPE_NEW_LOOKUP (10) for service 16 to the local control port
send(me.node, 0xfffffffe, struct.pack("<5I", 10, 16, 0, 0, 0))
loc = None
end = time.time() + 3
while time.time() < end and select.select([fd], [], [], 0.5)[0]:
    cmd, svc, inst, node, port = struct.unpack_from("<5I", os.read(fd, 4096))
    if cmd == 4 and svc == 16:        # NEW_SERVER
        loc = (node, port); print("LOC service on node %d port %d (instance %d)" % (node, port, inst))
    if cmd == 4 and svc == 0:          # end of the lookup list
        break
if not loc or "--lookup-only" in sys.argv:
    sys.exit(0 if loc else 1)

tlv = lambda t, d: struct.pack("<BH", t, len(d)) + d
# DELETE ASSIST DATA (0x44, "delete all" TLV 0x01) is refused by newer modems (Pro1-X MPSS.HA.1.1:
# QMI error 0x73); DELETE GNSS SERVICE DATA (0xA6) takes the same "delete all" flag.
for txn, mid in ((1, 0x44), (2, 0xA6)):
    body = tlv(0x01, b"\x01")
    send(loc[0], loc[1], struct.pack("<BHHH", 0, txn, mid, len(body)) + body)
    ok, end = None, time.time() + 5
    while time.time() < end and select.select([fd], [], [], 1)[0]:
        pkt = os.read(fd, 4096)
        typ, tid, m, l = struct.unpack_from("<BHHH", pkt)
        if m != mid:
            continue
        if typ == 2:
            res, err = struct.unpack_from("<HH", pkt, 10)
            print("0x%02x response result %d error 0x%x" % (mid, res, err))
            if res != 0:
                ok = False
                break
        if typ == 4:
            st = struct.unpack_from("<I", pkt, 10)[0]
            print("0x%02x indication status %d" % (mid, st))
            ok = st == 0
            break
    if ok:
        break
