set tmargin 1
set key bottom left at character 11,0.2 horizontal
set xtics border out nomirror norotate autojustify 100 offset 0,0.5
set ytics border out nomirror norotate autojustify 0.1
set datafile separator ";"
set style data lines
set terminal pdf size 15.6cm,5.5cm font "Palatino Linotype,16.5"
set xlabel "Zeit [s]" offset 0,1
set ylabel "Vibration (RMS) [g]" offset 1,0
set title "Druckvorgang Objekt 1, Durchgang 1 (Sensoreinheit 9: Druckkopf)" offset 0,-1
set xrange [ 0 : 1200 ]
set yrange [ 0 : 0.3 ]
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run9-dev9-accelrms-xyz-full.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000009-accelrms.csv" \
    using 0:2 lt rgb "#0000ff" title "X" axes x1y1, \
 "" using 0:3 lt rgb "#ff0000" title "Y" axes x1y1, \
 "" using 0:4 lt rgb "#00ff00" title "Z" axes x1y1
set title "Druckvorgang Objekt 2, Durchgang 1 (Sensoreinheit 9: Druckkopf)" offset 0,-1
set xrange [ 50 : 1250 ]
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run8-dev9-accelrms-xyz-full.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\008_20170123-test5-obj9\\350f0fee-e187-11e6-8588-a0a8cd46b0ad\\53414149534D505300000009-accelrms.csv" \
    using 0:2 lt rgb "#0000ff" title "X" axes x1y1, \
 "" using 0:3 lt rgb "#ff0000" title "Y" axes x1y1, \
 "" using 0:4 lt rgb "#00ff00" title "Z" axes x1y1
set title "Druckvorgang Objekt 1, Durchgang 2 (Sensoreinheit 9: Druckkopf)" offset 0,-1
set xrange [ 400 : 1600 ]
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run7-dev9-accelrms-xyz-full.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\007_20170123-test4-obj7\\cd2e6e1a-e182-11e6-a8ad-a0a8cd46b0ad\\53414149534D505300000009-accelrms.csv" \
    using 0:2 lt rgb "#0000ff" title "X" axes x1y1, \
 "" using 0:3 lt rgb "#ff0000" title "Y" axes x1y1, \
 "" using 0:4 lt rgb "#00ff00" title "Z" axes x1y1
set title "Druckvorgang Objekt 2, Durchgang 2 (Sensoreinheit 9: Druckkopf)" offset 0,-1
set xrange [ 350 : 1550 ]
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run6-dev9-accelrms-xyz-full.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\006_20170123-test3-obj9\\ff62ca64-e17d-11e6-9e53-a0a8cd46b0ad\\53414149534D505300000009-accelrms.csv" \
    using 0:2 lt rgb "#0000ff" title "X" axes x1y1, \
 "" using 0:3 lt rgb "#ff0000" title "Y" axes x1y1, \
 "" using 0:4 lt rgb "#00ff00" title "Z" axes x1y1
set ytics border out nomirror norotate autojustify 0.01
set title "Druckvorgang Objekt 1, Durchgang 1 (Sensoreinheit 2: Tischplatte)" offset 0,-1
set xrange [ 0 : 1200 ]
set yrange [ 0 : 0.03 ]
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run9-dev2-accelrms-xyz-full.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000002-accelrms.csv" \
    using 0:2 lt rgb "#0000ff" title "X" axes x1y1, \
 "" using 0:3 lt rgb "#ff0000" title "Y" axes x1y1, \
 "" using 0:4 lt rgb "#00ff00" title "Z" axes x1y1
set title "Druckvorgang Objekt 2, Durchgang 1 (Sensoreinheit 2: Tischplatte)" offset 0,-1
set xrange [ 50 : 1250 ]
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run8-dev2-accelrms-xyz-full.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\008_20170123-test5-obj9\\350f0fee-e187-11e6-8588-a0a8cd46b0ad\\53414149534D505300000002-accelrms.csv" \
    using 0:2 lt rgb "#0000ff" title "X" axes x1y1, \
 "" using 0:3 lt rgb "#ff0000" title "Y" axes x1y1, \
 "" using 0:4 lt rgb "#00ff00" title "Z" axes x1y1
set title "Druckvorgang Objekt 1, Durchgang 2 (Sensoreinheit 2: Tischplatte)" offset 0,-1
set xrange [ 400 : 1600 ]
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run7-dev2-accelrms-xyz-full.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\007_20170123-test4-obj7\\cd2e6e1a-e182-11e6-a8ad-a0a8cd46b0ad\\53414149534D505300000002-accelrms.csv" \
    using 0:2 lt rgb "#0000ff" title "X" axes x1y1, \
 "" using 0:3 lt rgb "#ff0000" title "Y" axes x1y1, \
 "" using 0:4 lt rgb "#00ff00" title "Z" axes x1y1
