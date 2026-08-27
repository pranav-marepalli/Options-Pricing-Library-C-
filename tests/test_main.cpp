#include "../include/pricer.hpp"
#include <iostream>
#include <cmath>
#include <vector>
#include <cstdlib>

// plain CHECK() is compiled out under NDEBUG, which CI's Release build
// defines — that would silently turn every check below into a no-op, so use
// a check macro that always runs instead.
#define CHECK(cond) \
  do { \
    if(!(cond)) { \
      std::cerr << "CHECK FAILED: " #cond " at " __FILE__ ":" << __LINE__ << "\n"; \
      std::abort(); \
    } \
  } while(0)

static bool approx(double a, double b, double eps=1e-4){ return std::fabs(a-b) < eps; }

// A synthetic payoff, distinct from Call/Put, used to force implied_vol and
// black_scholes down their non-analytic fallback branches while still being
// priceable in closed form (it's a call in disguise) so we have a reference
// to compare against.
struct DisguisedCall : Payoff {
  double K;
  explicit DisguisedCall(double K):K(K){}
  double operator()(double ST) const override { return std::max(ST - K, 0.0); }
};

// Bug 1: put theta must differ from call theta and match put-call parity.
static void test_put_theta_parity(){
  BSModel m{100, 0.02, 0.0, 0.20, 1.0};
  Pricer P(m);
  Call call(100); Put put(100);
  auto cg = P.black_scholes(call);
  auto pg = P.black_scholes(put);

  CHECK(!approx(cg.theta, pg.theta, 1e-6)); // must not silently equal call theta

  double expected_put_theta = cg.theta + m.r*100.0*std::exp(-m.r*m.T) - m.q*m.S0*std::exp(-m.q*m.T);
  CHECK(approx(pg.theta, expected_put_theta, 1e-9));

  // Cross-check against a finite-difference bump on T (central difference).
  double h = 1e-5;
  BSModel m_up = m; m_up.T += h;
  BSModel m_dn = m; m_dn.T -= h;
  double p_up = Pricer(m_up).black_scholes(put).price;
  double p_dn = Pricer(m_dn).black_scholes(put).price;
  double fd_theta = -(p_up - p_dn)/(2*h); // theta is -dPrice/dT
  CHECK(approx(pg.theta, fd_theta, 1e-2));

  std::cout << "test_put_theta_parity passed.\n";
}

// Bug 2: implied_vol's fallback branch must actually vary sigma and converge.
static void test_implied_vol_fallback_converges(){
  BSModel m{100, 0.02, 0.0, 0.20, 1.0};
  DisguisedCall payoff(100);
  Pricer P(m);

  // Target price generated from a different vol than the initial guess, so a
  // no-op (vega==0) Newton step would leave vol at the guess unchanged.
  BSModel m_true = m; m_true.sigma = 0.35;
  double target = Pricer(m_true).black_scholes(payoff).price;

  double guess = 0.20;
  double iv = P.implied_vol(payoff, target, guess);

  CHECK(!approx(iv, guess, 1e-4)); // must have moved away from the initial guess
  double resulting_price = Pricer(BSModel{m.S0, m.r, m.q, iv, m.T}).black_scholes(payoff).price;
  CHECK(approx(resulting_price, target, 1e-3));

  std::cout << "test_implied_vol_fallback_converges passed. iv=" << iv << "\n";
}

// Bug 3: Gauss-Hermite fallback quadrature nodes must be the correct
// probabilist's-convention nodes (z = +-1) rather than the unscaled
// physicist's-convention nodes (z = +-1/sqrt(2)).
//
// This is only a 2-point approximation of a kinked payoff, so it doesn't
// track the analytic price closely for every strike (right at the money the
// two node choices happen to land on either side of the true price, so
// "closer" isn't guaranteed there). Away from the money, though, the correct
// nodes should be a clear, reproducible improvement, so we check both that
// the fix lands within a reasonable tolerance of the analytic price *and*
// that it's closer than the old, buggy nodes would have gotten.
static void test_fallback_quadrature_matches_analytic(){
  BSModel m{100, 0.02, 0.0, 0.20, 1.0};
  double K = 90.0; // in-the-money call, away from the ATM edge case
  DisguisedCall payoff(K); // routed through the fallback path (not Call/Put)
  Call call(K);

  Pricer P(m);
  double fallback_price = P.black_scholes(payoff).price;
  double analytic_price = P.black_scholes(call).price;

  // Recompute what the old, unscaled (+-1/sqrt(2)) nodes would have given,
  // using the same lognormal/quadrature formula as the pricer's fallback.
  double mu = (m.r - m.q - 0.5*m.sigma*m.sigma)*m.T;
  double sd = m.sigma*std::sqrt(m.T);
  double df = std::exp(-m.r*m.T);
  double old_z = 1.0/std::sqrt(2.0);
  double ST1 = m.S0*std::exp(mu + sd*old_z);
  double ST2 = m.S0*std::exp(mu - sd*old_z);
  double old_fallback_price = 0.5*df*(payoff(ST1) + payoff(ST2));

  double new_err = std::fabs(fallback_price - analytic_price);
  double old_err = std::fabs(old_fallback_price - analytic_price);

  CHECK(new_err < old_err);              // fix must be a genuine improvement here
  CHECK(new_err/analytic_price < 0.10);  // and land within a reasonable tolerance

  std::cout << "test_fallback_quadrature_matches_analytic passed. fallback="
            << fallback_price << " analytic=" << analytic_price
            << " (old buggy nodes would have given " << old_fallback_price << ")\n";
}

int main(){
  BSModel m{100, 0.02, 0.0, 0.20, 1.0};
  Pricer P(m);
  Call call(100); Put put(100);

  // Known BS price (approx from standard calculators)
  auto cg = P.black_scholes(call);
  std::cout << "Call price: " << cg.price << "\n";
  CHECK(std::fabs(cg.price - 8.916) < 1e-2);

  // Put-call parity
  auto pg = P.black_scholes(put);
  double parity = cg.price - pg.price - (std::exp(-m.q*m.T)*m.S0 - std::exp(-m.r*m.T)*100.0);
  CHECK(std::fabs(parity) < 1e-6);

  // IV round-trip
  double target = cg.price;
  double iv = P.implied_vol(call, target, 0.2);
  std::cout << "IV: " << iv << "\n";
  CHECK(std::fabs(iv - 0.20) < 1e-3);

  test_put_theta_parity();
  test_implied_vol_fallback_converges();
  test_fallback_quadrature_matches_analytic();

  std::cout << "All tests passed.\n";
  return 0;
}
