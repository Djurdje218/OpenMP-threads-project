#include <iostream>
#include <string.h>
#include <fstream>
#include <vector>
#include <omp.h>
#include <math.h>
#include <string>

void getMinMaxValues(int ignorePerChannel, int histogram[], int &minY, int &maxY)
{
    int sum1 = 0;
        for(int i = 0; i < 256; i++)
        {
            sum1 += histogram[i];
            if(sum1 >= ignorePerChannel && sum1 != 0)
            {
                minY = i;
                break;
            }
        }
        
        sum1 = 0;
        for(int i = 255; i >= 0; i--)
        {
            sum1+= histogram[i];
            if(sum1 >= ignorePerChannel && sum1 != 0)
            {
                maxY = i; 
                break;
            }
        }
};

int main(int argc, char** argv)
{
    std::string inputFile, outputFile;
    float coef = 0.0;
    int numThreads = -1;

    if(argc != 8 && argc != 9)
    {   std::cerr<< "Invalid input data." << '\n'
                 << " Usage :  --input <file> --output <file> --coef <value> [--no-omp|--omp-threads <num|default>]\n";
                 return 1;
    }

    for(int i = 1; i < argc; i++) 
    {
      if (!strcmp(argv[i],"--input")) inputFile = argv[i + 1];
      else if (!strcmp(argv[i],"--output")) outputFile = argv[i + 1];
      else if (!strcmp(argv[i], "--coef")) coef = static_cast<float>(atof(argv[i + 1]));
      else if (!strcmp(argv[i], "--no-omp")) numThreads = 1;
      else if (!strcmp(argv[i], "--omp-threads")){
        if(!strcmp(argv[i + 1],"default")) numThreads = omp_get_max_threads();
        else numThreads = atoi(argv[i + 1]);
      } 
    }
    if(coef < 0.0f || coef >= 0.5f)
    {
        std::cerr << "Error - Incorrect value of Coefficient. Range - [0.0,0.5] \n";
        return 1;
    }

    if(numThreads == -1) 
    {
        std::cerr << "Error - Parralelism mode not set correctly \n Usage: --no-omp | --omp-threads [num_threads | default]";
        return 1; 
    }

    std::ifstream file(inputFile, std::ios::binary);
    if (!file)
    {
        std::cerr << "Error while opening the input file!\n";
        return 1;
    }

    std::string format;
    int width, height, max_val; 
    file >> format >> width >> height >> max_val;

    if (max_val != 255)
    {
        std::cerr << "Error! Only 8-bit images (max_val = 255) are supported.\n";
        return 1;
    }

    if(format != "P6" && format != "P5")
    {
        std::cerr << "Error! unsupported file format ( supports P6 and P5 format with max values of 256)";
        return 1;
    }
    file.ignore(); 

    size_t pixelCountAll = (format == "P6") ? 
    static_cast<size_t>(width) * height * 3 : 
    static_cast<size_t>(width) * height;

    std::vector<unsigned char> pixels(pixelCountAll);
    file.read(reinterpret_cast<char*>(pixels.data()), pixels.size());
    file.close();

    omp_set_num_threads(numThreads);

    int histogram1[256] = {0};
    int histogram2[256] = {0};
    int histogram3[256] = {0};

    size_t ignorePerChannel = static_cast<size_t>(width * height * coef);

    volatile double tstart = omp_get_wtime();

    int maxY, minY;

    if (format == "P6")
    {
        if (numThreads > 1) 
        {   
            #pragma omp parallel num_threads(numThreads)
            {
                int localHistogram1[256] = {0};
                int localHistogram2[256] = {0};
                int localHistogram3[256] = {0};
        
                #pragma omp for schedule(dynamic,10000)
                for (size_t i = 0; i < pixelCountAll; i += 3)
                {
                    localHistogram1[(int)pixels[i]]++;
                    localHistogram2[(int)pixels[i+1]]++;
                    localHistogram3[(int)pixels[i+2]]++;
                }
        
                #pragma omp critical
                {
                    for (int j = 0; j < 256; j++)
                    {
                        histogram1[j] += localHistogram1[j];
                        histogram2[j] += localHistogram2[j];
                        histogram3[j] += localHistogram3[j];
                    }
                }
            }
        }
        else
        {
            for (size_t i = 0; i < pixelCountAll; i += 3)
            {
                histogram1[pixels[i]]++;
                histogram2[pixels[i+1]]++;
                histogram3[pixels[i+2]]++;
            }
        }
    
        int min1Y = 0, max1Y = 0;
        getMinMaxValues(ignorePerChannel,histogram1, min1Y, max1Y);

        int min2Y, max2Y;
        getMinMaxValues(ignorePerChannel, histogram2, min2Y, max2Y);

        int min3Y, max3Y;
        getMinMaxValues(ignorePerChannel, histogram3, min3Y, max3Y);

        minY = std::min(min1Y, std::min(min2Y, min3Y));
        maxY = std::max(max1Y, std::max(max2Y, max3Y));

    }
    else 
    {
        if (numThreads > 1) 
        {
            #pragma omp parallel num_threads(numThreads)
            {
                int localHistogram1[256] = {0};

                #pragma omp for schedule(dynamic,10000) 
                for (size_t i = 0; i < pixelCountAll; i++)
                {
                    localHistogram1[(int)pixels[i]]++;
                }
            
                #pragma omp critical
                {
                    for (int j = 0; j < 256; j++)
                    {
                        histogram1[j] += localHistogram1[j];
                    }
                }
            }
        }
        else
        {
            for (size_t i = 0; i < pixelCountAll; i++ )
            {
                histogram1[pixels[i]]++;
            }
        }
        
        getMinMaxValues(ignorePerChannel, histogram1, minY, maxY);
    }
    

    if( maxY == minY)
    {
        std::cout << " All pixels are the same values, no need to adjust it\n"; 
    }
    else
    {
        float division = 255.0f / (maxY - minY);
        #pragma omp parallel num_threads(numThreads)
        {
            #pragma omp for schedule(dynamic,10000)
            for(size_t i = 0; i < pixelCountAll; i++)
            {
                int oldY = pixels[i];
                int newY =(oldY - minY) * division; 
                pixels[i] = static_cast<unsigned char>(std::max(0, std::min(255, newY)));
            }
        }
    }
    volatile double tend = omp_get_wtime();
    printf("Time (%i threads): %lg\n", numThreads,(tend - tstart) * 1000 );

    std::ofstream outFile(outputFile, std::ios::binary);
    outFile << format << "\n" << width << " " << height << "\n255\n";
    outFile.write(reinterpret_cast<char*>(pixels.data()), pixels.size());
    outFile.close();


    return 0;
}
