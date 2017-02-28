# SensorPlatform JSON Measurement Series Decoder
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

import os
import sys
import json
import functools

total = 0
last = 0
def parse(fileobj, decoder=json.JSONDecoder(), buffersize=65536):
    global total, last
    buffer = ""
    for chunk in iter(functools.partial(fileobj.read, buffersize), ""):
        total += len(chunk)
        if total - last >= 1048576:
            last = total
            print("%d MiB processed" % (total / 1048576))
        buffer += chunk
        while buffer:
            buffer = buffer.lstrip() 
            try:
                result, index = decoder.raw_decode(buffer)
                yield result
                buffer = buffer[index:]
            except ValueError: break

series = ""
writers = {}
with open(sys.argv[1]) as f:
    for packet in parse(f):
        if series != packet["series"]:
            for w in writers.values(): w.close()
            writers = {}
            series = packet["series"]
            if not os.path.exists(series): os.makedirs(series)
        for r in packet["data"]:
            if r["type"] != "decoded": continue
            filename = "%s/%s-%d.csv" % (series, r["device"], r["sensor"])
            if not filename in writers:
                w = open(filename, "w")
                writers[filename] = w
                w.write("Time;" + ";".join(r["component"]) + "\n")
                w.write("ms;" + ";".join(r["unit"]) + "\n")
            writers[filename].write(str(r["time"]) + ";" + ";".join(map(str, r["value"])) + "\n")
for w in writers.values(): w.close()
