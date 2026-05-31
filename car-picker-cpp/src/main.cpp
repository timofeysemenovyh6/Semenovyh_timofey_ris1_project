#include "client_tools.h"
#include "db.h"
#include "html.h"
#include "http.h"
#include "reco.h"
#include "session.h"

#include <cctype>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <numeric>
#include <optional>
#include <random>
#include <sstream>
#include <vector>

static std::optional<int> parse_port(const char* s) {
  if (!s) return std::nullopt;
  try {
    int p = std::stoi(std::string(s));
    if (p < 1 || p > 65535) return std::nullopt;
    return p;
  } catch (...) {
    return std::nullopt;
  }
}

static std::optional<int64_t> q_int64(const std::string& q, const std::string& key) {
  size_t start = 0;
  while (start < q.size()) {
    size_t amp = q.find('&', start);
    if (amp == std::string::npos) amp = q.size();
    std::string pair = q.substr(start, amp - start);
    size_t eq = pair.find('=');
    std::string k = (eq == std::string::npos) ? pair : pair.substr(0, eq);
    std::string v = (eq == std::string::npos) ? "" : pair.substr(eq + 1);
    if (http::url_decode(k) == key) {
      try {
        return std::stoll(http::url_decode(v));
      } catch (...) {
        return std::nullopt;
      }
    }
    start = amp + 1;
  }
  return std::nullopt;
}

static std::vector<int64_t> parse_query_ids(const std::string& q) {
  std::vector<int64_t> out;
  size_t start = 0;
  while (start < q.size()) {
    size_t amp = q.find('&', start);
    if (amp == std::string::npos) amp = q.size();
    std::string pair = q.substr(start, amp - start);
    size_t eq = pair.find('=');
    std::string k = (eq == std::string::npos) ? pair : pair.substr(0, eq);
    std::string v = (eq == std::string::npos) ? "" : pair.substr(eq + 1);
    if (http::url_decode(k) != "ids") {
      start = amp + 1;
      continue;
    }
    std::string vs = http::url_decode(v);
    size_t p = 0;
    while (p < vs.size()) {
      size_t comma = vs.find(',', p);
      std::string piece = vs.substr(p, comma == std::string::npos ? std::string::npos : comma - p);
      while (!piece.empty() && std::isspace(static_cast<unsigned char>(piece.front()))) piece.erase(piece.begin());
      while (!piece.empty() && std::isspace(static_cast<unsigned char>(piece.back()))) piece.pop_back();
      if (!piece.empty()) {
        try {
          out.push_back(std::stoll(piece));
        } catch (...) {
        }
      }
      if (comma == std::string::npos) break;
      p = comma + 1;
    }
    break;
  }
  if (out.size() > 3) out.resize(3);
  return out;
}

static std::string sanitize_fav_next(const std::string& raw) {
  if (raw == "/wizard" || raw == "/chat" || raw == "/favorites" || raw == "/compare" || raw == "/extras") return raw;
  return "/wizard";
}

static void extras_ensure_blind(SessionState& s, const std::vector<CarRow>& all, bool force_new) {
  if (!force_new && s.blindtest_ids.size() == 3) return;
  s.blindtest_ids.clear();
  s.blindtest_pick = -1;
  if (all.size() < 3) return;
  std::vector<size_t> idx(all.size());
  std::iota(idx.begin(), idx.end(), 0);
  std::mt19937 rng{std::random_device{}()};
  std::shuffle(idx.begin(), idx.end(), rng);
  for (int k = 0; k < 3; ++k) s.blindtest_ids.push_back(all[idx[static_cast<size_t>(k)]].id);
}

static std::string append_chat_insights(const std::string& answer, const std::string& msg, const std::vector<ScoredCar>& recs,
                                        const Preferences& prefs) {
  if (msg.empty() || recs.empty()) return answer;
  const std::string add = chat_insight_addon_ru(msg, recs[0].car, prefs);
  if (add.empty()) return answer;
  return answer + add;
}

static std::string build_sensitivity_fragment(const Preferences& prefs, const std::vector<CarRow>& cars) {
  const auto ctx = recommend(cars, prefs, 7);
  return html::build_sensitivity_export_block(prefs, cars, ctx);
}

static bool parse_my_car_form(const std::map<std::string, std::string>& form, MyCarProfile& out, std::string& err) {
  err.clear();
  auto it_t = form.find("my_trunk");
  auto it_f = form.find("my_fuel");
  auto it_p = form.find("my_power");
  if (it_t == form.end() || it_p == form.end()) {
    err = "Укажите багажник и мощность.";
    return false;
  }
  try {
    out.trunk_l = std::stoi(it_t->second);
    out.power_hp = std::stoi(it_p->second);
  } catch (...) {
    err = "Багажник и мощность должны быть целыми числами.";
    return false;
  }
  std::string fs = (it_f == form.end()) ? "8" : it_f->second;
  for (char& c : fs)
    if (c == ',') c = '.';
  try {
    out.fuel_l100 = std::stod(fs);
  } catch (...) {
    err = "Расход: число (л/100км).";
    return false;
  }
  auto it_ev = form.find("my_ev");
  out.is_electric = it_ev != form.end() && (it_ev->second == "1" || it_ev->second == "on");
  if (out.is_electric) out.fuel_l100 = 0.0;
  else if (out.fuel_l100 < 3.0 || out.fuel_l100 > 28.0) {
    err = "Для ДВС расход от 3 до 28 л/100км, либо отметьте «Электромобиль».";
    return false;
  }
  auto it_pr = form.find("my_price");
  out.approx_price_rub = 0;
  if (it_pr != form.end() && !it_pr->second.empty()) {
    try {
      out.approx_price_rub = std::stoi(it_pr->second);
    } catch (...) {
      out.approx_price_rub = 0;
    }
  }
  if (out.trunk_l < 80 || out.trunk_l > 1200) {
    err = "Багажник: от 80 до 1200 л.";
    return false;
  }
  if (out.power_hp < 40 || out.power_hp > 700) {
    err = "Мощность: от 40 до 700 л.с.";
    return false;
  }
  if (out.approx_price_rub < 0) out.approx_price_rub = 0;
  out.configured = true;
  return true;
}

static bool chat_wants_reset(const std::string& msg) {
  if (msg.find("очист") != std::string::npos && msg.find("фильтр") != std::string::npos) return true;
  if (msg.find("сброс") == std::string::npos && msg.find("Сброс") == std::string::npos) return false;
  return msg.find("фильтр") != std::string::npos || msg.find("критер") != std::string::npos ||
         msg.find("всё") != std::string::npos || msg.find("все ") != std::string::npos;
}

