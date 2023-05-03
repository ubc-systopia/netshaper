//
// Created by Amir Sabzi
//
#include <iostream>
#include "NoiseGenerator.h"

double NoiseGenerator::gaussian(double mu, double sigma) {
  double U1, U2, W, mult;
  static double X1, X2;
  static int call = 0;

  if (call == 1) {
    call = !call;
    return (mu + sigma * (double) X2);
  }

  do {
    U1 = -1 + (double) std::rand() / RAND_MAX * 2;
    U2 = -1 + (double) std::rand() / RAND_MAX * 2;
    W = pow(U1, 2) + pow(U2, 2);
  } while (W >= 1 || W == 0);

  mult = sqrt((-2 * log(W)) / W);
  X1 = U1 * mult;
  X2 = U2 * mult;

  call = !call;

  return (mu + sigma * (double) X1);
}

double NoiseGenerator::gaussianDP() const {
  double mu = 0;
  double sigma = sensitivity * noiseMultiplier;
  return gaussian(mu, sigma);
}

size_t NoiseGenerator::getDPDecision(size_t aggregatedQueueSize) {
  auto gaussianNoise = gaussianDP();
  return (size_t) floor(
      std::max((double) minDecisionSize, std::min((double) maxDecisionSize,
                                                  (double) aggregatedQueueSize +
                                                  gaussianNoise)));
}

NoiseGenerator::NoiseGenerator(double noiseMultiplier, double sensitivity,
                               uint64_t maxDecisionSize,
                               uint64_t minDecisionSize)
    : noiseMultiplier(noiseMultiplier), sensitivity(sensitivity),
      maxDecisionSize(maxDecisionSize), minDecisionSize(minDecisionSize) {

}