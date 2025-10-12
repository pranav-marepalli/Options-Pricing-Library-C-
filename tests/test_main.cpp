#include "../include/pricer.hpp"
#include <iostream>
#include <cmath>
#include <vector>
#include <cassert>

static bool approx(double a, double b, double eps=1e-4){ return std::fabs(a-b) < eps; }

int main(){
  BSModel m{100, 0.02, 0.0, 0.20, 1.0};
  Pricer P(m);
  Call call(100); Put put(100);

  // Known BS price (approx from standard calculators)
  auto cg = P.black_scholes(call);
  std::cout << "Call price: " << cg.price << "\n";
  assert(std::fabs(cg.price - 8.916) < 1e-2);

  // Put-call parity
  auto pg = P.black_scholes(put);
  double parity = cg.price - pg.price - (std::exp(-m.q*m.T)*m.S0 - std::exp(-m.r*m.T)*100.0);
  assert(std::fabs(parity) < 1e-6);

  // IV round-trip
  double target = cg.price;
  double iv = P.implied_vol(call, target, 0.2);
  std::cout << "IV: " << iv << "\n";
  assert(std::fabs(iv - 0.20) < 1e-3);

  std::cout << "All tests passed.\n";
  return 0;
}