static void session_store_top_rec_ids(SessionState& s, const std::vector<ScoredCar>& recs, size_t maxn = 3) {
  s.last_top_rec_ids.clear();
  for (size_t i = 0; i < recs.size() && i < maxn; ++i) s.last_top_rec_ids.push_back(recs[i].car.id);
}

static CarRow car_from_form(const std::map<std::string, std::string>& f, std::string& err) {
  auto get = [&](const std::string& k) -> std::string {
    auto it = f.find(k);
    return it == f.end() ? "" : it->second;
  };
  auto geti = [&](const std::string& k) -> int {
    try {
      return std::stoi(get(k));
    } catch (...) {
      return 0;
    }
  };
  auto getd = [&](const std::string& k) -> double {
    try {
      std::string s = get(k);
      for (char& c : s)
        if (c == ',') c = '.';
      return std::stod(s);
    } catch (...) {
      return 0.0;
    }
  };

  CarRow c;
  c.name = get("name");
  c.brand = get("brand");
  c.price = geti("price");
  c.fuel_consumption = getd("fuel_consumption");
  c.body_type = get("body_type");
  c.gearbox = get("gearbox");
  c.drive = get("drive");
  c.seats = geti("seats");
  c.trunk = geti("trunk");
  c.power = geti("power");
  c.max_speed_kmh = geti("max_speed");
  c.segment = get("segment");
  if (c.segment.empty()) c.segment = "mass";
  {
    auto it = f.find("is_electric");
    c.is_electric = it != f.end() && (it->second == "1" || it->second == "on");
  }
  if (c.brand.empty() && !c.name.empty()) {
    size_t sp = c.name.find(' ');
    c.brand = (sp == std::string::npos) ? c.name : c.name.substr(0, sp);
  }
  if (c.is_electric) c.fuel_consumption = 0.0;
  if (c.max_speed_kmh <= 0)
    c.max_speed_kmh = default_max_speed_kmh(c.power, c.is_electric, c.body_type);

  if (c.name.empty()) err = "Название обязательно.";
  else if (c.price <= 0) err = "Цена должна быть больше 0.";
  else if (c.body_type.empty() || c.gearbox.empty() || c.drive.empty()) err = "Кузов/КПП/Привод обязательны.";
  else if (c.seats <= 0 || c.trunk <= 0 || c.power <= 0) err = "Места/багажник/мощность должны быть больше 0.";
  else if (c.max_speed_kmh < 80 || c.max_speed_kmh > 360)
    err = "Макс. скорость: от 80 до 360 км/ч (или 0 — подставим по мощности и кузову).";
  else if (c.segment != "economy" && c.segment != "mass" && c.segment != "premium")
    err = "Сегмент: economy, mass или premium.";
  return c;
}

static http::Response redirect_to(const std::string& location) {
  http::Response r;
  r.status = 302;
  r.content_type = "text/plain; charset=utf-8";
  r.headers["Location"] = location;
  r.body = "Redirecting...";
  return r;
}

static std::string json_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 16);
  for (unsigned char c : s) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", (unsigned)c);
          out += buf;
        } else {
          out.push_back((char)c);
        }
    }
  }
  return out;
}

static std::string json_str(const std::string& s) { return "\"" + json_escape(s) + "\""; }

static std::string json_opt_int(const std::optional<int>& v) { return v ? std::to_string(*v) : "null"; }
static std::string json_opt_double(const std::optional<double>& v) {
  if (!v) return "null";
  std::ostringstream o;
  o << *v;
  return o.str();
}
static std::string json_opt_str(const std::optional<std::string>& v) { return v ? json_str(*v) : "null"; }
static std::string json_opt_bool(const std::optional<bool>& v) {
  if (!v.has_value()) return "null";
  return *v ? "true" : "false";
}

/// Объект машины в том же формате, что элементы массива `recs` в `/api/chat` и `/api/wizard`.
/// При переданных prefs добавляются title и avito_url (поиск на Авито, как в HTML car_title_link).
static std::string json_car_rec_object(const CarRow& c, double score, const std::string& explain_ru,
                                       const Preferences* prefs = nullptr) {
  std::ostringstream o;
  o << "{"
    << "\"id\":" << c.id << ","
    << "\"name\":" << json_str(c.name) << ","
    << "\"brand\":" << json_str(c.brand) << ","
    << "\"price\":" << c.price << ","
    << "\"fuel\":" << c.fuel_consumption << ","
    << "\"body_type\":" << json_str(c.body_type) << ","
    << "\"gearbox\":" << json_str(c.gearbox) << ","
    << "\"drive\":" << json_str(c.drive) << ","
    << "\"seats\":" << c.seats << ","
    << "\"trunk\":" << c.trunk << ","
    << "\"power\":" << c.power << ","
    << "\"max_speed\":" << c.max_speed_kmh << ","
    << "\"is_electric\":" << (c.is_electric ? "true" : "false") << ","
    << "\"segment\":" << json_str(c.segment) << ","
    << "\"score\":" << score << ","
    << "\"tco_year_rub\":" << estimated_annual_tco_rub(c) << ","
    << "\"explain_ru\":" << json_str(explain_ru);
  if (prefs) {
    o << ",\"title\":" << json_str(car_display_title(c))
      << ",\"avito_url\":" << json_str(avito_search_url(c, *prefs));
  }
  o << "}";
  return o.str();
}

static void json_stream_my_car_profile(std::ostringstream& o, const MyCarProfile& mc) {
  o << "\"my_car\":{"
    << "\"configured\":" << (mc.configured ? "true" : "false") << ","
    << "\"trunk_l\":" << mc.trunk_l << ","
    << "\"fuel_l100\":" << mc.fuel_l100 << ","
    << "\"power_hp\":" << mc.power_hp << ","
    << "\"is_electric\":" << (mc.is_electric ? "true" : "false") << ","
    << "\"approx_price_rub\":" << mc.approx_price_rub << "}";
}

static std::vector<ScoredCar> session_top_scored(Db& db, SessionState& session, int k = 3) {
  std::vector<ScoredCar> out;
  for (int64_t id : session.last_top_rec_ids) {
    auto c = db.get_car(id);
    if (!c) continue;
    ScoredCar sc;
    sc.car = *c;
    sc.score = score_soft(*c, session.prefs);
    sc.explain_ru = explain_match_ru(*c, session.prefs);
    out.push_back(sc);
    if ((int)out.size() >= k) break;
  }
  if (!out.empty()) return out;
  auto cars = db.list_cars();
  return recommend(cars, session.prefs, k);
}

