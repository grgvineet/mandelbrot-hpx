i=1
while [ $i -lt 524288 ]
do
    #echo "Running mandelbrot with $i hpx threads"
    ./build/mandelbrot --n-hpx-threads=$i
    #printf "\n"
    i=$(( i+1000 ))
done
