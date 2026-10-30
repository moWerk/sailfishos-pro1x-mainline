#!/usr/bin/env python3
# Set the modem's voice domain preference (QMI NAS SET SYSTEM SELECTION PREFERENCE 0x33,
# TLV 0x23), which qmicli cannot set. usage: qx1050-voice-domain.py <cs-only|ps-only|cs-preferred|ps-preferred> [permanent]
# Without "permanent" it holds until the next modem power cycle (TLV 0x17 = 0).
import ctypes, os, select, socket, struct, sys, time
VAL = {"cs-only": 0, "ps-only": 1, "cs-preferred": 2, "ps-preferred": 3}[sys.argv[1]]
DUR = 1 if len(sys.argv) > 2 and sys.argv[2] == "permanent" else 0
libc = ctypes.CDLL(None, use_errno=True)
class SA(ctypes.Structure):
    _fields_ = [("family", ctypes.c_ushort), ("node", ctypes.c_uint), ("port", ctypes.c_uint)]
fd = libc.socket(42, socket.SOCK_DGRAM, 0)
me = SA(); ln = ctypes.c_uint(ctypes.sizeof(me)); libc.getsockname(fd, ctypes.byref(me), ctypes.byref(ln))
def send(node, port, pkt):
    d = SA(42, node, port); b = ctypes.create_string_buffer(pkt, len(pkt))
    if libc.sendto(fd, b, len(pkt), 0, ctypes.byref(d), ctypes.sizeof(d)) < 0:
        raise OSError(ctypes.get_errno(), "sendto")
send(me.node, 0xfffffffe, struct.pack("<5I", 10, 3, 0, 0, 0))   # lookup NAS (service 3)
nas = None; end = time.time() + 3
while time.time() < end and select.select([fd], [], [], 0.5)[0]:
    cmd, svc, inst, node, port = struct.unpack_from("<5I", os.read(fd, 4096))
    if cmd == 4 and svc == 3 and nas is None: nas = (node, port)
    if cmd == 4 and svc == 0: break
if not nas: sys.exit("no NAS service")
tlv = lambda t, d: struct.pack("<BH", t, len(d)) + d
body = tlv(0x17, struct.pack("<B", DUR)) + tlv(0x23, struct.pack("<I", VAL))
send(nas[0], nas[1], struct.pack("<BHHH", 0, 1, 0x33, len(body)) + body)
end = time.time() + 5
while time.time() < end and select.select([fd], [], [], 1)[0]:
    pkt = os.read(fd, 4096); typ, tid, mid, l = struct.unpack_from("<BHHH", pkt)
    if mid == 0x33 and typ == 2:
        res, err = struct.unpack_from("<HH", pkt, 10)
        print("NAS 0x33 voice domain %s (%s): result %d error 0x%x" % (sys.argv[1], "permanent" if DUR else "until power cycle", res, err))
        break
