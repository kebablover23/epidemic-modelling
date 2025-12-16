set xlabel 'Dage'
set ylabel 'Antal individer'
set title 'SIR Model - Deterministisk Simulering'
set grid
set key outside top center
set key maxcols 5
set style line 1 lc rgb '#00FF00' lt 1 lw 2
set style line 2 lc rgb '#FFA500' lt 1 lw 2
set style line 3 lc rgb '#FF0000' lt 1 lw 2
set style line 4 lc rgb '#800080' lt 1 lw 2
set style line 5 lc rgb '#0000FF' lt 1 lw 2
plot \
    'data_file.txt' using 1:2 with lines ls 1 title 'S (Input 1)', \
    'data_file.txt' using 1:3 with lines ls 3 title 'I (Input 1)', \
    'data_file.txt' using 1:4 with lines ls 5 title 'R (Input 1)'
pause -1 'Tast enter for at lukke'
