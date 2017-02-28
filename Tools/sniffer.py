# SensorPlatform Radio Sniffer Decoder
# Copyright (C) 2016-2017 Michael Sparmann
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import sys
import time
import serial
import struct
import binascii

if len(sys.argv) < 3:
    print("Usage: %s <port> <nodeid>" % sys.argv[0])
    sys.exit(127)
port = serial.Serial(sys.argv[1], 1200000, timeout=0.1)
nodeid = int(sys.argv[2])
port.send_break(0.01)
cmdseq = 0

def sendCmd(type, data):
    global cmdseq
    cmdseq += 1
    port.write(struct.pack("<HBB", 0x5352, type, cmdseq) + data)

def setChannel(channel, speed, pipes, netid1, netid2, nodeid, size):
    rfcfg = [0x0f, 0x07, 0x07, 0x27][speed]
    sendCmd(0x01,
        struct.pack("BBBBBBBBB", 0x0f, 0x00, pipes, 0x01, 0x00, channel, rfcfg, 0x00, 0x00) +
        struct.pack("BB", 0, 0) +
        struct.pack("BBBBB", 0x00, 0x00, 0x00, 0x00, 0x00) +
        struct.pack("BBBBB", nodeid, netid2, 0xec, 0x00, 0x00) +
        struct.pack("BBBBB", 0xff, netid1, 0xec, 0x00, 0x00) +
        struct.pack("BBBB", 0xfe, 0x7f, 0x00, 0xfd) +
        struct.pack("BBBBBB", size, 32, 32, 32, 32, 1)
    )
    
setChannel(82, 1, 0x01, 0, 0xdd, 0, 16)
port.read(4096)
#setChannel(80, 0, 0x3f, 123, 0xdd, 0, 16)

buf = b""
speed = 1
fs = 0
while True:
    buf += port.read(40)
    while len(buf) >= 40:
        if struct.unpack("<H", buf[:2])[0] != 0x5352:
            print("Discarding junk byte: %02X" % buf[0])
            buf = buf[1:]
            continue
        msg, seq, result, data = buf[2], buf[3], struct.unpack("<I", buf[4:8])[0], buf[8:40]
        if msg == 0x81:
            print("Got result for command %02X: %08X" % (seq, result))
        elif msg == 0xc0:
            pipe = seq & 7
            size = (seq >> 3) + 1
            end = result - fs
            start = end - (((size + 6) * 8 << speed) >> 1)
            if pipe == 1: fs, end = result, 0
            header = "%04d-%04d %d" % (start, end, pipe)
            if pipe == 0 and size == 16:
                channel, netid, offset, guard, cmds, _, _, ts = struct.unpack("<BBHBBHII", data[:16])
                speed, offset = offset >> 14, offset & 0x3fff
                seq, ts = ts >> 28, ts & 0xfffffff
                fs = result
                setChannel(channel, speed, 0x3f, netid, netid, nodeid, 0x20)
                print("%09d %d BEACON channel=%02d speed=%d netid=%02X cmds=%d offset=%04d guard=%03d seq=%X time=%010d" %
                      (result, pipe, channel, speed, netid, cmds, offset, guard, seq, ts))
            elif pipe == 0:
                content = "CMD(%02X %04X %02X %s)" % \
                          (data[3] & 0x1f, struct.unpack("<H", data[:2])[0], data[2], binascii.hexlify(data[4:size]).decode("ascii"))
                print("%s NODE%2X %s" % (header, nodeid, content))
            elif pipe == 1:
                ts = struct.unpack("<I", data[28:])[0]
                seq, ts = ts >> 28, ts & 0xfffffff
                ack = ""
                owner = ""
                for i in range(28):
                    ack += "%d" % (data[i] >> 7)
                    owner += ",%02X" % (data[i] & 0x7f)
                print("%s SOF %02d ack=%s owner=%s time=%010d" % (header, seq, ack, owner[1:], ts))
            elif pipe == 2:
                print("%s BRCAST %s" % (header, binascii.hexlify(data[:size])))
            elif pipe == 3:
                content = "UNKNOWN(%s)" % binascii.hexlify(data[:size]).decode("ascii")
                print("%s NOTREP %s" % (header, content))
            elif pipe == 4:
                type = "RE%02X" % data[0]
                content = ""
                if data[0] == 0x7f:
                    type = "NOTI"
                    content += "UNKNOWN(%s)" % binascii.hexlify(data[:size]).decode("ascii")
                else:
                    content = "urgency=%d pending=%02d " % (data[1] >> 5, data[1] & 0x1f)
                    if data[3] == 0xff:
                        content += "NODATA skip=%03d localtime=%010d bitrate=%010d dataseq=%010d sofrx=%05d sofbadtiming=%05d sofdiscont=%05d txattempt=%05d txack=%05d rxcmd=%05d" % \
                                   struct.unpack("<BIIIHHHHHH", data[2:3] + data[4:28])
                    elif (data[3] & 0xe0) == 0x80:
                        content += "CMDRESP(%02X %02X %s)" % \
                                   (data[3] & 0x1f, data[2], binascii.hexlify(data[4:size]).decode("ascii"))
                    elif (data[3] & 0x80) == 0:
                        content += "DATA(%04X %s)" % \
                                   (struct.unpack("<H", data[2:4])[0], binascii.hexlify(data[4:size]).decode("ascii"))
                    else: content += "UNKNOWN(%s)" % binascii.hexlify(data[:size]).decode("ascii")
                slotlen = (((32 + 6) * 8 + guard) << speed) >> 1
                slot = (end - ((offset << speed) >> 1) - slotlen / 16) / slotlen
                print("%0s %02d%s %s" % (header, slot, type, content))
            elif pipe == 5:
                pass#print("%s HEADER %s" % (header, binascii.hexlify(data[:size])))
            else:
                print("%s UNKNWN %s" % (header, binascii.hexlify(data[:size])))
        else:
            print("Got unknown message %02X (seq %02X): %08X %s" % (msg, seq, result, binascii.hexlify(data)))
        buf = buf[40:]
