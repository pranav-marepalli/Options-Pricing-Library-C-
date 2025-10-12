#pragma once
#include "model.hpp"
#include "payoff.hpp"
#include <tuple>

struct PriceGreeks { double price, delta, gamma, vega, theta; };

class Pricer {
public:
  explicit Pricer(const BSModel& m) : m_(m) {}
  PriceGreeks black_scholes(const Payoff& payoff) const;
  double implied_vol(const Payoff& payoff, double target, double guess=0.2) const;
private:
  BSModel m_;
};
