g++ -o build/mandelbrot mandelbrot.cpp `pkg-config --cflags --libs hpx_application` -lhpx_iostreams -DHPX_APPLICATION_NAME=mandelbrot
