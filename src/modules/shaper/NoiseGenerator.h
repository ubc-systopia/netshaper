#ifndef MINESVPN_NOISE_GENERATOR_H
#define MINESVPN_NOISE_GENERATOR_H

#include <cmath>
#include <cstdlib>

class NoiseGenerator {
private:
  double noiseMultiplier, sensitivity;
  int maxDecisionSize, minDecisionSize;

  static double gaussian(double mu, double sigma);

  [[nodiscard]] double gaussianDP() const;

public:
  size_t getDPDecision(size_t aggregatedQueueSize);

  NoiseGenerator(double noiseMultiplier, double sensitivity,
                 int maxDecisionSize = 10000, int minDecisionSize = 0);
};

#endif //MINESVPN_NOISE_GENERATOR_H