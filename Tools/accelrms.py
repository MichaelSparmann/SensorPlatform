# SensorPlatform Acclerometer RMS Signal Analyzer
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

scale = int(sys.argv[2])

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
print("Averages:", avg)

with open(sys.argv[1] + "-16.csv") as f:
    out = open(sys.argv[1] + "-accelrms.csv", "w")
    out.write("Time;X;Y;Z\n")
    out.write("ms;g;g;g\n")
    count = 0
    sum = [0, 0, 0]
    for line in f.readlines()[2:]:
        if count == scale:
            out.write("%f;%f;%f;%f\n" % (meastime, math.sqrt(sum[0] / count), math.sqrt(sum[1] / count), math.sqrt(sum[2] / count)))
            count = 0
            sum = [0, 0, 0]
        line = line.split(";")
        meastime = float(line[0])
        sum[0] += (float(line[1]) - avg[0]) ** 2
        sum[1] += (float(line[2]) - avg[1]) ** 2
        if float(line[3]): sum[2] += (float(line[3]) - avg[2]) ** 2
        count += 1
