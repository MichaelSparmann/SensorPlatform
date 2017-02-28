# SensorPlatform Acclerometer Signal-to-Noise Ratio Analyzer
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
import math

idlewin = int(sys.argv[2])
printwin = int(sys.argv[3])
winlen = int(sys.argv[4])

sum = [0, 0, 0]
count = 0
with open(sys.argv[1] + "-16.csv") as f:
    for line in f.readlines()[2:]:
        line = line.split(";")
        sum[0] += float(line[1])
        sum[1] += float(line[2])
        sum[2] += float(line[3])
        count += 1
avg = [x / count for x in sum]

idlesum = [0, 0, 0]
with open(sys.argv[1] + "-16.csv") as f:
    for line in f.readlines()[2 + idlewin:][:winlen]:
        line = line.split(";")
        idlesum[0] += (float(line[1]) - avg[0]) ** 2
        idlesum[1] += (float(line[2]) - avg[1]) ** 2
        if float(line[3]): idlesum[2] += (float(line[3]) - avg[2]) ** 2

printsum = [0, 0, 0]
with open(sys.argv[1] + "-16.csv") as f:
    for line in f.readlines()[2 + printwin:][:winlen]:
        line = line.split(";")
        printsum[0] += (float(line[1]) - avg[0]) ** 2
        printsum[1] += (float(line[2]) - avg[1]) ** 2
        if float(line[3]): printsum[2] += (float(line[3]) - avg[2]) ** 2

noiselevel = [x / winlen for x in idlesum]
sumlevel = [x / winlen for x in printsum]
signallevel = [max(sumlevel[i] - noiselevel[i], 0.000000000001) for i in range(3)]
fmt = "$%+05.1f$"
noisedb = [fmt % (10 * math.log10(x)) for x in noiselevel]
signaldb = [fmt % (10 * math.log10(x)) for x in signallevel]
snrdb = [fmt % (10 * math.log10(signallevel[i] / noiselevel[i])) for i in range(3)]
print("    " + sys.argv[1][-1:])
print("&   " + (" & ".join(noisedb)))
print("&   " + (" & ".join(signaldb)))
print("&   " + (" & ".join(snrdb)))
print("\\\\ \\hline")
