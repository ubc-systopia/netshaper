#ifndef MINESVPN_NOISE_GENERATOR_H
#define MINESVPN_NOISE_GENERATOR_H

#include <cmath>
#include <cstdlib>

class NoiseGenerator {
private:
  double epsilon, delta, sensitivity;
  int minDecisionSize, maxDecisionSize;

  static double gaussian(double mu, double sigma);

  [[nodiscard]] double gaussianDP() const;

public:
  size_t getDPDecision(size_t aggregatedQueueSize);

  explicit NoiseGenerator(double epsilon = 0.01, double sensitivity = 10000,
                          double
                          delta = 0.00001, int maxDecisionSize = 10000,
                          int minDecisionSize = 0);
};

#endif //MINESVPN_NOISE_GENERATOR_H