static void json_stream_my_car_vs_top(std::ostringstream& o, const MyCarProfile& mc, const std::vector<ScoredCar>& top_recs) {
  o << ",\"my_car_has_top\":" << (!top_recs.empty() ? "true" : "false");
  o << ",\"my_car_rows\":[";
  if (mc.configured && !top_recs.empty()) {
    const CarRow my = synthetic_my_car_row(mc);
    const int my_tco = estimated_annual_tco_rub(my, 15000);
    const int my_mo = estimate_life_with_car_5y(my, 15000).monthly_equiv_rub;
    const size_t n = std::min<size_t>(top_recs.size(), 3u);
    for (size_t i = 0; i < n; ++i) {
      if (i) o << ',';
      const auto& c = top_recs[i].car;
      const int ct = estimated_annual_tco_rub(c, 15000);
      const int cm = estimate_life_with_car_5y(c, 15000).monthly_equiv_rub;
      double fuel_delta = 0.0;
      std::string fuel_delta_kind = "neu";
      if (my.is_electric && !c.is_electric)
        fuel_delta_kind = "neu";
      else if (!my.is_electric && c.is_electric)
        fuel_delta_kind = "neu";
      else if (my.is_electric && c.is_electric) {
        fuel_delta = 0.0;
        fuel_delta_kind = "neu";
      } else {
        fuel_delta = c.fuel_consumption - my.fuel_consumption;
        if (fuel_delta < -1e-6)
          fuel_delta_kind = "good";
        else if (fuel_delta > 1e-6)
          fuel_delta_kind = "bad";
        else
          fuel_delta_kind = "neu";
      }
      o << "{"
        << "\"title\":" << json_str(car_display_title(c)) << ","
        << "\"trunk\":" << c.trunk << ","
        << "\"trunk_delta\":" << (c.trunk - my.trunk) << ","
        << "\"fuel\":" << c.fuel_consumption << ","
        << "\"fuel_is_electric\":" << (c.is_electric ? "true" : "false") << ","
        << "\"fuel_delta\":" << fuel_delta << ","
        << "\"fuel_delta_kind\":" << json_str(fuel_delta_kind) << ","
        << "\"tco_year\":" << ct << ","
        << "\"tco_delta\":" << (ct - my_tco) << ","
        << "\"monthly_5y\":" << cm << ","
        << "\"monthly_delta\":" << (cm - my_mo) << "}";
    }
    o << "],\"my_car_ref\":{"
      << "\"name\":" << json_str(my.name) << ","
      << "\"trunk\":" << my.trunk << ","
      << "\"fuel\":" << my.fuel_consumption << ","
      << "\"is_electric\":" << (my.is_electric ? "true" : "false") << ","
      << "\"tco_year\":" << my_tco << ","
      << "\"monthly_5y\":" << my_mo << "}";
  } else {
    o << "],\"my_car_ref\":null";
  }
}

static std::string json_my_car_payload(SessionState& session, Db& db) {
  auto top = session_top_scored(db, session, 3);
  std::ostringstream o;
  o << "{";
  json_stream_my_car_profile(o, session.my_car);
  json_stream_my_car_vs_top(o, session.my_car, top);
  o << "}";
  return o.str();
}

static void json_stream_favorite_ids(std::ostringstream& o, const std::vector<int64_t>& ids) {
  o << "\"favorite_ids\":[";
  for (size_t i = 0; i < ids.size(); ++i) {
    if (i) o << ',';
    o << ids[i];
  }
  o << "]";
}

static void json_stream_str_vec(std::ostringstream& o, const char* key, const std::vector<std::string>& v) {
  o << "\"" << key << "\":[";
  for (size_t i = 0; i < v.size(); ++i) {
    if (i) o << ',';
    o << json_str(v[i]);
  }
  o << "]";
}

static void json_stream_top3_insights(std::ostringstream& o, const Preferences& prefs, const std::vector<ScoredCar>& recs) {
  const size_t n = std::min<size_t>(recs.size(), 3);
  o << "\"top3_insights\":[";
  for (size_t i = 0; i < n; ++i) {
    if (i) o << ',';
    const auto& sc = recs[i];
    const auto& c = sc.car;
    const auto ex = explainable_match(c, prefs);
    const auto tco = tco_breakdown(c, 15000);
    const auto life = estimate_life_with_car_5y(c, 15000);
    o << "{"
      << "\"rank\":" << (i + 1) << ","
      << "\"id\":" << c.id << ","
      << "\"title\":" << json_str(car_display_title(c)) << ","
      << "\"score\":" << sc.score << ","
      << "\"score_pct\":" << static_cast<int>(std::lround(sc.score * 100.0)) << ","
      << "\"explain_ru\":" << json_str(sc.explain_ru) << ",";
    json_stream_str_vec(o, "strengths", ex.strengths);
    o << ",";
    json_stream_str_vec(o, "preferences", ex.preferences);
    o << ",";
    json_stream_str_vec(o, "cautions", ex.cautions);
    o << ",\"tco\":{"
      << "\"km_per_year\":" << tco.km_per_year << ","
      << "\"fuel_energy_year_rub\":" << tco.fuel_energy_year_rub << ","
      << "\"tax_insurance_year_rub\":" << tco.tax_insurance_year_rub << ","
      << "\"total_year_rub\":" << tco.total_year_rub << "},"
      << "\"life_5y\":{"
      << "\"monthly_equiv_rub\":" << life.monthly_equiv_rub << ","
      << "\"depreciation_5y_rub\":" << life.depreciation_5y_rub << ","
      << "\"total_5y_rub\":" << life.total_5y_rub << ","
      << "\"comment_ru\":" << json_str(life.comment_ru) << "}"
      << "}";
  }
  o << "]";
}

static std::string json_favorites_payload(SessionState& session, Db& db) {
  std::ostringstream o;
  o << "{";
  json_stream_favorite_ids(o, session.favorite_car_ids);
  o << ",\"cars\":[";
  bool first = true;
  for (int64_t id : session.favorite_car_ids) {
    auto c = db.get_car(id);
    if (!c) continue;
    if (!first) o << ',';
    first = false;
    o << json_car_rec_object(*c, 0.0, "");
  }
  o << "]}";
  return o.str();
}

static std::string json_client_profile(const ClientProfile& c) {
  std::ostringstream o;
  o << "{"
    << "\"name\":" << json_str(c.name) << ","
    << "\"phone\":" << json_str(c.phone) << ","
    << "\"email\":" << json_str(c.email) << ","
    << "\"notes\":" << json_str(c.notes) << ","
    << "\"manager_notes\":" << json_str(c.manager_notes) << ","
    << "\"education_mode\":" << (c.education_mode ? "true" : "false")
    << "}";
  return o.str();
}

