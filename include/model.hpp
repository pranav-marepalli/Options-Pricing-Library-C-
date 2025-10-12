#pragma once
struct BSModel {
  double S0;   // spot
  double r;    // risk-free rate (annualized, cont. comp.)
  double q;    // dividend yield
  double sigma;// volatility
  double T;    // time to maturity (years)
};
