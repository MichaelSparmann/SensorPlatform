# Remote device protocol to driver mapper
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


import sensorplatform.rfdevice
import sensorplatform.device.iris.multisensor


# Mapping table: (ProtoVendor, ProtoType[, ProtoVersion]) => RFDevice subclass
RFDevMap = {
    0x53414149: {
        0x5053: sensorplatform.device.iris.multisensor.MultiSensorDevice,
    },
}


# Instantiate the correct driver object for a given node identification
def instantiate(manager, id):
    c = sensorplatform.rfdevice.RFDevice
    if id.info.protoVendor in RFDevMap and id.info.protoType in RFDevMap[id.info.protoVendor]:
        e = RFDevMap[id.info.protoVendor][id.info.protoType]
        if isinstance(e, dict):
            if id.info.protoVersion in e: c = e[id.info.protoVersion]
        else: c = e
    return c(manager, id)