static std::string json_prefs_object(const Preferences& p) {
  std::ostringstream o;
  o << "{"
    << "\"min_price\":" << json_opt_int(p.min_price) << ","
    << "\"max_price\":" << json_opt_int(p.max_price) << ","
    << "\"min_price_floor_opt_out\":" << json_opt_bool(p.min_price_floor_opt_out) << ","
    << "\"max_fuel\":" << json_opt_double(p.max_fuel) << ","
    << "\"fuel_ignore_cap\":" << json_opt_bool(p.fuel_ignore_cap) << ","
    << "\"min_seats\":" << json_opt_int(p.min_seats) << ","
    << "\"min_trunk\":" << json_opt_int(p.min_trunk) << ","
    << "\"min_power\":" << json_opt_int(p.min_power) << ","
    << "\"max_power\":" << json_opt_int(p.max_power) << ","
    << "\"max_power_ceiling_opt_out\":" << json_opt_bool(p.max_power_ceiling_opt_out) << ","
    << "\"body_type\":" << json_opt_str(p.body_type) << ","
    << "\"gearbox\":" << json_opt_str(p.gearbox) << ","
    << "\"drive\":" << json_opt_str(p.drive) << ","
    << "\"brand_substr\":" << json_opt_str(p.brand_substr) << ","
    << "\"segment\":" << json_opt_str(p.segment) << ","
    << "\"only_ev\":" << json_opt_bool(p.only_ev) << ","
    << "\"trip_profile\":" << json_opt_str(p.trip_profile) << ","
    << "\"drive_soft\":" << json_opt_str(p.drive_soft) << ","
    << "\"body_soft\":" << json_opt_str(p.body_soft) << ","
    << "\"gearbox_soft\":" << json_opt_str(p.gearbox_soft) << ","
    << "\"soft_min_seats\":" << json_opt_int(p.soft_min_seats) << ","
    << "\"brand_exclude_substr\":" << json_opt_str(p.brand_exclude_substr) << "}";
  return o.str();
}

static std::string json_life_obj(const LifeWithCar5y& L) {
  std::ostringstream o;
  o << "{\"monthly_equiv_rub\":" << L.monthly_equiv_rub << ",\"total_5y_rub\":" << L.total_5y_rub
    << ",\"depreciation_5y_rub\":" << L.depreciation_5y_rub << ",\"running_year_rub\":" << L.running_year_rub
    << ",\"service_year_rub\":" << L.service_year_rub << ",\"tires_year_rub\":" << L.tires_year_rub
    << ",\"comment_ru\":" << json_str(L.comment_ru) << "}";
  return o.str();
}

static std::string json_snow_obj(const SnowIndex& S) {
  std::ostringstream o;
  o << "{\"score_0_to_10\":" << S.score_0_to_10 << ",\"clearance_proxy_mm\":" << S.clearance_proxy_mm
    << ",\"tier_ru\":" << json_str(S.tier_ru) << ",\"explain_ru\":" << json_str(S.explain_ru) << "}";
  return o.str();
}

