set terminal pngcairo size 1200,700

set output 'canal_A.png'
set title 'Canal A'
set xlabel 'Tiempo (ms)'
set ylabel 'Tension (mV)'
set grid
set key top right
plot 'data_canal_A.txt' using 1:2 with lines linewidth 1.5 title 'Canal A'

set output 'canal_B.png'
set title 'Canal B'
set xlabel 'Tiempo (ms)'
set ylabel 'Tension (mV)'
set grid
set key top right
plot 'data_canal_B.txt' using 1:2 with lines linewidth 1.5 title 'Canal B'

set output