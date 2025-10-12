#include "../include/pricer.hpp"
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>
#include <iostream>

int main(){
  using namespace std::chrono;
  std::mt19937_64 rng(42);
  std::uniform_real_distribution<> uS(50,150), uK(50,150), uSig(0.05,0.6), uT(0.05,2.0);
  std::vector<double> lat;
  lat.reserve(2000);
  for(int i=0;i<2000;++i){
    BSModel m{uS(rng), 0.02, 0.0, 0.20, uT(rng)};
    Pricer P(m);
    Call c(uK(rng));
    auto t0 = high_resolution_clock::now();
    auto g = P.black_scholes(c);
    auto t1 = high_resolution_clock::now();
    lat.push_back(duration<double, std::micro>(t1-t0).count()); // microseconds
  }
  std::sort(lat.begin(), lat.end());
  auto p50 = lat[(size_t)(0.50*lat.size())];
  auto p95 = lat[(size_t)(0.95*lat.size())];
  double mean=0; for(double x:lat) mean+=x; mean/=lat.size();
  double per_sec = 1e6 / p50; // rough: per second based on median
  std::cout << "p50(us)="<<p50<<" p95(us)="<<p95<<" mean(us)="<<mean<<" approx prices/s~"<<per_sec<<"\n";
  return 0;
}
