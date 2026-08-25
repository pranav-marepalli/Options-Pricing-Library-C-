#include "pricer.hpp"

#include "httplib.h"
#include "json.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using json = nlohmann::json;

namespace {

// Production frontend (Vercel). Also set CORS_ORIGIN on Render to this value.
constexpr const char* kProdFrontendOrigin =
    "https://options-pricing-library-c.vercel.app";

std::string normalize_origin(std::string s) {
  while (!s.empty() && s.back() == '/') s.pop_back();
  return s;
}

int port_from_env() {
  const char* p = std::getenv("PORT");
  if (!p || !*p) return 8080;
  try {
    return std::stoi(p);
  } catch (...) {
    return 8080;
  }
}

bool origin_allowed(const std::string& origin) {
  const std::string o = normalize_origin(origin);
  if (o == "http://localhost:3000") return true;
  if (o == kProdFrontendOrigin) return true;
  const char* env = std::getenv("CORS_ORIGIN");
  return env && *env && o == normalize_origin(env);
}

void set_cors(const httplib::Request& req, httplib::Response& res) {
  auto it = req.headers.find("Origin");
  if (it != req.headers.end() && origin_allowed(it->second)) {
    res.set_header("Access-Control-Allow-Origin", it->second);
    res.set_header("Vary", "Origin");
  } else if (!req.get_header_value("Origin").empty()) {
    // Unknown origin: omit ACAO (browser will block)
  } else {
    // Non-browser clients (curl, etc.)
  }
  res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

bool parse_common(const json& body, BSModel& model, double& K, bool& is_call,
                  std::string& err) {
  try {
    if (!body.contains("S0") || !body.contains("K") || !body.contains("r") ||
        !body.contains("q") || !body.contains("T") || !body.contains("type")) {
      err = "missing required fields: S0, K, r, q, T, type";
      return false;
    }
    model.S0 = body.at("S0").get<double>();
    model.r = body.at("r").get<double>();
    model.q = body.at("q").get<double>();
    model.T = body.at("T").get<double>();
    model.sigma = body.value("sigma", 0.2);
    K = body.at("K").get<double>();

    std::string type = body.at("type").get<std::string>();
    if (type == "call") {
      is_call = true;
    } else if (type == "put") {
      is_call = false;
    } else {
      err = "type must be \"call\" or \"put\"";
      return false;
    }

    if (model.S0 <= 0 || K <= 0 || model.T < 0) {
      err = "S0 and K must be positive; T must be non-negative";
      return false;
    }
    return true;
  } catch (const std::exception& e) {
    err = std::string("invalid JSON fields: ") + e.what();
    return false;
  }
}

PriceGreeks price_greeks(const BSModel& model, double K, bool is_call) {
  Pricer pricer(model);
  if (is_call) return pricer.black_scholes(Call(K));
  return pricer.black_scholes(Put(K));
}

json greeks_json(const PriceGreeks& g) {
  return json{
      {"price", g.price},
      {"delta", g.delta},
      {"gamma", g.gamma},
      {"vega", g.vega},
      {"theta", g.theta},
  };
}

// Sweep bounds relative to a base parameter value.
void axis_range(const std::string& axis, double base, double& lo, double& hi) {
  if (axis == "S0" || axis == "K") {
    lo = std::max(1.0, base * 0.4);
    hi = std::max(lo + 1.0, base * 1.6);
  } else if (axis == "sigma") {
    lo = 0.01;
    hi = std::min(1.0, std::max(0.4, base * 2.5));
  } else if (axis == "T") {
    lo = 0.01;
    hi = std::min(5.0, std::max(1.0, base * 2.0));
  } else if (axis == "r") {
    lo = 0.0;
    hi = std::min(0.30, std::max(0.15, base + 0.10));
  } else if (axis == "q") {
    lo = 0.0;
    hi = std::min(0.20, std::max(0.10, base + 0.08));
  } else {
    lo = base * 0.5;
    hi = base * 1.5;
  }
}

int64_t now_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// Stable fingerprint for hashmap keys (quantized doubles).
std::string price_fingerprint(const BSModel& m, double K, bool is_call) {
  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "S0=%.8g|K=%.8g|r=%.8g|q=%.8g|sigma=%.8g|T=%.8g|type=%s", m.S0, K,
                m.r, m.q, m.sigma, m.T, is_call ? "call" : "put");
  return std::string(buf);
}

std::string curve_fingerprint(const BSModel& m, double K, bool is_call,
                              const std::string& axis, int points, double lo,
                              double hi) {
  char buf[320];
  std::snprintf(buf, sizeof(buf),
                "S0=%.8g|K=%.8g|r=%.8g|q=%.8g|sigma=%.8g|T=%.8g|type=%s|"
                "axis=%s|n=%d|lo=%.8g|hi=%.8g",
                m.S0, K, m.r, m.q, m.sigma, m.T, is_call ? "call" : "put",
                axis.c_str(), points, lo, hi);
  return std::string(buf);
}

struct CurvePointPod {
  double x, price, delta, gamma, vega, theta;
};

json series_to_json(const std::vector<CurvePointPod>& pts) {
  json series = json::array();
  series.get_ref<json::array_t&>().reserve(pts.size());
  for (const auto& p : pts) {
    series.push_back(json{
        {"x", p.x},
        {"price", p.price},
        {"delta", p.delta},
        {"gamma", p.gamma},
        {"vega", p.vega},
        {"theta", p.theta},
    });
  }
  return series;
}

// Process-local shared store: O(1) param → PriceGreeks lookup, plus heaps for
// latest-by-time and top-by-|price| retrieval without scanning the full map.
struct PriceRecord {
  std::string key;
  BSModel model{};
  double K = 0;
  bool is_call = true;
  PriceGreeks greeks{};
  int64_t ts_ms = 0;
};

struct RecentHeapItem {
  int64_t ts_ms;
  std::string key;
  bool operator<(const RecentHeapItem& o) const { return ts_ms < o.ts_ms; }
};

struct TopPriceHeapItem {
  double abs_price;
  int64_t ts_ms;
  std::string key;
  bool operator<(const TopPriceHeapItem& o) const {
    return abs_price < o.abs_price;
  }
};

class SharedPriceStore {
 public:
  static constexpr size_t kMaxEntries = 256;

