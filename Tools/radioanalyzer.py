# SensorPlatform Radio Telemetry Analyzer
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

iir_coeff = 0.1

with open(sys.argv[1] + "-1.csv") as f:
    out = open(sys.argv[1] + "-ra.csv", "w")
    out.write("Time;SOFReceived;SOFTimingFailed;SOFDiscontinuity;TXAttemptCount;TXACKCount;TXLossCount;FrameRate;BadTimingRate;DiscontRate;TXAttemptRate;TxACKRate;TxLossRate;BadTimingRatio;DiscontRatio;TXACKRatio;FilteredTxAttemptRate;FilteredTxACKRate;FilteredTXLossRate;FilteredTxACKRatio\n")
    out.write("ms;;;;;;;Hz;Hz;Hz;Hz;Hz;Hz;;;;Hz;Hz;Hz;\n")
    first = True
    for line in f.readlines()[2:]:
        line = line.split(";")
        meastime, sof, badtiming, discont, txattempt, txack = float(line[0]), int(line[1]), int(line[2]), int(line[3]), int(line[4]), int(line[5])
        if first:
            first = False
            sofoffset, badtimingoffset, discontoffset, txattemptoffset, txackoffset = -sof, -badtiming, -discont, -txattempt, -txack
            lastmeastime, lastsof, lastbadtiming, lastdiscont, lasttxattempt, lasttxack = 0, 0, 0, 0, 0, 0
            filteredtxattemptrate, filteredtxackrate = 0, 0
        timestep = (meastime - lastmeastime) / 1000.
        if sof + sofoffset < lastsof: sofoffset += 65536
        if badtiming + badtimingoffset < lastbadtiming: badtimingoffset += 65536
        if discont + discontoffset < lastdiscont: discontoffset += 65536
        if txattempt + txattemptoffset < lasttxattempt: txattemptoffset += 65536
        if txack + txackoffset < lasttxack: txackoffset += 65536
        sof += sofoffset
        badtiming += badtimingoffset
        discont += discontoffset
        txattempt += txattemptoffset
        txack += txackoffset
        txloss = txattempt - txack
        sofrate, badtimingrate, discontrate, txattemptrate, txackrate = (sof - lastsof) / timestep, (badtiming - lastbadtiming) / timestep, (discont - lastdiscont) / timestep, (txattempt - lasttxattempt) / timestep, (txack - lasttxack) / timestep
        txlossrate = (txattempt - lasttxattempt - txack + lasttxack) / timestep
        filteredtxattemptrate = filteredtxattemptrate * (1 - iir_coeff) + txattemptrate * iir_coeff
        filteredtxackrate = filteredtxackrate * (1 - iir_coeff) + txackrate * iir_coeff
        filteredtxlossrate = filteredtxattemptrate - filteredtxackrate
        filteredtxackratio = filteredtxackrate / filteredtxattemptrate if filteredtxattemptrate > 0 else 0
        badtimingratio = (badtiming - lastbadtiming) / (sof - lastsof) if sof != lastsof else 0
        discontratio = (discont - lastdiscont) / (sof - lastsof) if sof != lastsof else 0
        txackratio = (txack - lasttxack) / (txattempt - lasttxattempt) if txattempt != lasttxattempt else 0
        lastmeastime, lastsof, lastbadtiming, lastdiscont, lasttxattempt, lasttxack = meastime, sof, badtiming, discont, txattempt, txack
        out.write("%f;%d;%d;%d;%d;%d;%d;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f\n" % (meastime, sof, badtiming, discont, txattempt, txack, txloss, sofrate, badtimingrate, discontrate, txattemptrate, txackrate, txlossrate, badtimingratio, discontratio, txackratio, filteredtxattemptrate, filteredtxackrate, filteredtxlossrate, filteredtxackratio))
