#include <stdio.h>
#include <stdlib.h>
#include "../../NoiseGenerator.h"

int main(int argc, char *argv[])
{
  if(argc != 2){
    printf("Usage: %s <number of samples>\n", argv[0]);
  }

  int sample_num = atoi(argv[1]);
  // create a file to save the samples
  FILE *fp = fopen("samples.txt", "w");
  if(fp == NULL){
    printf("Error: cannot open file samples.txt\n");
    return -1;
  }

  double mu = 1.3;
  double sigma = 7;

  for (int i = 0; i < sample_num; i++){
    double sample = gaussian(mean, variance);
    fprintf(fp, "%f\n", sample);
  }
  fclose(fp);
  return 0;

}