  void upsert(const BSModel& model, double K, bool is_call, const PriceGreeks& g) {
    const std::string key = price_fingerprint(model, K, is_call);
    const int64_t ts = now_ms();

    std::lock_guard<std::mutex> lock(mu_);
    auto it = by_key_.find(key);
    if (it == by_key_.end()) {
      if (by_key_.size() >= kMaxEntries) evict_oldest_unlocked();
      PriceRecord rec;
      rec.key = key;
      rec.model = model;
      rec.K = K;
      rec.is_call = is_call;
      rec.greeks = g;
      rec.ts_ms = ts;
      by_key_.emplace(key, std::move(rec));
    } else {
      it->second.model = model;
      it->second.K = K;
      it->second.is_call = is_call;
      it->second.greeks = g;
      it->second.ts_ms = ts;
    }

    recent_heap_.push(RecentHeapItem{ts, key});
    // Stamp ts_ms so re-upserts of the same |price| invalidate prior heap
    // entries (otherwise /v1/store/top can emit duplicate keys).
    top_heap_.push(TopPriceHeapItem{std::fabs(g.price), ts, key});
    maybe_compact_heaps_unlocked();
    ++version_;
  }

  json latest_json() {
    std::lock_guard<std::mutex> lock(mu_);
    prune_recent_unlocked();
    if (recent_heap_.empty()) return json{{"entry", nullptr}};
    const auto& top = recent_heap_.top();
    auto it = by_key_.find(top.key);
    if (it == by_key_.end()) return json{{"entry", nullptr}};
    return json{{"entry", record_json(it->second)}};
  }

  json recent_json(size_t limit) {
    if (limit == 0) limit = 10;
    if (limit > 50) limit = 50;

    std::lock_guard<std::mutex> lock(mu_);
    prune_recent_unlocked();

    std::vector<RecentHeapItem> popped;
    popped.reserve(limit);
    json arr = json::array();

    while (!recent_heap_.empty() && arr.size() < limit) {
      auto item = recent_heap_.top();
      recent_heap_.pop();
      auto it = by_key_.find(item.key);
      if (it == by_key_.end() || it->second.ts_ms != item.ts_ms) continue;
      arr.push_back(record_json(it->second));
      popped.push_back(std::move(item));
    }
    for (auto& p : popped) recent_heap_.push(std::move(p));

    return json{{"count", arr.size()}, {"entries", arr}};
  }

  json top_json(size_t limit) {
    if (limit == 0) limit = 10;
    if (limit > 50) limit = 50;

    std::lock_guard<std::mutex> lock(mu_);
    prune_top_unlocked();

    std::vector<TopPriceHeapItem> popped;
    popped.reserve(limit);
    json arr = json::array();

    while (!top_heap_.empty() && arr.size() < limit) {
      auto item = top_heap_.top();
      top_heap_.pop();
      auto it = by_key_.find(item.key);
      if (it == by_key_.end() || it->second.ts_ms != item.ts_ms ||
          std::fabs(it->second.greeks.price) != item.abs_price) {
        continue;
      }
      arr.push_back(record_json(it->second));
      popped.push_back(std::move(item));
    }
    for (auto& p : popped) top_heap_.push(std::move(p));

    return json{{"count", arr.size()}, {"entries", arr}};
  }

