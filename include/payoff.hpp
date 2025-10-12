#pragma once
#include <algorithm>
struct Payoff {
  virtual double operator()(double ST) const = 0;
  virtual ~Payoff() = default;
};

struct Call : Payoff {
  double K;
  explicit Call(double K):K(K){}
  double operator()(double ST) const override { return std::max(ST - K, 0.0); }
};

struct Put : Payoff {
  double K;
  explicit Put(double K):K(K){}
  double operator()(double ST) const override { return std::max(K - ST, 0.0); }
};
