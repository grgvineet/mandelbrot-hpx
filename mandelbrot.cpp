#include <hpx/hpx_init.hpp>
#include <hpx/include/actions.hpp>
#include <hpx/include/util.hpp>
#include <hpx/include/lcos.hpp>
#include <hpx/include/iostreams.hpp>

#include <vector>

#include <boost/format.hpp>

#include "EasyBMP/EasyBMP.h"
#include "EasyBMP/EasyBMP.cpp"

int const sizeY = 2048;
int const sizeX = 2 * sizeY;

int const max_iteration = 1000;

int fractals(float x0, float y0);
std::vector<int> fractals_batch(int start, int num_pixels);

// This is to generate the required boilerplate we need for the remote
// invocation to work.
HPX_PLAIN_ACTION(fractals, fractals_action);
HPX_PLAIN_ACTION(fractals_batch, fractals_batch_action);

///////////////////////////////////////////////////////////////////////////////
int fractals(float x0, float y0)
{
    float x = 0, y = 0;
    int iteration = 0;
    while (x*x + y*y < 2*2 && iteration < max_iteration)
    {
        float xtemp = x*x - y*y + x0;
        y = 2*x*y + y0;
        x = xtemp;

        ++iteration;
    }
    return iteration;
}

std::vector<int> fractals_batch(int start, int num_pixels) {
    // std::cout << "fractals_batch " << start << std::endl; 
    std::vector<int> result;
    for(int x = start; x < start + num_pixels ;  x++) {
        int i = x % sizeX;
        int j = x / sizeX;

        float x0 = (float)i * 3.5f / (float)sizeX - 2.5f;
        float y0 = (float)j * 2.0f / (float)sizeY - 1.0f;
        result.push_back(fractals(x0, y0));
    }
    // std::cout << "start :" << start << std::endl;
    return result;
}

///////////////////////////////////////////////////////////////////////////////
int hpx_main(boost::program_options::variables_map& vm)
{
    BMP SetImage;
    SetImage.SetBitDepth(24);
    SetImage.SetSize(sizeX,sizeY);
    hpx::util::high_resolution_timer t;

    std::cout << "My mandelbrot hj" << std::endl;

    {
        using namespace std;

        using hpx::future;
        using hpx::async;
        using hpx::wait_all;

        int n_hpx_threads = vm["n-hpx-threads"].as<std::uint64_t>();
        int n_os_threads = 1;

        int total_pixels = sizeX * sizeY;
        int pixels_per_thread = total_pixels / n_hpx_threads;
        int remaining_pixels = total_pixels % n_hpx_threads;

        vector<future<vector<int> > > iteration;
        iteration.reserve(total_pixels);

        hpx::cout << "Initial setup completed in "
                  << t.elapsed()
                  << "s. Initializing and running futures...\n"; 
        t.restart();

        hpx::id_type const here = hpx::find_here();
        fractals_batch_action fractal_batch;
        // fractals_action fractal_pixel;


        // for (int i = 0; i < sizeX; i++)
        // {
        //     for (int j = 0; j < sizeY; j++)
        //     {
        //         float x0 = (float)i * 3.5f / (float)sizeX - 2.5f;
        //         float y0 = (float)j * 2.0f / (float)sizeY - 1.0f;

        //         iteration.push_back(async(fractal_pixel, here, x0, y0));
        //     }
        // }

        t.restart();
        wait_all(async(fractal_batch, here, 0, pixels_per_thread));
        std::cout << "Pixels per hpx thread " << pixels_per_thread << ", time taken " << t.elapsed() << "\n";
        
        t.restart();
        int start = 0;
        for(int i = 0; i < remaining_pixels; i++) {
            iteration.push_back(async(fractal_batch, here, start, pixels_per_thread+1));
            start += pixels_per_thread + 1;
        }
        for(int i = remaining_pixels; i < n_hpx_threads; i++) {
            iteration.push_back(async(fractal_batch, here, start, pixels_per_thread));
            start += pixels_per_thread;
        }
        wait_all(iteration);

         hpx::cout << sizeX*sizeY << " calculations run in "
                  << t.elapsed()
                  << "s. Transferring from futures to general memory...\n"; 
        // std::cout << n_hpx_threads << "," << t.elapsed() << "\n";
        t.restart();

        // for (int i = 0; i < sizeX; ++i)
        // {
        //     for (int j = 0; j < sizeY; ++j)
        //     {
        //         int it = iteration[i*sizeY + j].get();
        //         int r = (it) % max_iteration;;
        //         int g = (it * it) % max_iteration;
        //         int b = (it * it * it) % max_iteration;
        //         SetImage.SetPixel(i, j, RGBApixel(r, g, b));
        //     }
        // }
        int pixel_x = 0, pixel_y = 0;
        for(int i = 0; i < n_hpx_threads; i++) {
            vector<int> v = iteration[i].get();
            for(int j = 0; j < v.size(); j++) {
                int it = v[j];
                int r = (it) % max_iteration;;
                int g = (it * it) % max_iteration;
                int b = (it * it * it) % max_iteration;
                SetImage.SetPixel(pixel_x, pixel_y, RGBApixel(r, g, b));
                pixel_x++;
                if (pixel_x == sizeX) {
                    pixel_x = 0;
                    pixel_y ++;
                }
            }
        }
    }

    hpx::cout << "Transfer process completed in "
              << t.elapsed() << "s. Writing to hard disk...\n";
    t.restart();

    SetImage.WriteToFile("out.bmp");

    hpx::cout << "Fractal image written to file \"out.bmp\" from memory in "
              << t.elapsed() << "s.\nInitializing shutdown process.\n"; 

    return hpx::finalize(); // Handles HPX shutdown
}

int main(int argc, char* argv[])
{
    boost::program_options::options_description
       desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

    desc_commandline.add_options()
        ( "n-hpx-threads",
          boost::program_options::value<std::uint64_t>()->default_value(1),
          "Number of hpx threads to use");
    return hpx::init(desc_commandline, argc, argv);
}

