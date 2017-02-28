# SensorPlatform Measurement Timing Analyzer
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

iir_coeff = 0.02

with open(sys.argv[1] + "-0.csv") as f:
    out = open(sys.argv[1] + "-ta.csv", "w")
    out.write("Time;LocalTime;GlobalTime;MeasDelta;OscDelta;MeasDrift;OscDrift;FilteredMeasDrift;FilteredOscDrift\n")
    out.write("ms;ms;ms;µs;µs;ppm;ppm;ppm;ppm\n")
    first = True
    for line in f.readlines()[2:]:
        line = line.split(";")
        meastime, localtime, globaltime = float(line[0]), int(line[1]), int(line[2])
        if first:
            first = False
            measdelta, oscdelta, measdrift, oscdrift, filteredmeasdrift, filteredoscdrift = 0, 0, 0, 0, 0, 0
        else:
            timestep = int((meastime - lastmeastime) * 1000)
            if localtime:
                measdrift = (localtime - lastlocaltime - timestep) & 0xffff
                if measdrift > 32767: measdrift -= 65536
                measdelta += measdrift
                measdrift = 1000000. * measdrift / timestep
            else: localtime = int(lastlocaltime + timestep + measdrift * timestep / 1000000.)
            if globaltime:
                oscdrift = (globaltime - lastglobaltime - localtime + lastlocaltime) & 0xffff
                if oscdrift > 32767: oscdrift -= 65536
                oscdelta += oscdrift
                oscdrift = 1000000. * oscdrift / (localtime - lastlocaltime) if localtime != lastlocaltime else 0
            else: globaltime = int(lastglobaltime + timestep + oscdrift * (localtime - lastlocaltime) / 1000000.)
            filteredmeasdrift = filteredmeasdrift * (1 - iir_coeff) + measdrift * iir_coeff
            filteredoscdrift = filteredoscdrift * (1 - iir_coeff) + oscdrift * iir_coeff
        lastmeastime, lastlocaltime, lastglobaltime = meastime, localtime, globaltime
        lastmeasdelta, lastoscdelta = measdelta, oscdelta
        localtime = meastime + measdelta / 1000.
        globaltime = localtime + oscdelta / 1000.
        out.write("%f;%d;%d;%d;%d;%d;%d;%f;%f\n" % (meastime, localtime, globaltime, measdelta, oscdelta, measdrift, oscdrift, filteredmeasdrift, filteredoscdrift))
