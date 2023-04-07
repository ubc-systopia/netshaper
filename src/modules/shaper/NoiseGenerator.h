#ifndef MINESVPN_NOISE_GENERATOR_H
#define MINESVPN_NOISE_GENERATOR_H

#include <cmath>
#include <cstdlib>

class NoiseGenerator {
private:
  double noiseMultiplier, sensitivity;
  uint64_t maxDecisionSize, minDecisionSize;

  static double gaussian(double mu, double sigma);

  [[nodiscard]] double gaussianDP() const;

public:
  size_t getDPDecision(size_t aggregatedQueueSize);

  NoiseGenerator(double noiseMultiplier, double sensitivity,
                 uint64_t maxDecisionSize = 500000,
                 uint64_t minDecisionSize = 0);
};

#endif //MINESVPN_NOISE_GENERATOR_H