  json stats_json() {
    std::lock_guard<std::mutex> lock(mu_);
    return json{
        {"size", by_key_.size()},
        {"max_entries", kMaxEntries},
        {"version", version_},
    };
  }

 private:
  static json record_json(const PriceRecord& r) {
    return json{
        {"key", r.key},
        {"ts_ms", r.ts_ms},
        {"S0", r.model.S0},
        {"K", r.K},
        {"r", r.model.r},
        {"q", r.model.q},
        {"sigma", r.model.sigma},
        {"T", r.model.T},
        {"type", r.is_call ? "call" : "put"},
        {"price", r.greeks.price},
        {"delta", r.greeks.delta},
        {"gamma", r.greeks.gamma},
        {"vega", r.greeks.vega},
        {"theta", r.greeks.theta},
    };
  }

  void prune_recent_unlocked() {
    while (!recent_heap_.empty()) {
      const auto& top = recent_heap_.top();
      auto it = by_key_.find(top.key);
      if (it != by_key_.end() && it->second.ts_ms == top.ts_ms) break;
      recent_heap_.pop();
    }
  }

  void prune_top_unlocked() {
    while (!top_heap_.empty()) {
      const auto& top = top_heap_.top();
      auto it = by_key_.find(top.key);
      if (it != by_key_.end() && it->second.ts_ms == top.ts_ms &&
          std::fabs(it->second.greeks.price) == top.abs_price) {
        break;
      }
      top_heap_.pop();
    }
  }

  void evict_oldest_unlocked() {
    if (by_key_.empty()) return;
    auto oldest = by_key_.begin();
    for (auto it = by_key_.begin(); it != by_key_.end(); ++it) {
      if (it->second.ts_ms < oldest->second.ts_ms) oldest = it;
    }
    by_key_.erase(oldest);
  }

  // Rebuild heaps from the map when lazy-invalidated entries accumulate.
  void maybe_compact_heaps_unlocked() {
    const size_t soft = kMaxEntries * 4;
    if (recent_heap_.size() <= soft && top_heap_.size() <= soft) return;

    decltype(recent_heap_) fresh_recent;
    decltype(top_heap_) fresh_top;
    for (const auto& kv : by_key_) {
      fresh_recent.push(RecentHeapItem{kv.second.ts_ms, kv.first});
      fresh_top.push(TopPriceHeapItem{std::fabs(kv.second.greeks.price),
                                      kv.second.ts_ms, kv.first});
    }
    recent_heap_.swap(fresh_recent);
    top_heap_.swap(fresh_top);
  }

  std::mutex mu_;
  std::unordered_map<std::string, PriceRecord> by_key_;
  std::priority_queue<RecentHeapItem> recent_heap_;
  std::priority_queue<TopPriceHeapItem> top_heap_;
  uint64_t version_ = 0;
};

// Curve response cache: identical (params, axis, points, lo, hi) → cached JSON.
struct CachedCurve {
  json payload;
  int64_t ts_ms = 0;
};

class CurveCache {
 public:
  static constexpr size_t kMaxEntries = 64;
  static constexpr int64_t kTtlMs = 30'000;

  bool try_get(const std::string& key, json& out) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = map_.find(key);
    if (it == map_.end()) {
      ++misses_;
      return false;
    }
    if (now_ms() - it->second.ts_ms > kTtlMs) {
      map_.erase(it);
      ++misses_;
      return false;
    }
    out = it->second.payload;
    ++hits_;
    return true;
  }

  void put(const std::string& key, json payload) {
    std::lock_guard<std::mutex> lock(mu_);
    if (map_.size() >= kMaxEntries && map_.find(key) == map_.end()) {
      // Evict oldest
      auto oldest = map_.begin();
      for (auto it = map_.begin(); it != map_.end(); ++it) {
        if (it->second.ts_ms < oldest->second.ts_ms) oldest = it;
      }
      map_.erase(oldest);
    }
    map_[key] = CachedCurve{std::move(payload), now_ms()};
  }

  json stats_json() {
    std::lock_guard<std::mutex> lock(mu_);
    return json{
        {"size", map_.size()},
        {"max_entries", kMaxEntries},
        {"ttl_ms", kTtlMs},
        {"hits", hits_},
        {"misses", misses_},
    };
  }

