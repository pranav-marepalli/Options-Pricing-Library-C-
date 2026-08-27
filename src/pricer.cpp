#include "pricer.hpp"
#include <cmath>
#include <cassert>
#include <algorithm>

namespace {
inline double norm_cdf(double x){ return 0.5 * std::erfc(-x/std::sqrt(2.0)); }
inline double norm_pdf(double x){ static const double invsqrt2pi = 0.3989422804014327; return invsqrt2pi*std::exp(-0.5*x*x); }

struct Analytic {
  static PriceGreeks call(const BSModel& m, double K){
    double S=m.S0, r=m.r, q=m.q, v=m.sigma, T=m.T;
    double sqrtT = std::sqrt(std::max(T,1e-12));
    double d1 = (std::log(S/K) + (r - q + 0.5*v*v)*T)/(v*sqrtT);
    double d2 = d1 - v*sqrtT;
    double df_r = std::exp(-r*T);
    double df_q = std::exp(-q*T);
    double Nd1 = norm_cdf(d1), Nd2 = norm_cdf(d2);
    double price = df_q*S*Nd1 - df_r*K*Nd2;
    double delta = df_q*Nd1;
    double gamma = df_q*norm_pdf(d1)/(S*v*sqrtT);
    double vega = df_q*S*norm_pdf(d1)*sqrtT;
    double theta = -df_q*S*norm_pdf(d1)*v/(2*sqrtT) - r*df_r*K*Nd2 + q*df_q*S*Nd1;
    return {price, delta, gamma, vega, theta};
  }
  static PriceGreeks put(const BSModel& m, double K){
    // put-call parity for price; greeks from symmetries
    PriceGreeks c = call(m, K);
    double df_r = std::exp(-m.r*m.T);
    double df_q = std::exp(-m.q*m.T);
    double price = c.price - (df_q*m.S0 - df_r*K);
    double delta = c.delta - df_q;
    double gamma = c.gamma;
    double vega  = c.vega;
    double theta = c.theta + r_term(m, K) - q_term(m);
    return {price, delta, gamma, vega, theta};
  }
  // Put-call parity correction terms for theta:
  //   theta_put = theta_call + r*K*exp(-r*T) - q*S0*exp(-q*T)
  static double r_term(const BSModel& m, double K){ return m.r*K*std::exp(-m.r*m.T); }
  static double q_term(const BSModel& m){ return m.q*m.S0*std::exp(-m.q*m.T); }
};

} // namespace

PriceGreeks Pricer::black_scholes(const Payoff& payoff) const {
  // Detect basic vanilla types with dynamic_cast; fallback to finite differences for exotic payoff price only.
  if(auto c = dynamic_cast<const Call*>(&payoff)) {
    return Analytic::call(m_, c->K);
  }
  if(auto p = dynamic_cast<const Put*>(&payoff)) {
    return Analytic::put(m_, p->K);
  }
  // Fallback: price via finite differences with small bump (Greeks not provided)
  double eps = 1e-4;
  BSModel m2 = m_;
  auto price_only = [&](double S)->double{
    // One-step lognormal expectation (analytic for general payoff not available); simple MC would be heavy—so use small bump approx:
    // Here we approximate price by expected discounted payoff under lognormal via Gauss-Hermite (2-point) quadrature.
    double mu = (m_.r - m_.q - 0.5*m_.sigma*m_.sigma)*m_.T;
    double sd = m_.sigma*std::sqrt(m_.T);
    double z1 =  1.0;
    double z2 = -1.0;
    double ST1 = S*std::exp(mu + sd*z1);
    double ST2 = S*std::exp(mu + sd*z2);
    double df  = std::exp(-m_.r*m_.T);
    return 0.5*df*(payoff(ST1) + payoff(ST2));
  };
  double p0 = price_only(m_.S0);
  double p_up = price_only(m_.S0*(1+eps));
  double p_dn = price_only(m_.S0*(1-eps));
  double delta = (p_up - p_dn)/(2*eps*m_.S0);
  double gamma = (p_up - 2*p0 + p_dn)/(eps*eps*m_.S0*m_.S0);
  double vega = 0.0, theta = 0.0; // omitted for general payoff
  return {p0, delta, gamma, vega, theta};
}

double Pricer::implied_vol(const Payoff& payoff, double target, double guess) const {
  // Newton-Raphson with bracketing + damping
  double vol = std::max(1e-6, std::min(5.0, guess));
  double lo=1e-6, hi=5.0;
  double last = vol;
  for(int iter=0; iter<100; ++iter){
    BSModel m2 = m_; m2.sigma = vol;
    // Compute price and vega analytically when possible
    double price, vega;
    if(auto c = dynamic_cast<const Call*>(&payoff)){
      auto cg = Analytic::call(m2, c->K);
      price = cg.price; vega = cg.vega;
    } else if(auto p = dynamic_cast<const Put*>(&payoff)){
      auto pg = Analytic::put(m2, p->K);
      price = pg.price; vega = pg.vega;
    } else {
      // Fallback finite-diff on vol: price the payoff under three distinct
      // models (current vol, bumped up, bumped down) via temporary Pricer
      // instances so each call actually sees its own sigma.
      BSModel m_up = m2, m_dn = m2;
      double h = 1e-4;
      m_up.sigma = vol*(1+h); m_dn.sigma = vol*(1-h);
      Pricer pricer_cur(m2), pricer_up(m_up), pricer_dn(m_dn);
      price = pricer_cur.black_scholes(payoff).price;
      double p_up = pricer_up.black_scholes(payoff).price;
      double p_dn = pricer_dn.black_scholes(payoff).price;
      vega = (p_up - p_dn)/(2*h*vol + 1e-12);
    }
    double diff = price - target;
    if(std::fabs(diff) < 1e-6) return vol;
    double step = diff / std::max(1e-8, vega);
    // Damping
    step = std::clamp(step, -0.5, 0.5);
    double next = std::clamp(vol - step, lo, hi);
    // Bracket tighten
    if(diff>0) hi = std::min(hi, vol);
    else       lo = std::max(lo, vol);
    vol = next;
    if(std::fabs(vol-last) < 1e-8) break;
    last = vol;
  }
  return vol;
}