set title "Druckvorgang Objekt 2, Durchgang 2 (Sensoreinheit 2: Tischplatte)" offset 0,-1
set xrange [ 350 : 1550 ]
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run6-dev2-accelrms-xyz-full.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\006_20170123-test3-obj9\\ff62ca64-e17d-11e6-9e53-a0a8cd46b0ad\\53414149534D505300000002-accelrms.csv" \
    using 0:2 lt rgb "#0000ff" title "X" axes x1y1, \
 "" using 0:3 lt rgb "#ff0000" title "Y" axes x1y1, \
 "" using 0:4 lt rgb "#00ff00" title "Z" axes x1y1
set title "Druckvorgang Objekt 1, Durchgang 1 (Sensoreinheit 1: Raum-Boden)" offset 0,-1
set xrange [ 0 : 1200 ]
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run9-dev1-accelrms-xyz-full.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000001-accelrms.csv" \
    using 0:2 lt rgb "#0000ff" title "X" axes x1y1, \
 "" using 0:3 lt rgb "#ff0000" title "Y" axes x1y1, \
 "" using 0:4 lt rgb "#00ff00" title "Z" axes x1y1
set ytics border out nomirror norotate autojustify 0.5
set ylabel "Temperatur [°C]" offset 1,0
set xrange [ 0 : 1030 ]
set yrange [ 56 : 57.6 ]
set title "Druckvorgang Objekt 1, Durchgang 1 (Sensoreinheit 8: Druckbett)" offset 0,-1
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run9-dev8-temperature.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000008-20.csv" every 50 using 0:3 lt rgb "#0000ff" title "BMP280" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000008-21.csv" every 10 using 0:3 lt rgb "#ff0000" title "Si7020" axes x1y1
set yrange [ 48 : 50.5 ]
set title "Druckvorgang Objekt 1, Durchgang 1 (Sensoreinheit 9: Druckkopf)" offset 0,-1
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run9-dev9-temperature.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000009-20.csv" every 50 using 0:3 lt rgb "#0000ff" title "BMP280" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000009-21.csv" every 10 using 0:3 lt rgb "#ff0000" title "Si7020" axes x1y1
set ytics border out nomirror norotate autojustify 5
set xrange [ 0 : 1200 ]
set yrange [ 20 : 55 ]
set title "Aufheizphase (Sensoreinheit 8: Druckbett)" offset 0,-1
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run4-dev8-temperature.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\004_20170123-test1-obj9\\bc580240-e175-11e6-b791-a0a8cd46b0ad\\53414149534D505300000008-20.csv" every 50 using 0:3 lt rgb "#0000ff" title "BMP280" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\004_20170123-test1-obj9\\bc580240-e175-11e6-b791-a0a8cd46b0ad\\53414149534D505300000008-21.csv" every 10 using 0:3 lt rgb "#ff0000" title "Si7020" axes x1y1
set ytics border out nomirror norotate autojustify 2
set yrange [ 23 : 35 ]
set title "Aufheizphase (Sensoreinheit 9: Druckkopf)" offset 0,-1
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run4-dev9-temperature.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\004_20170123-test1-obj9\\bc580240-e175-11e6-b791-a0a8cd46b0ad\\53414149534D505300000009-20.csv" every 50 using 0:3 lt rgb "#0000ff" title "BMP280" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\004_20170123-test1-obj9\\bc580240-e175-11e6-b791-a0a8cd46b0ad\\53414149534D505300000009-21.csv" every 10 using 0:3 lt rgb "#ff0000" title "Si7020" axes x1y1
set xtics border out nomirror norotate autojustify 0.1 offset 0,0.5
set ytics border out nomirror norotate autojustify 0.05
set ylabel "Beschleunigung [g]" offset 1,0
set xrange [ 201 : 202 ]
set yrange [ -0.1 : 0.1 ]
set title "Druckvorgang Objekt 2, Durchgang 1 (Accelerometer Druckergehäuse X-Achse)" offset 0,-1
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run8-case-16-x-201s.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\008_20170123-test5-obj9\\350f0fee-e187-11e6-8588-a0a8cd46b0ad\\53414149534D505300000004-16.csv" using ($0/400+0.017786):2 lt rgb "#0000ff" title "S4" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\008_20170123-test5-obj9\\350f0fee-e187-11e6-8588-a0a8cd46b0ad\\53414149534D505300000005-16.csv" using ($0/400+0.021811):2 lt rgb "#ff0000" title "S5" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\008_20170123-test5-obj9\\350f0fee-e187-11e6-8588-a0a8cd46b0ad\\53414149534D505300000006-16.csv" using ($0/400-0.001212):2 lt rgb "#00ff00" title "S6" axes x1y1
set title "Druckvorgang Objekt 2, Durchgang 1 (Accelerometer Druckergehäuse Y-Achse)" offset 0,-1
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run8-case-16-y-201s.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\008_20170123-test5-obj9\\350f0fee-e187-11e6-8588-a0a8cd46b0ad\\53414149534D505300000004-16.csv" using ($0/400+0.017786):3 lt rgb "#0000ff" title "S4" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\008_20170123-test5-obj9\\350f0fee-e187-11e6-8588-a0a8cd46b0ad\\53414149534D505300000005-16.csv" using ($0/400+0.021811):3 lt rgb "#ff0000" title "S5" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\008_20170123-test5-obj9\\350f0fee-e187-11e6-8588-a0a8cd46b0ad\\53414149534D505300000006-16.csv" using ($0/400-0.001212):3 lt rgb "#00ff00" title "S6" axes x1y1
set yrange [ -1.1 : -0.9 ]
set title "Druckvorgang Objekt 2, Durchgang 1 (Accelerometer Druckergehäuse Z-Achse)" offset 0,-1
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run8-case-16-z-201s.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\008_20170123-test5-obj9\\350f0fee-e187-11e6-8588-a0a8cd46b0ad\\53414149534D505300000004-16.csv" using ($0/400+0.017786):4 lt rgb "#0000ff" title "S4" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\008_20170123-test5-obj9\\350f0fee-e187-11e6-8588-a0a8cd46b0ad\\53414149534D505300000005-16.csv" using ($0/400+0.021811):4 lt rgb "#ff0000" title "S5" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\008_20170123-test5-obj9\\350f0fee-e187-11e6-8588-a0a8cd46b0ad\\53414149534D505300000006-16.csv" using ($0/400-0.001212):4 lt rgb "#00ff00" title "S6" axes x1y1
set key off
set xtics border out nomirror norotate autojustify 100 offset 0,0.5
set ytics border out nomirror norotate autojustify 0.25
set ylabel "Zeitabweichung [s]" offset 1,0
set xrange [ 0 : 1000 ]
set yrange [ -0.5 : 0.5 ]
set title "Druckvorgang Objekt 1, Durchgang 1 (Zeitdrift der Sensoreinheiten)" offset 0,-1
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run9-timingdelta.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000001-ta.csv" every 50 using 0:($5/1000000) title "1" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000002-ta.csv" every 50 using 0:($5/1000000) title "2" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000003-ta.csv" every 50 using 0:($5/1000000) title "3" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000004-ta.csv" every 50 using 0:($5/1000000) title "4" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000005-ta.csv" every 50 using 0:($5/1000000) title "5" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000006-ta.csv" every 50 using 0:($5/1000000) title "6" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000007-ta.csv" every 50 using 0:($5/1000000) title "7" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000008-ta.csv" every 50 using 0:($5/1000000) title "8" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000009-ta.csv" every 50 using 0:($5/1000000) title "9" axes x1y1
set terminal pdf size 15.6cm,8cm font "Palatino Linotype,16.5"
set ytics border out nomirror norotate autojustify 200
set ylabel "Taktabweichung [ppm]" offset 1,0
set xrange [ 0 : 1000 ]
set yrange [ -500 : 500 ]
set title "Druckvorgang Objekt 1, Durchgang 1 (Taktratenabweichungen)" offset 0,-1
set output "D:\\Uni\\Diplomarbeit\\Ausarbeitung\\tex\\graphics\\pdf\\plots\\run9-timingdrift.pdf"
plot "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000001-ta.csv" every 50 using 0:9 title "1" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000002-ta.csv" every 50 using 0:9 title "2" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000003-ta.csv" every 50 using 0:9 title "3" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000004-ta.csv" every 50 using 0:9 title "4" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000005-ta.csv" every 50 using 0:9 title "5" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000006-ta.csv" every 50 using 0:9 title "6" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000007-ta.csv" every 50 using 0:9 title "7" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000008-ta.csv" every 50 using 0:9 title "8" axes x1y1, \
     "D:\\Uni\\Diplomarbeit\\Messungen\\009_20170123-test6-obj7\\b675b21e-e18a-11e6-ac51-a0a8cd46b0ad\\53414149534D505300000009-ta.csv" every 50 using 0:9 title "9" axes x1y1