 private:
  std::mutex mu_;
  std::unordered_map<std::string, CachedCurve> map_;
  uint64_t hits_ = 0;
  uint64_t misses_ = 0;
};

SharedPriceStore g_price_store;
CurveCache g_curve_cache;

}  // namespace

int main() {
  const int port = port_from_env();
  const char* cors_env = std::getenv("CORS_ORIGIN");

  httplib::Server svr;

  svr.Options(R"(.*)", [&](const httplib::Request& req, httplib::Response& res) {
    set_cors(req, res);
    res.status = 204;
  });

  svr.Get("/health", [&](const httplib::Request& req, httplib::Response& res) {
    set_cors(req, res);
    res.set_content(R"({"status":"ok"})", "application/json");
  });

  // Shared store inspection (updated by POST /v1/price).
  svr.Get("/v1/store/latest", [&](const httplib::Request& req, httplib::Response& res) {
    set_cors(req, res);
    res.set_content(g_price_store.latest_json().dump(), "application/json");
  });

  svr.Get("/v1/store/recent", [&](const httplib::Request& req, httplib::Response& res) {
    set_cors(req, res);
    size_t limit = 10;
    if (req.has_param("limit")) {
      try {
        limit = static_cast<size_t>(std::stoul(req.get_param_value("limit")));
      } catch (...) {
        limit = 10;
      }
    }
    res.set_content(g_price_store.recent_json(limit).dump(), "application/json");
  });

  svr.Get("/v1/store/top", [&](const httplib::Request& req, httplib::Response& res) {
    set_cors(req, res);
    size_t limit = 10;
    if (req.has_param("limit")) {
      try {
        limit = static_cast<size_t>(std::stoul(req.get_param_value("limit")));
      } catch (...) {
        limit = 10;
      }
    }
    res.set_content(g_price_store.top_json(limit).dump(), "application/json");
  });

  svr.Get("/v1/store/stats", [&](const httplib::Request& req, httplib::Response& res) {
    set_cors(req, res);
    json out = {
        {"price_store", g_price_store.stats_json()},
        {"curve_cache", g_curve_cache.stats_json()},
    };
    res.set_content(out.dump(), "application/json");
  });

  svr.Post("/v1/price", [&](const httplib::Request& req, httplib::Response& res) {
    set_cors(req, res);
    json body;
    try {
      body = json::parse(req.body);
    } catch (...) {
      res.status = 400;
      res.set_content(R"({"error":"invalid JSON"})", "application/json");
      return;
    }

    BSModel model{};
    double K = 0;
    bool is_call = true;
    std::string err;
    if (!parse_common(body, model, K, is_call, err)) {
      res.status = 400;
      res.set_content(json{{"error", err}}.dump(), "application/json");
      return;
    }
    if (!body.contains("sigma")) {
      res.status = 400;
      res.set_content(R"({"error":"missing required field: sigma"})", "application/json");
      return;
    }
    if (model.sigma <= 0) {
      res.status = 400;
      res.set_content(R"({"error":"sigma must be positive"})", "application/json");
      return;
    }

    Pricer pricer(model);
    PriceGreeks g;
    if (is_call) {
      g = pricer.black_scholes(Call(K));
    } else {
      g = pricer.black_scholes(Put(K));
    }

    // Upsert into process-local hashmap (+ heaps for latest/top retrieval).
    g_price_store.upsert(model, K, is_call, g);

    json out = {
        {"price", g.price},
        {"delta", g.delta},
        {"gamma", g.gamma},
        {"vega", g.vega},
        {"theta", g.theta},
    };
    res.set_content(out.dump(), "application/json");
  });

  svr.Post("/v1/implied-vol", [&](const httplib::Request& req, httplib::Response& res) {
    set_cors(req, res);
    json body;
    try {
      body = json::parse(req.body);
    } catch (...) {
      res.status = 400;
      res.set_content(R"({"error":"invalid JSON"})", "application/json");
      return;
    }

    BSModel model{};
    double K = 0;
    bool is_call = true;
    std::string err;
    if (!parse_common(body, model, K, is_call, err)) {
      res.status = 400;
      res.set_content(json{{"error", err}}.dump(), "application/json");
      return;
    }
    if (!body.contains("target")) {
      res.status = 400;
      res.set_content(R"({"error":"missing required field: target"})", "application/json");
      return;
    }

    double target = 0;
    try {
      target = body.at("target").get<double>();
    } catch (const std::exception& e) {
      res.status = 400;
      res.set_content(json{{"error", std::string("invalid target: ") + e.what()}}.dump(),
                      "application/json");
      return;
    }

    double guess = body.value("sigma", 0.2);
    model.sigma = guess;

    Pricer pricer(model);
    double iv;
    if (is_call) {
      iv = pricer.implied_vol(Call(K), target, guess);
    } else {
      iv = pricer.implied_vol(Put(K), target, guess);
    }

    res.set_content(json{{"implied_vol", iv}}.dump(), "application/json");
  });

  // Builds an ordered vector of (x, price, greeks) along one axis for charts.
  // POD vector → single JSON serialize; identical requests hit CurveCache.
  svr.Post("/v1/curve", [&](const httplib::Request& req, httplib::Response& res) {
    set_cors(req, res);
    json body;
    try {
      body = json::parse(req.body);
    } catch (...) {
      res.status = 400;
      res.set_content(R"({"error":"invalid JSON"})", "application/json");
      return;
    }

    BSModel model{};
    double K = 0;
    bool is_call = true;
    std::string err;
    if (!parse_common(body, model, K, is_call, err)) {
      res.status = 400;
      res.set_content(json{{"error", err}}.dump(), "application/json");
      return;
    }
    if (!body.contains("sigma")) {
      res.status = 400;
      res.set_content(R"({"error":"missing required field: sigma"})", "application/json");
      return;
    }
    if (model.sigma <= 0) {
      res.status = 400;
      res.set_content(R"({"error":"sigma must be positive"})", "application/json");
      return;
    }

    std::string axis = body.value("axis", "S0");
    const std::vector<std::string> allowed = {"S0", "K", "r", "q", "sigma", "T"};
    if (std::find(allowed.begin(), allowed.end(), axis) == allowed.end()) {
      res.status = 400;
      res.set_content(R"({"error":"axis must be one of S0, K, r, q, sigma, T"})",
                      "application/json");
      return;
    }

    int n = body.value("points", 41);
    if (n < 5) n = 5;
    if (n > 201) n = 201;

    double base = model.S0;
    if (axis == "K")
      base = K;
    else if (axis == "r")
      base = model.r;
    else if (axis == "q")
      base = model.q;
    else if (axis == "sigma")
      base = model.sigma;
    else if (axis == "T")
      base = model.T;

    double lo = 0, hi = 0;
    axis_range(axis, base, lo, hi);

    const std::string cache_key =
        curve_fingerprint(model, K, is_call, axis, n, lo, hi);
    json cached;
    if (g_curve_cache.try_get(cache_key, cached)) {
      res.set_content(cached.dump(), "application/json");
      return;
    }

    // Grow POD series first (avoid per-point json object churn).
    std::vector<CurvePointPod> series;
    series.reserve(static_cast<size_t>(n));

    PriceGreeks spot{};
    bool have_spot = false;
    const double spot_x = model.S0;

    for (int i = 0; i < n; ++i) {
      const double t = (n == 1) ? 0.0 : static_cast<double>(i) / (n - 1);
      const double x = lo + t * (hi - lo);

      BSModel m = model;
      double k = K;
      if (axis == "S0")
        m.S0 = x;
      else if (axis == "K")
        k = x;
      else if (axis == "r")
        m.r = x;
      else if (axis == "q")
        m.q = x;
      else if (axis == "sigma")
        m.sigma = std::max(1e-6, x);
      else if (axis == "T")
        m.T = std::max(0.0, x);

      if (m.S0 <= 0 || k <= 0) continue;

      PriceGreeks g = price_greeks(m, k, is_call);
      series.push_back(CurvePointPod{x, g.price, g.delta, g.gamma, g.vega, g.theta});

      // Reuse series sample when S0 axis lands exactly on the spot.
      if (axis == "S0" && !have_spot && std::fabs(x - spot_x) <= 1e-12 * std::max(1.0, spot_x)) {
        spot = g;
        have_spot = true;
      }
    }

    if (!have_spot) spot = price_greeks(model, K, is_call);

    json out = {
        {"axis", axis},
        {"lo", lo},
        {"hi", hi},
        {"spot", greeks_json(spot)},
        {"series", series_to_json(series)},
    };
    g_curve_cache.put(cache_key, out);
    res.set_content(out.dump(), "application/json");
  });

  std::cout << "options_api listening on 0.0.0.0:" << port
            << " (CORS: http://localhost:3000 + " << kProdFrontendOrigin
            << (cors_env && *cors_env ? std::string(" + ") + cors_env : "")
            << ")" << std::endl;
  if (!svr.listen("0.0.0.0", port)) {
    std::cerr << "failed to bind port " << port << "\n";
    return 1;
  }
  return 0;
}