int main(int argc, char** argv) {
  http::ServerConfig cfg;
  cfg.host = "127.0.0.1";
  cfg.port = 8080;
  cfg.static_dir = "static";

  std::string db_path = "cars.db";
  if (const char* env = std::getenv("CARPICKER_DB")) db_path = env;
  if (const char* envp = std::getenv("CARPICKER_PORT")) {
    if (auto p = parse_port(envp)) cfg.port = (uint16_t)*p;
  }
  if (argc >= 2) db_path = argv[1];
  if (argc >= 3) {
    if (auto p = parse_port(argv[2])) cfg.port = (uint16_t)*p;
  }

  Db db(db_path);
  if (!db.open()) {
    std::cerr << "Не удалось открыть базу: " << db_path << "\n";
    return 1;
  }
  if (!db.init_schema() || !db.sync_catalog()) {
    std::cerr << "Не удалось инициализировать базу.\n";
    return 2;
  }

  SessionStore sessions;

  auto handler = [&](const http::Request& req, http::Response& res) -> bool {
    std::string set_cookie;
    auto& session = sessions.get_or_create(req.cookies, set_cookie);
    if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;

    if (req.path == "/api/health" && req.method == "GET") {
      res.content_type = "application/json; charset=utf-8";
      res.body = "{\"ok\":true}";
      return true;
    }

    if (req.path == "/export/report.md" && req.method == "GET") {
      auto cars = db.list_cars();
      auto recs = recommend(cars, session.prefs, 7);
      res.content_type = "text/markdown; charset=utf-8";
      res.headers["Content-Disposition"] = "attachment; filename=\"autoselect-report.md\"";
      res.body = html::markdown_export_report(session.prefs, recs);
      return true;
    }

    if (req.path == "/export/client-report.md" && req.method == "GET") {
      auto cars = db.list_cars();
      auto recs = recommend(cars, session.prefs, 3);
      res.content_type = "text/markdown; charset=utf-8";
      res.headers["Content-Disposition"] = "attachment; filename=\"client-report.md\"";
      res.body = build_client_report_markdown(session.client, session.prefs, recs);
      return true;
    }

    if (req.path == "/api/education" && req.method == "GET") {
      std::ostringstream o;
      o << "{\"education_mode\":" << (session.client.education_mode ? "true" : "false") << ",\"topics\":[";
      const auto topics = education_topics();
      for (size_t i = 0; i < topics.size(); ++i) {
        if (i) o << ',';
        o << "{\"id\":" << json_str(topics[i].id) << ",\"title\":" << json_str(topics[i].title) << ",\"teaser\":"
          << json_str(topics[i].teaser) << ",\"body_html\":" << json_str(topics[i].body_html) << "}";
      }
      o << "]}";
      res.content_type = "application/json; charset=utf-8";
      res.body = o.str();
      return true;
    }

    if (req.path == "/api/client-profile" && req.method == "GET") {
      res.content_type = "application/json; charset=utf-8";
      res.body = json_client_profile(session.client);
      return true;
    }
    if (req.path == "/api/client-profile" && req.method == "POST") {
      apply_client_profile_form(session.client, req.form);
      res.content_type = "application/json; charset=utf-8";
      res.body = std::string("{\"ok\":true,\"profile\":") + json_client_profile(session.client) + "}";
      return true;
    }

    if (req.path == "/api/client-report" && req.method == "GET") {
      auto cars = db.list_cars();
      auto recs = recommend(cars, session.prefs, 3);
      const std::string md = build_client_report_markdown(session.client, session.prefs, recs);
      res.content_type = "application/json; charset=utf-8";
      res.body = std::string("{\"markdown\":") + json_str(md) + ",\"profile\":" + json_client_profile(session.client) + "}";
      return true;
    }

    if (req.path == "/api/top3-insights" && req.method == "GET") {
      auto cars = db.list_cars();
      auto recs = recommend(cars, session.prefs, 7);
      std::ostringstream o;
      o << "{";
      json_stream_top3_insights(o, session.prefs, recs);
      o << "}";
      res.content_type = "application/json; charset=utf-8";
      res.body = o.str();
      return true;
    }

    if (req.path == "/api/chat" && req.method == "POST") {
      auto it = req.form.find("message");
      std::string msg = (it == req.form.end()) ? "" : it->second;
      if (!msg.empty()) {
        session.history.push_back({"user", msg});
        if (chat_wants_reset(msg)) {
          session.prefs = Preferences{};
          session.last_top_rec_ids.clear();
          session.my_car = MyCarProfile{};
        }
        Preferences delta = prefs_from_chat_text(msg, &session.prefs);
        merge_chat_prefs(session.prefs, delta);
      }

      auto cars = db.list_cars();
      auto eligible = recommend(cars, session.prefs, 1000);
      auto recs = recommend(cars, session.prefs, 5);
      session_store_top_rec_ids(session, recs);
      auto follow = wizard_followups(session.prefs, (int)eligible.size());

      std::string answer = "Учёл ваш ответ в критериях подбора.";
      if (eligible.empty()) {
        answer += " Сейчас в базе нет машин под эти условия — напишите, что готовы ослабить (бюджет, кузов, привод, багажник).";
      } else {
        answer += " Подходит примерно " + std::to_string((int)eligible.size()) + " вариант(ов).";
      }
      for (const auto& wq : follow) {
        answer += "\n\n";
        answer += (wq.kind == WizardQuestion::Kind::Lead ? "Наводящий вопрос: " : "Уточняющий вопрос: ");
        answer += wq.text;
      }
      answer = append_chat_insights(answer, msg, recs, session.prefs);
      if (!msg.empty()) {
        const std::string edu = education_hint_for_message_ru(msg, session.client.education_mode);
        if (!edu.empty()) answer += edu;
      }
      if (!msg.empty()) session.history.push_back({"assistant", answer});

      std::ostringstream o;
      o << "{";
      o << "\"answer\":" << json_str(answer) << ",";
      o << "\"prefs\":" << json_prefs_object(session.prefs) << ",";

      o << "\"recs\":[";
      for (size_t i = 0; i < recs.size(); ++i) {
        const auto& r = recs[i];
        const auto& c = r.car;
        if (i) o << ",";
        o << "{"
          << "\"id\":" << c.id << ","
          << "\"name\":" << json_str(c.name) << ","
          << "\"brand\":" << json_str(c.brand) << ","
          << "\"price\":" << c.price << ","
          << "\"fuel\":" << c.fuel_consumption << ","
          << "\"body_type\":" << json_str(c.body_type) << ","
          << "\"gearbox\":" << json_str(c.gearbox) << ","
          << "\"drive\":" << json_str(c.drive) << ","
          << "\"seats\":" << c.seats << ","
          << "\"trunk\":" << c.trunk << ","
          << "\"power\":" << c.power << ","
          << "\"max_speed\":" << c.max_speed_kmh << ","
          << "\"is_electric\":" << (c.is_electric ? "true" : "false") << ","
          << "\"segment\":" << json_str(c.segment) << ","
          << "\"score\":" << r.score << ","
          << "\"tco_year_rub\":" << estimated_annual_tco_rub(c) << ","
          << "\"explain_ru\":" << json_str(r.explain_ru)
          << "}";
      }
      o << "]";
      o << ",";
      json_stream_top3_insights(o, session.prefs, recs);
      o << ",";
      json_stream_favorite_ids(o, session.favorite_car_ids);
      o << ",";
      json_stream_my_car_profile(o, session.my_car);
      json_stream_my_car_vs_top(o, session.my_car, recs);
      o << "}";

      res.content_type = "application/json; charset=utf-8";
      res.body = o.str();
      return true;
    }

    if (req.path == "/api/wizard" && req.method == "POST") {
      Preferences p = prefs_from_form(req.form);
      session.prefs = p;

      auto cars = db.list_cars();
      auto eligible = recommend(cars, p, 1000);
      auto recs = recommend(cars, p, 7);
      session_store_top_rec_ids(session, recs);
      auto follow = wizard_followups(p, (int)eligible.size());

      std::ostringstream o;
      o << "{";
      o << "\"eligible_count\":" << (int)eligible.size() << ",";
      o << "\"followups\":[";
      for (size_t i = 0; i < follow.size(); ++i) {
        if (i) o << ",";
        o << "{"
          << "\"kind\":" << json_str(follow[i].kind == WizardQuestion::Kind::Lead ? "lead" : "detail") << ","
          << "\"text\":" << json_str(follow[i].text)
          << "}";
      }
      o << "],";
      o << "\"recs\":[";
      for (size_t i = 0; i < recs.size(); ++i) {
        const auto& r = recs[i];
        const auto& c = r.car;
        if (i) o << ",";
        o << "{"
          << "\"id\":" << c.id << ","
          << "\"name\":" << json_str(c.name) << ","
          << "\"brand\":" << json_str(c.brand) << ","
          << "\"price\":" << c.price << ","
          << "\"fuel\":" << c.fuel_consumption << ","
          << "\"body_type\":" << json_str(c.body_type) << ","
          << "\"gearbox\":" << json_str(c.gearbox) << ","
          << "\"drive\":" << json_str(c.drive) << ","
          << "\"seats\":" << c.seats << ","
          << "\"trunk\":" << c.trunk << ","
          << "\"power\":" << c.power << ","
          << "\"max_speed\":" << c.max_speed_kmh << ","
          << "\"is_electric\":" << (c.is_electric ? "true" : "false") << ","
          << "\"segment\":" << json_str(c.segment) << ","
          << "\"score\":" << r.score << ","
          << "\"tco_year_rub\":" << estimated_annual_tco_rub(c) << ","
          << "\"explain_ru\":" << json_str(r.explain_ru)
          << "}";
      }
      o << "]";
      o << ",";
      json_stream_top3_insights(o, p, recs);
      o << ",";
      json_stream_favorite_ids(o, session.favorite_car_ids);
      o << ",";
      json_stream_my_car_profile(o, session.my_car);
      json_stream_my_car_vs_top(o, session.my_car, recs);
      o << "}";

      res.content_type = "application/json; charset=utf-8";
      res.body = o.str();
      return true;
    }

    if (req.path == "/api/my-car" && req.method == "GET") {
      res.content_type = "application/json; charset=utf-8";
      res.body = json_my_car_payload(session, db);
      return true;
    }
    if (req.path == "/api/my-car" && req.method == "POST") {
      MyCarProfile tmp;
      std::string err;
      res.content_type = "application/json; charset=utf-8";
      if (!parse_my_car_form(req.form, tmp, err)) {
        res.body = std::string("{\"ok\":false,\"error\":") + json_str(err) + "}";
        return true;
      }
      session.my_car = tmp;
      res.body = std::string("{\"ok\":true,") + json_my_car_payload(session, db).substr(1);
      return true;
    }
    if (req.path == "/api/my-car/clear" && req.method == "POST") {
      session.my_car = MyCarProfile{};
      res.content_type = "application/json; charset=utf-8";
      res.body = json_my_car_payload(session, db);
      return true;
    }

    if (req.path == "/api/favorites" && req.method == "GET") {
      res.content_type = "application/json; charset=utf-8";
      res.body = json_favorites_payload(session, db);
      return true;
    }
    if (req.path == "/api/favorite/toggle" && req.method == "POST") {
      auto it_id = req.form.find("id");
      int64_t car_id = 0;
      try {
        car_id = (it_id == req.form.end()) ? 0 : std::stoll(it_id->second);
      } catch (...) {
        car_id = 0;
      }
      if (car_id > 0) {
        auto& v = session.favorite_car_ids;
        auto f = std::find(v.begin(), v.end(), car_id);
        if (f != v.end())
          v.erase(f);
        else if (v.size() < 80)
          v.push_back(car_id);
      }
      res.content_type = "application/json; charset=utf-8";
      res.body = json_favorites_payload(session, db);
      return true;
    }
    if (req.path == "/api/favorites/clear" && req.method == "POST") {
      session.favorite_car_ids.clear();
      res.content_type = "application/json; charset=utf-8";
      res.body = json_favorites_payload(session, db);
      return true;
    }

    if (req.path == "/api/compare" && req.method == "GET") {
      std::vector<int64_t> ids = parse_query_ids(req.query);
      bool from_session_top = false;
      if (ids.empty() && !session.last_top_rec_ids.empty()) {
        ids = session.last_top_rec_ids;
        from_session_top = true;
      }
      std::vector<CarRow> sel;
      for (int64_t id : ids) {
        if (auto c = db.get_car(id)) sel.push_back(*c);
      }
      std::ostringstream o;
      o << "{\"from_session_top\":" << (from_session_top ? "true" : "false") << ",\"empty\":" << (sel.empty() ? "true" : "false");
      if (sel.empty()) {
        o << ",\"prefs\":" << json_prefs_object(session.prefs) << "}";
        res.content_type = "application/json; charset=utf-8";
        res.body = o.str();
        return true;
      }
      size_t winner = 0;
      double best = -1.0;
      for (size_t i = 0; i < sel.size(); ++i) {
        const double s = score_soft(sel[i], session.prefs);
        if (s > best) {
          best = s;
          winner = i;
        }
      }
      o << ",\"winner_index\":" << winner << ",\"leader_explain\":" << json_str(explain_match_ru(sel[winner], session.prefs))
        << ",\"prefs\":" << json_prefs_object(session.prefs) << ",\"cars\":[";
      for (size_t i = 0; i < sel.size(); ++i) {
        if (i) o << ',';
        const double sc = score_soft(sel[i], session.prefs);
        o << json_car_rec_object(sel[i], sc, explain_match_ru(sel[i], session.prefs), &session.prefs);
      }
      o << "]}";
      res.content_type = "application/json; charset=utf-8";
      res.body = o.str();
      return true;
    }

    if (req.path == "/api/extras" && req.method == "GET") {
      auto cars = db.list_cars();
      extras_ensure_blind(session, cars, false);
      std::vector<CarRow> blind;
      blind.reserve(3);
      for (int64_t id : session.blindtest_ids) {
        if (auto c = db.get_car(id)) blind.push_back(*c);
      }
      auto top3 = recommend(cars, session.prefs, 3);
      const std::string sense = build_sensitivity_fragment(session.prefs, cars);
      std::ostringstream o;
      o << "{\"blind_pick\":" << session.blindtest_pick << ",\"blind_ids\":[";
      for (size_t i = 0; i < session.blindtest_ids.size(); ++i) {
        if (i) o << ',';
        o << session.blindtest_ids[i];
      }
      o << "],\"blind_cars\":[";
      for (size_t i = 0; i < blind.size(); ++i) {
        if (i) o << ',';
        o << json_car_rec_object(blind[i], 0.0, "");
      }
      o << "],\"insight\":[";
      for (size_t i = 0; i < top3.size(); ++i) {
        if (i) o << ',';
        const auto& sc = top3[i];
        const auto& c = sc.car;
        const auto L = estimate_life_with_car_5y(c, 15000);
        const auto S = snow_index_for_car(c, session.prefs);
        o << "{\"car\":" << json_car_rec_object(c, sc.score, sc.explain_ru) << ",\"life_5y\":" << json_life_obj(L)
          << ",\"snow\":" << json_snow_obj(S) << "}";
      }
      o << "],\"sensitivity_html\":" << json_str(sense) << "}";
      res.content_type = "application/json; charset=utf-8";
      res.body = o.str();
      return true;
    }
    if (req.path == "/api/extras/blind/pick" && req.method == "POST") {
      auto it = req.form.find("pick");
      int p = -1;
      if (it != req.form.end()) {
        try {
          p = std::stoi(it->second);
        } catch (...) {
          p = -1;
        }
      }
      if (p >= 0 && p <= 2 && session.blindtest_ids.size() == 3) session.blindtest_pick = p;
      res.content_type = "application/json; charset=utf-8";
      res.body = "{\"ok\":true}";
      return true;
    }
    if (req.path == "/api/extras/blind/new" && req.method == "POST") {
      auto cars2 = db.list_cars();
      extras_ensure_blind(session, cars2, true);
      res.content_type = "application/json; charset=utf-8";
      res.body = "{\"ok\":true}";
      return true;
    }

    if (req.path == "/api/admin/cars" && req.method == "GET") {
      auto all = db.list_cars();
      std::ostringstream o;
      o << "{\"cars\":[";
      for (size_t i = 0; i < all.size(); ++i) {
        if (i) o << ',';
        o << json_car_rec_object(all[i], 0.0, "");
      }
      o << "]}";
      res.content_type = "application/json; charset=utf-8";
      res.body = o.str();
      return true;
    }
    if (req.path == "/api/admin/cars/create" && req.method == "POST") {
      std::string err;
      CarRow c = car_from_form(req.form, err);
      res.content_type = "application/json; charset=utf-8";
      if (!err.empty()) {
        res.body = std::string("{\"ok\":false,\"error\":") + json_str(err) + "}";
        return true;
      }
      if (!db.create_car(c)) {
        res.body = "{\"ok\":false,\"error\":\"Ошибка сохранения в БД.\"}";
        return true;
      }
      res.body = "{\"ok\":true}";
      return true;
    }
    if (req.path == "/api/admin/cars/update" && req.method == "POST") {
      auto it = req.form.find("id");
      int64_t id = 0;
      try {
        id = (it == req.form.end()) ? 0 : std::stoll(it->second);
      } catch (...) {
        id = 0;
      }
      std::string err;
      CarRow c = car_from_form(req.form, err);
      c.id = id;
      res.content_type = "application/json; charset=utf-8";
      if (id <= 0) err = "Некорректный id.";
      if (!err.empty()) {
        res.body = std::string("{\"ok\":false,\"error\":") + json_str(err) + "}";
        return true;
      }
      if (!db.update_car(id, c)) {
        res.body = "{\"ok\":false,\"error\":\"Ошибка обновления в БД.\"}";
        return true;
      }
      res.body = "{\"ok\":true}";
      return true;
    }
    if (req.path == "/api/admin/cars/delete" && req.method == "POST") {
      auto it = req.form.find("id");
      int64_t id = 0;
      try {
        id = (it == req.form.end()) ? 0 : std::stoll(it->second);
      } catch (...) {
        id = 0;
      }
      res.content_type = "application/json; charset=utf-8";
      if (id > 0) db.delete_car(id);
      res.body = "{\"ok\":true}";
      return true;
    }

    if (req.path == "/compare" && req.method == "GET") {
      std::vector<int64_t> ids = parse_query_ids(req.query);
      bool from_session_top = false;
      if (ids.empty() && !session.last_top_rec_ids.empty()) {
        ids = session.last_top_rec_ids;
        from_session_top = true;
      }
      std::vector<CarRow> sel;
      for (int64_t id : ids) {
        if (auto c = db.get_car(id)) sel.push_back(*c);
      }
      res.body = html::compare_page(sel, session.prefs, from_session_top);
      return true;
    }

    if (req.path == "/favorites" && req.method == "GET") {
      std::vector<CarRow> cars;
      for (int64_t id : session.favorite_car_ids) {
        if (auto c = db.get_car(id)) cars.push_back(*c);
      }
      res.body = html::favorites_page(cars, session.prefs, session.favorite_car_ids);
      return true;
    }
    if (req.path == "/favorite/toggle" && req.method == "POST") {
      auto it_id = req.form.find("id");
      auto it_next = req.form.find("next");
      int64_t car_id = 0;
      try {
        car_id = (it_id == req.form.end()) ? 0 : std::stoll(it_id->second);
      } catch (...) {
        car_id = 0;
      }
      std::string next_raw = (it_next == req.form.end()) ? "" : it_next->second;
      std::string next = sanitize_fav_next(next_raw);
      if (car_id > 0) {
        auto& v = session.favorite_car_ids;
        auto f = std::find(v.begin(), v.end(), car_id);
        if (f != v.end())
          v.erase(f);
        else if (v.size() < 80)
          v.push_back(car_id);
      }
      res = redirect_to(next);
      if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;
      return true;
    }
    if (req.path == "/favorites/clear" && req.method == "POST") {
      session.favorite_car_ids.clear();
      res = redirect_to("/favorites");
      if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;
      return true;
    }

    if (req.path == "/wizard/reset" && req.method == "POST") {
      session.prefs = Preferences{};
      session.last_top_rec_ids.clear();
      session.my_car = MyCarProfile{};
      res = redirect_to("/wizard");
      if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;
      return true;
    }
    if (req.path == "/chat/reset" && req.method == "POST") {
      session.prefs = Preferences{};
      session.last_top_rec_ids.clear();
      session.my_car = MyCarProfile{};
      res = redirect_to("/chat");
      if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;
      return true;
    }

    if (req.path == "/" && req.method == "GET") {
      res.body = html::home_page();
      return true;
    }

    if (req.path == "/admin/cars" && req.method == "GET") {
      res.body = html::cars_list_page(db.list_cars());
      return true;
    }
    if (req.path == "/admin/cars/new" && req.method == "GET") {
      res.body = html::car_form_page("Добавить авто", "/admin/cars/create", nullptr);
      return true;
    }
    if (req.path == "/admin/cars/create" && req.method == "POST") {
      std::string err;
      CarRow c = car_from_form(req.form, err);
      if (!err.empty()) {
        res.body = html::car_form_page("Добавить авто", "/admin/cars/create", &c, err);
        return true;
      }
      if (!db.create_car(c)) {
        res.body = html::car_form_page("Добавить авто", "/admin/cars/create", &c, "Ошибка сохранения в БД.");
        return true;
      }
      res = redirect_to("/admin/cars");
      if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;
      return true;
    }
    if (req.path == "/admin/cars/edit" && req.method == "GET") {
      auto id = q_int64(req.query, "id");
      if (!id) {
        res.status = 400;
        res.content_type = "text/plain; charset=utf-8";
        res.body = "Bad Request";
        return true;
      }
      auto car = db.get_car(*id);
      if (!car) {
        res.status = 404;
        res.content_type = "text/plain; charset=utf-8";
        res.body = "Not Found";
        return true;
      }
      res.body = html::car_form_page("Редактировать авто", "/admin/cars/update", &*car);
      return true;
    }
    if (req.path == "/admin/cars/update" && req.method == "POST") {
      auto it = req.form.find("id");
      int64_t id = 0;
      try {
        id = (it == req.form.end()) ? 0 : std::stoll(it->second);
      } catch (...) {
        id = 0;
      }
      std::string err;
      CarRow c = car_from_form(req.form, err);
      c.id = id;
      if (id <= 0) err = "Некорректный id.";
      if (!err.empty()) {
        res.body = html::car_form_page("Редактировать авто", "/admin/cars/update", &c, err);
        return true;
      }
      if (!db.update_car(id, c)) {
        res.body = html::car_form_page("Редактировать авто", "/admin/cars/update", &c, "Ошибка обновления в БД.");
        return true;
      }
      res = redirect_to("/admin/cars");
      if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;
      return true;
    }
    if (req.path == "/admin/cars/delete" && req.method == "POST") {
      auto it = req.form.find("id");
      int64_t id = 0;
      try {
        id = (it == req.form.end()) ? 0 : std::stoll(it->second);
      } catch (...) {
        id = 0;
      }
      if (id > 0) db.delete_car(id);
      res = redirect_to("/admin/cars");
      if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;
      return true;
    }

    if (req.path == "/wizard/my-car" && req.method == "POST") {
      MyCarProfile tmp;
      std::string err;
      if (parse_my_car_form(req.form, tmp, err)) session.my_car = tmp;
      res = redirect_to("/wizard#mycar");
      if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;
      return true;
    }
    if (req.path == "/wizard/my-car/clear" && req.method == "POST") {
      session.my_car = MyCarProfile{};
      res = redirect_to("/wizard#mycar");
      if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;
      return true;
    }

    if (req.path == "/chat/my-car" && req.method == "POST") {
      MyCarProfile tmp;
      std::string err;
      if (parse_my_car_form(req.form, tmp, err)) session.my_car = tmp;
      res = redirect_to("/chat#mycar");
      if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;
      return true;
    }
    if (req.path == "/chat/my-car/clear" && req.method == "POST") {
      session.my_car = MyCarProfile{};
      res = redirect_to("/chat#mycar");
      if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;
      return true;
    }
    if (req.path == "/wizard/client-profile" && req.method == "POST") {
      apply_client_profile_form(session.client, req.form);
      res = redirect_to("/wizard#client-tools");
      if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;
      return true;
    }
    if (req.path == "/chat/client-profile" && req.method == "POST") {
      apply_client_profile_form(session.client, req.form);
      res = redirect_to("/chat#client-tools");
      if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;
      return true;
    }

    if (req.path == "/client-tools" && req.method == "GET") {
      auto cars = db.list_cars();
      auto recs = recommend(cars, session.prefs, 3);
      res.body = client_tools_page(session.client, session.prefs, recs);
      return true;
    }
    if (req.path == "/client-tools/save" && req.method == "POST") {
      apply_client_profile_form(session.client, req.form);
      res = redirect_to("/client-tools");
      if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;
      return true;
    }

    if (req.path == "/wizard" && req.method == "GET") {
      Preferences p = session.prefs;
      auto cars = db.list_cars();
      auto eligible = recommend(cars, p, 1000);
      auto recs = recommend(cars, p, 7);
      session_store_top_rec_ids(session, recs);
      auto follow = wizard_followups(p, (int)eligible.size());
      res.body = html::wizard_page(p, recs, follow, session.favorite_car_ids, build_sensitivity_fragment(p, cars), session.my_car,
                                   "/wizard/my-car", "/wizard/my-car/clear", session.client);
      return true;
    }
    if (req.path == "/wizard" && req.method == "POST") {
      Preferences p = prefs_from_form(req.form);
      session.prefs = p;
      auto cars = db.list_cars();
      auto eligible = recommend(cars, p, 1000);
      auto follow = wizard_followups(p, (int)eligible.size());
      auto recs = recommend(cars, p, 7);
      session_store_top_rec_ids(session, recs);
      res.body = html::wizard_page(p, recs, follow, session.favorite_car_ids, build_sensitivity_fragment(p, cars), session.my_car,
                                   "/wizard/my-car", "/wizard/my-car/clear", session.client);
      return true;
    }

    if (req.path == "/chat" && req.method == "GET") {
      auto cars = db.list_cars();
      auto recs = recommend(cars, session.prefs, 5);
      session_store_top_rec_ids(session, recs);
      res.body = html::chat_page(session.history, session.prefs, recs, session.favorite_car_ids, build_sensitivity_fragment(session.prefs, cars),
                                 session.my_car, "/chat/my-car", "/chat/my-car/clear", session.client);
      return true;
    }
    if (req.path == "/chat" && req.method == "POST") {
      auto it = req.form.find("message");
      std::string msg = (it == req.form.end()) ? "" : it->second;
      if (!msg.empty()) {
        session.history.push_back({"user", msg});
        if (chat_wants_reset(msg)) {
          session.prefs = Preferences{};
          session.last_top_rec_ids.clear();
          session.my_car = MyCarProfile{};
        }
        Preferences delta = prefs_from_chat_text(msg, &session.prefs);
        merge_chat_prefs(session.prefs, delta);

        auto cars = db.list_cars();
        auto recs_post = recommend(cars, session.prefs, 5);
        session_store_top_rec_ids(session, recs_post);
        auto eligible = recommend(cars, session.prefs, 1000);
        auto follow = wizard_followups(session.prefs, (int)eligible.size());

        std::string answer = "Учёл ваш ответ в критериях подбора.";
        if (eligible.empty()) {
          answer += " Сейчас в базе нет машин под эти условия — напишите, что готовы ослабить (бюджет, кузов, привод, багажник).";
        } else {
          answer += " Подходит примерно " + std::to_string((int)eligible.size()) + " вариант(ов) — смотрите справа.";
        }
        for (const auto& wq : follow) {
          answer += "\n\n";
          answer += (wq.kind == WizardQuestion::Kind::Lead ? "Наводящий вопрос: " : "Уточняющий вопрос: ");
          answer += wq.text;
        }

        answer = append_chat_insights(answer, msg, recs_post, session.prefs);
        const std::string edu = education_hint_for_message_ru(msg, session.client.education_mode);
        if (!edu.empty()) answer += edu;

        session.history.push_back({"assistant", answer});
      }
      res = redirect_to("/chat");
      if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;
      return true;
    }

    if (req.path == "/extras" && req.method == "GET") {
      auto cars = db.list_cars();
      extras_ensure_blind(session, cars, false);
      std::vector<CarRow> blind;
      blind.reserve(3);
      for (int64_t id : session.blindtest_ids) {
        if (auto c = db.get_car(id)) blind.push_back(*c);
      }
      auto top3 = recommend(cars, session.prefs, 3);
      const std::string sense = build_sensitivity_fragment(session.prefs, cars);
      res.body = html::extras_page(session.prefs, top3, blind, session.blindtest_pick, sense);
      return true;
    }
    if (req.path == "/extras/blind/pick" && req.method == "POST") {
      auto it = req.form.find("pick");
      int p = -1;
      if (it != req.form.end()) {
        try {
          p = std::stoi(it->second);
        } catch (...) {
          p = -1;
        }
      }
      if (p >= 0 && p <= 2 && session.blindtest_ids.size() == 3) session.blindtest_pick = p;
      res = redirect_to("/extras#blind");
      if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;
      return true;
    }
    if (req.path == "/extras/blind/new" && req.method == "POST") {
      auto cars = db.list_cars();
      extras_ensure_blind(session, cars, true);
      res = redirect_to("/extras#blind");
      if (!set_cookie.empty()) res.headers["Set-Cookie"] = set_cookie;
      return true;
    }

    return false;
  };

  std::cout << "Server: http://" << cfg.host << ":" << cfg.port << "\n";
  std::cout << "DB: " << db_path << "\n";
  return http::run_server(cfg, handler);
}

