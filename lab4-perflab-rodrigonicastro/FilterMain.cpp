#include <stdio.h>
#include "cs1300bmp.h"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include "Filter.h"

using namespace std;

#include "rdtsc.h"

//
// Forward declare the functions
//
Filter * readFilter(string filename);
double applyFilter(Filter *filter, cs1300bmp *input, cs1300bmp *output);

int
main(int argc, char **argv)
{

  if ( argc < 2) {
    fprintf(stderr,"Usage: %s filter inputfile1 inputfile2 .... \n", argv[0]);
  }

  //
  // Convert to C++ strings to simplify manipulation
  //
  string filtername = argv[1];

  //
  // remove any ".filter" in the filtername
  //
  string filterOutputName = filtername;
  string::size_type loc = filterOutputName.find(".filter");
  if (loc != string::npos) {
    //
    // Remove the ".filter" name, which should occur on all the provided filters
    //
    filterOutputName = filtername.substr(0, loc);
  }

  Filter *filter = readFilter(filtername);

  double sum = 0.0;
  int samples = 0;

  struct cs1300bmp *input = new struct cs1300bmp;
  struct cs1300bmp *output = new struct cs1300bmp;

  string outputFilename = "filtered-" + filterOutputName + "-";

  for (int inNum = 2; inNum < argc; inNum++) {
    string inputFilename = argv[inNum];
    string outputFilename = filterOutputName + inputFilename;

    char* inputFilenameChar = (char *) inputFilename.c_str();
    char* outputFilenameChar = (char *) outputFilename.c_str();

    if ( cs1300bmp_readfile( inputFilenameChar, input) ) {
      double sample = applyFilter(filter, input, output);
      sum += sample;
      samples++;
      cs1300bmp_writefile(outputFilenameChar, output);
    }
  }
  fprintf(stdout, "Average cycles per sample is %f\n", sum / samples);

  delete input;
  delete output;

}

class Filter *
readFilter(string filename)
{
  ifstream input(filename.c_str());

  if ( ! input.bad() ) {
    int size = 0;
    input >> size;
    Filter *filter = new Filter(size);
    int div;
    input >> div;
    filter -> setDivisor(div);
    for (int i=0; i < size; i++) {
      for (int j=0; j < size; j++) {
	int value;
	input >> value;
	filter -> set(i,j,value);
      }
    }
    return filter;
  } else {
    cerr << "Bad input in readFilter:" << filename << endl;
    exit(-1);
  }
}


double
applyFilter(class Filter *filter, cs1300bmp *input, cs1300bmp *output)
{

  int cycStart, cycStop;

  cycStart = rdtscll();

  output -> width = input -> width;
  output -> height = input -> height;


const short width_minus_1 = input->width - 1;
const short height_minus_1 = input->height - 1;
const short filter_size = filter->getSize();
const short filter_divisor = filter->getDivisor();

int gets[3][3];

for(int i = 0; i < filter_size; i++){
  for(int j = 0; j < filter_size; j++){
    gets[i][j] = filter->get(i,j);
  }
}

for (int row = 1; row < height_minus_1; row++) {
        for (int col = 1; col < width_minus_1; col++) {
            int r = 0;
            int g = 0;
            int b = 0;

            int row1 = row - 1;
            int row2 = row;
            int row3 = row + 1;

            int col1 = col - 1;
            int col2 = col;
            int col3 = col + 1;

            //row 1
            r += input->color[0][row1][col1] * gets[0][0];
            g += input->color[1][row1][col1] * gets[0][0];
            b += input->color[2][row1][col1] * gets[0][0];

            r += input->color[0][row1][col2] * gets[0][1];
            g += input->color[1][row1][col2] * gets[0][1];
            b += input->color[2][row1][col2] * gets[0][1];

            r += input->color[0][row1][col3] * gets[0][2];
            g += input->color[0][row1][col3] * gets[0][2];
            b += input->color[0][row1][col3] * gets[0][2];

            //row 2
            r += input->color[0][row2][col1] * gets[1][0];
            g += input->color[1][row2][col1] * gets[1][0];
            b += input->color[2][row2][col1] * gets[1][0];

            r += input->color[0][row2][col2] * gets[1][1];
            g += input->color[1][row2][col2] * gets[1][1];
            b += input->color[2][row2][col2] * gets[1][1];

            r += input->color[0][row2][col3] * gets[1][2];
            g += input->color[1][row2][col3] * gets[1][2];
            b += input->color[2][row2][col3] * gets[1][2];

            //row 3
            r += input->color[0][row3][col1] * gets[2][0];
            g += input->color[1][row3][col1] * gets[2][0];
            b += input->color[2][row3][col1] * gets[2][0];

            r += input->color[0][row3][col2] * gets[2][1];
            g += input->color[1][row3][col2] * gets[2][1];
            b += input->color[2][row3][col2] * gets[2][1];

            r += input->color[0][row3][col3] * gets[2][2];
            g += input->color[1][row3][col3] * gets[2][2];
            b += input->color[2][row3][col3] * gets[2][2];

            r /= filter_divisor;
            g /= filter_divisor;
            b /= filter_divisor;

            r = max(0, min(r, 255));
            g = max(0, min(b, 255));
            b = max(0, min(g, 255));

            output->color[0][row][col] = r;            
            output->color[1][row][col] = g;                                    
            output->color[2][row][col] = b;
        }
    }

  cycStop = rdtscll();
  double diff = cycStop - cycStart;
  double diffPerPixel = diff / (output -> width * output -> height);
  fprintf(stderr, "Took %f cycles to process, or %f cycles per pixel\n",
	  diff, diff / (output -> width * output -> height));
  return diffPerPixel;
}