#include "reco.h"

#include "http.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <climits>
#include <regex>
#include <sstream>
#include <unordered_set>
#include <vector>

static std::optional<int> to_int(const std::map<std::string, std::string>& f, const std::string& k) {
  auto it = f.find(k);
  if (it == f.end() || it->second.empty()) return std::nullopt;
  try {
    return std::stoi(it->second);
  } catch (...) {
    return std::nullopt;
  }
}

static std::optional<double> to_double(const std::map<std::string, std::string>& f, const std::string& k) {
  auto it = f.find(k);
  if (it == f.end() || it->second.empty()) return std::nullopt;
  try {
    std::string s = it->second;
    std::replace(s.begin(), s.end(), ',', '.');
    return std::stod(s);
  } catch (...) {
    return std::nullopt;
  }
}

static std::optional<std::string> to_str(const std::map<std::string, std::string>& f, const std::string& k) {
  auto it = f.find(k);
  if (it == f.end() || it->second.empty()) return std::nullopt;
  return it->second;
}

static std::string lower(std::string s) {
  for (char& c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  return s;
}

static std::string trim(std::string s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
  return s;
}

Preferences prefs_from_form(const std::map<std::string, std::string>& form) {
  Preferences p;
  p.min_price = to_int(form, "min_price");
  p.max_price = to_int(form, "max_price");
  p.max_fuel = to_double(form, "max_fuel");
  p.min_seats = to_int(form, "min_seats");
  p.min_trunk = to_int(form, "min_trunk");
  p.min_power = to_int(form, "min_power");
  p.max_power = to_int(form, "max_power");
  p.body_type = to_str(form, "body_type");
  p.gearbox = to_str(form, "gearbox");
  p.drive = to_str(form, "drive");
  p.brand_substr = to_str(form, "brand_substr");
  p.segment = to_str(form, "segment");
  if (auto e = to_str(form, "energy")) {
    if (*e == "ev")
      p.only_ev = true;
    else if (*e == "ice")
      p.only_ev = false;
    else
      p.only_ev = std::nullopt;
  }
  if (auto tr = to_str(form, "trip_profile")) {
    const std::string& v = *tr;
    if (v == "city" || v == "highway" || v == "family" || v == "dacha" || v == "taxi") p.trip_profile = v;
  }
  if (form.find("soft_awd") != form.end()) p.drive_soft = "awd";
  if (form.find("soft_crossover") != form.end()) p.body_soft = "crossover";
  if (form.find("soft_at") != form.end()) p.gearbox_soft = "at";
  if (auto ss = to_int(form, "soft_min_seats")) {
    if (*ss >= 5 && *ss <= 9) p.soft_min_seats = *ss;
  }
  if (auto ex = to_str(form, "brand_exclude")) {
    std::string x = lower(trim(std::string(*ex)));
    if (!x.empty()) p.brand_exclude_substr = x;
  }
  return p;
}

// std::stoi бросает out_of_range на длинных числах — из-за этого POST /chat давал 500.
static std::optional<int> chat_parse_int_strict(const std::string& s) {
  if (s.empty()) return std::nullopt;
  try {
    size_t idx = 0;
    long long v = std::stoll(s, &idx, 10);
    if (idx != s.size()) return std::nullopt;
    if (v > INT_MAX) return INT_MAX;
    if (v < INT_MIN) return INT_MIN;
    return static_cast<int>(v);
  } catch (...) {
    return std::nullopt;
  }
}

static std::optional<int> chat_parse_million_rub(std::string s) {
  for (char& c : s)
    if (c == ',') c = '.';
  try {
    size_t idx = 0;
    double x = std::stod(s, &idx);
    if (idx != s.size()) return std::nullopt;
    long long rub = static_cast<long long>(std::llround(x * 1'000'000.0));
    if (rub > INT_MAX) return INT_MAX;
    if (rub < 0) return 0;
    return static_cast<int>(rub);
  } catch (...) {
    return std::nullopt;
  }
}

static std::optional<int> chat_parse_thousands_rub(const std::string& s) {
  if (auto k = chat_parse_int_strict(s)) {
    long long rub = static_cast<long long>(*k) * 1000LL;
    if (rub > INT_MAX) return INT_MAX;
    if (rub < 0) return 0;
    return static_cast<int>(rub);
  }
  return std::nullopt;
}

static bool brand_matches(const CarRow& c, const std::string& sub) {
  std::string s = lower(sub);
  std::string b = lower(c.brand);
  std::string n = lower(c.name);
  return b.find(s) != std::string::npos || n.find(s) != std::string::npos;
}

static std::string chat_cap_brand_token(const char* br) {
  std::string r = br;
  if (r.size() <= 3) {
    for (char& c : r) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return r;
  }
  r[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(r[0])));
  return r;
}

// Длинные подстроки раньше — чтобы «land cruiser» не превратился в «land».
static const char* kChatBrandKeywords[] = {
    "land cruiser", "mercedes-benz", "alfa romeo", "aston martin", "rolls-royce", "rolls royce", "great wall",
    "mitsubishi", "chevrolet", "cadillac", "changan", "hongqi", "seres", "jetour", "volkswagen", "mercedes",
    "infiniti",   "renault",    "peugeot",    "citroen",    "subaru",     "suzuki",      "honda",      "ford",
    "opel",       "dodge",      "jeep",       "porsche",    "genesis",    "jac",        "gac",        "voyah",
    "exeed",      "omoda",      "jaecoo",     "haval",      "geely",      "chery",      "byd",        "zeekr",
    "tank",       "gwm",        "nio",        "xpeng",      "li auto",    "lixiang",    "volvo",      "lexus",
    "tesla",      "bmw",        "audi",       "skoda",      "seat",       "mini",       "smart",      "fiat",
    "alfa",       "maserati",   "bentley",    "ferrari",    "lamborghini", "mazda",     "nissan",     "toyota",
    "hyundai",    "kia",        "lada",       "uaz",        "vaz",        "москвич",    "ravon",      "daewoo",
    "ssangyong",  "isuzu",      "man",        "iveco",
};

static std::optional<std::string> chat_detect_brand(const std::string& t) {
  size_t best_pos = std::string::npos;
  std::optional<std::string> best;
  for (const char* br : kChatBrandKeywords) {
    size_t p = t.rfind(br);
    if (p == std::string::npos) continue;
    if (best_pos == std::string::npos || p >= best_pos) {
      best_pos = p;
      best = chat_cap_brand_token(br);
    }
  }
  return best;
}

static std::vector<std::string> chat_split_message(const std::string& raw_lower) {
  std::string t = trim(raw_lower);
  for (char& c : t) {
    if (c == '\r' || c == '\n') c = ' ';
  }
  std::vector<std::string> parts;
  auto push = [&](std::string s) {
    s = trim(s);
    if (!s.empty()) parts.push_back(s);
  };
  std::string cur;
  for (size_t i = 0; i < t.size(); ++i) {
    char c = t[i];
    if (c == ';' || c == '!' || c == '.' || c == '\n') {
      push(cur);
      cur.clear();
      continue;
    }
    if (c == ',' && i + 1 < t.size()) {
      size_t j = i + 1;
      while (j < t.size() && (t[j] == ' ' || t[j] == '\t')) ++j;
      if (j < t.size()) {
        bool prev_digit = i > 0 && std::isdigit(static_cast<unsigned char>(t[i - 1]));
        bool next_digit = std::isdigit(static_cast<unsigned char>(t[j]));
        if (!(prev_digit && next_digit)) {
          push(cur);
          cur.clear();
          i = j - 1;
          continue;
        }
      }
    }
    cur.push_back(c);
  }
  push(cur);
  if (parts.empty() && !t.empty()) parts.push_back(t);
  return parts;
}

// Один фрагмент сообщения (предложение или кусок через запятую) → частичные Preferences.
static bool power_max_settled(const Preferences& p) {
  if (p.max_power_ceiling_opt_out.has_value() && *p.max_power_ceiling_opt_out) return true;
  return p.max_power.has_value();
}

static Preferences prefs_from_chat_segment(std::string t, const Preferences* ctx = nullptr) {
  Preferences p;
  t = trim(lower(t));
  if (t.empty()) return p;

  std::smatch m;

  {
    std::optional<int> cand_min, cand_max;
    auto take_min_pr = [&](int v) {
      if (v <= 0) return;
      if (!cand_min || v > *cand_min) cand_min = v;
    };
    auto take_max_pr = [&](int v) {
      if (v <= 0) return;
      if (!cand_max || v < *cand_max) cand_max = v;
    };
    if (std::regex_search(t, m, std::regex(R"rx(от\s+(\d{6,})\s*(?:руб)?)rx"))) {
      if (auto v = chat_parse_int_strict(m[1].str())) take_min_pr(*v);
    }
    if (std::regex_search(t, m, std::regex(R"rx(от\s+(\d+(?:[.,]\d+)?)\s*млн)rx"))) {
      if (auto v = chat_parse_million_rub(m[1].str())) take_min_pr(*v);
    }
    // «цена >10 млн», «цена > 10 млн», «>10 млн» — нижняя граница (пробел после > необязателен).
    if (std::regex_search(t, m, std::regex(R"rx((?:цена|бюджет|стоимость)\s*>\s*(\d+(?:[.,]\d+)?)\s*млн)rx"))) {
      if (auto v = chat_parse_million_rub(m[1].str())) take_min_pr(*v);
    }
    if (std::regex_search(t, m, std::regex(R"rx((?:цена|бюджет|стоимость)\s*(?:>=|=>)\s*(\d+(?:[.,]\d+)?)\s*млн)rx"))) {
      if (auto v = chat_parse_million_rub(m[1].str())) take_min_pr(*v);
    }
    if (std::regex_search(t, m, std::regex(R"rx(^>\s*(\d+(?:[.,]\d+)?)\s*млн)rx"))) {
      if (auto v = chat_parse_million_rub(m[1].str())) take_min_pr(*v);
    }
    if (std::regex_search(t, m, std::regex(R"rx((?:>|свыше|не\s*менее|хотя\s*бы)\s*(\d+(?:[.,]\d+)?)\s*млн)rx"))) {
      if (auto v = chat_parse_million_rub(m[1].str())) take_min_pr(*v);
    }
    if (std::regex_search(t, m, std::regex(R"rx((?:цена|бюджет|стоимость)\s+(?:более|больше|свыше)\s+(\d+(?:[.,]\d+)?)\s*млн)rx"))) {
      if (auto v = chat_parse_million_rub(m[1].str())) take_min_pr(*v);
    }
    if (std::regex_search(t, m, std::regex(R"rx((?:цена|бюджет|стоимость)\s+(?:более|больше|свыше)\s+(\d+(?:[.,]\d+)?)\s*(?:миллион|миллионов))rx"))) {
      if (auto v = chat_parse_million_rub(m[1].str())) take_min_pr(*v);
    }
    if (std::regex_search(t, m, std::regex(R"rx(от\s+(\d+)\s*тыс)rx"))) {
      if (auto v = chat_parse_thousands_rub(m[1].str())) take_min_pr(*v);
    }
    if (std::regex_search(t, m, std::regex(R"rx((?:>|свыше|хотя\s*бы|не\s*менее|более|больше)\s*(\d{6,})\s*(?:руб|р\.?)?)rx"))) {
      if (auto v = chat_parse_int_strict(m[1].str())) take_min_pr(*v);
    }
    if (std::regex_search(t, m, std::regex(R"rx((?:бюджет|цена|стоимость)\s*(?:до|не\s*более)\s+(\d+(?:[.,]\d+)?)\s*млн)rx"))) {
      if (auto v = chat_parse_million_rub(m[1].str())) take_max_pr(*v);
    }
    if (std::regex_search(t, m, std::regex(R"rx((?:бюджет|цена|стоимость)\s*(?:до|не\s*более)\s+(\d{6,})\s*(?:руб)?)rx"))) {
      if (auto v = chat_parse_int_strict(m[1].str())) take_max_pr(*v);
    }
    if (std::regex_search(t, m, std::regex(R"rx((?:не\s*дороже|максимум)\s+(\d+(?:[.,]\d+)?)\s*млн)rx"))) {
      if (auto v = chat_parse_million_rub(m[1].str())) take_max_pr(*v);
    }
    if (std::regex_search(t, m, std::regex(R"rx((?:не\s*дороже|максимум)\s+(\d{6,})\s*(?:руб)?)rx"))) {
      if (auto v = chat_parse_int_strict(m[1].str())) take_max_pr(*v);
    }
    if (std::regex_search(t, m, std::regex(R"rx(до\s+(\d{6,})\s*(?:руб)?)rx"))) {
      if (auto v = chat_parse_int_strict(m[1].str())) take_max_pr(*v);
    }
    if (std::regex_search(t, m, std::regex(R"rx(до\s+(\d+(?:[.,]\d+)?)\s*млн)rx"))) {
      if (auto v = chat_parse_million_rub(m[1].str())) take_max_pr(*v);
    }
    if (std::regex_search(t, m, std::regex(R"rx(до\s+(\d+)\s*тыс)rx"))) {
      if (auto v = chat_parse_thousands_rub(m[1].str())) take_max_pr(*v);
    }
    if (std::regex_search(t, m, std::regex(R"rx(<\s*(\d{6,})\s*(?:руб|р\.?)?)rx"))) {
      if (auto v = chat_parse_int_strict(m[1].str())) take_max_pr(*v);
    }
    // Только цифры — верх бюджета в рублях (например 5000000 после просьбы «цифрами»).
    if (!cand_max && std::regex_match(t, m, std::regex(R"rx(^(\d{6,8})$)rx"))) {
      if (auto v = chat_parse_int_strict(m[1].str())) {
        if (*v >= 400000 && *v <= 120000000) take_max_pr(*v);
      }
    }
    if (cand_min) p.min_price = *cand_min;
    if (cand_max) p.max_price = *cand_max;
  }

  {
    bool opt = false;
    if (t.find("любой вариант") != std::string::npos) opt = true;
    if (t.find("без нижн") != std::string::npos || t.find("без миним") != std::string::npos) opt = true;
    if (t.find("только до максим") != std::string::npos) opt = true;
    if (t.find("оставляем") != std::string::npos && t.find("максим") != std::string::npos) opt = true;
    if (t.find("оставляем любой") != std::string::npos) opt = true;
    if ((t.find("не нужен") != std::string::npos || t.find("не нужна") != std::string::npos || t.find("не нужно") != std::string::npos ||
         t.find("не надо") != std::string::npos) &&
        (t.find("нижн") != std::string::npos || t.find("порог") != std::string::npos || t.find("минимум") != std::string::npos ||
         t.size() < 16))
      opt = true;
    if (t == "нет" || t == "нет." || t == "no" || t == "неа") opt = true;
    if (opt) p.min_price_floor_opt_out = true;
  }

  {
    std::optional<int> seats;
    auto consider_seats = [&](int v) {
      if (v < 2 || v > 9) return;
      if (!seats || v > *seats) seats = v;
    };
    if (std::regex_search(t, m, std::regex(R"rx(семья\s+(?:из\s+)?(\d+))rx"))) {
      if (auto people = chat_parse_int_strict(m[1].str())) consider_seats(std::max(*people, 4));
    }
    if (std::regex_search(t, m, std::regex(R"rx((\d+)\s*челов)rx"))) {
      if (auto people = chat_parse_int_strict(m[1].str())) consider_seats(std::max(*people, 4));
    }
    if (std::regex_search(t, m, std::regex(R"rx((\d{1,2})\s*[\-\s]*мест)rx"))) {
      if (auto n = chat_parse_int_strict(m[1].str())) consider_seats(*n);
    }
    if (std::regex_search(t, m, std::regex(R"rx((\d{1,2})\s*местн)rx"))) {
      if (auto n = chat_parse_int_strict(m[1].str())) consider_seats(*n);
    }
    if (t.find("семимест") != std::string::npos || t.find("7 мест") != std::string::npos || t.find("7-мест") != std::string::npos)
      consider_seats(7);
    if (t.find("восьмимест") != std::string::npos || t.find("8 мест") != std::string::npos) consider_seats(8);
    if (t.find("большая семья") != std::string::npos || t.find("большой семь") != std::string::npos) consider_seats(6);
    else if (t.find("семья") != std::string::npos && !seats)
      consider_seats(5);
    if (seats) p.min_seats = *seats;
  }

  {
    std::optional<int> tr;
    auto consider_tr = [&](int v) {
      if (v < 200 || v > 950) return;
      if (!tr || v > *tr) tr = v;
    };
    if (std::regex_search(t, m, std::regex(R"rx((\d{3,4})\s*(?:л(?:итр(?:ов)?)?|liters?\b))rx"))) {
      if (auto liters = chat_parse_int_strict(m[1].str())) consider_tr(*liters);
    }
    if (std::regex_search(t, m, std::regex(R"rx(багаж(?:ник)?\s*[:\-]?\s*(\d{3,4}))rx"))) {
      if (auto liters = chat_parse_int_strict(m[1].str())) consider_tr(*liters);
    }
    const bool ctx_awaiting_trunk =
        ctx && ctx->min_seats.has_value() && *ctx->min_seats >= 4 && !ctx->min_trunk.has_value();
    const bool ctx_needs_pwr_max =
        ctx && ctx->min_power.has_value() && !power_max_settled(*ctx);
    if (ctx_awaiting_trunk && !ctx_needs_pwr_max &&
        std::regex_match(t, std::regex(R"rx(\s*\d{3,4}\s*)rx"))) {
      if (auto liters = chat_parse_int_strict(trim(t))) consider_tr(*liters);
    }
    if (t.find("багаж") != std::string::npos) {
      if (t.find("очень") != std::string::npos || t.find("огром") != std::string::npos) consider_tr(550);
      else if (t.find("больш") != std::string::npos) consider_tr(480);
      else if (t.find("обычн") != std::string::npos || t.find("средн") != std::string::npos) consider_tr(380);
      else if (!tr) consider_tr(400);
    }
    if (tr) p.min_trunk = *tr;
  }

  {
    size_t best = std::string::npos;
    std::string val;
    auto hit = [&](const char* kw, const char* v) {
      size_t pos = t.rfind(kw);
      if (pos == std::string::npos) return;
      if (best == std::string::npos || pos >= best) {
        best = pos;
        val = v;
      }
    };
    hit("минивэн", "van");
    hit("минивен", "van");
    hit("универсал", "wagon");
    hit("wagon", "wagon");
    hit("лифтбек", "liftback");
    hit("хэтчбек", "hatchback");
    hit("хэтч", "hatchback");
    hit("седан", "sedan");
    hit("кроссовер", "crossover");
    hit("паркетник", "crossover");
    hit("джип", "crossover");
    hit("suv", "crossover");
    if (best != std::string::npos) p.body_type = val;
  }

  if (t.find("внедорож") != std::string::npos || t.find("дач") != std::string::npos || t.find("гряз") != std::string::npos ||
      t.find("offroad") != std::string::npos) {
    p.drive = "awd";
    if (!p.body_type) p.body_type = "crossover";
  }

  {
    std::optional<std::string> drv;
    size_t best = std::string::npos;
    auto hit_d = [&](const char* kw, const char* v) {
      size_t pos = t.rfind(kw);
      if (pos == std::string::npos) return;
      if (best == std::string::npos || pos >= best) {
        best = pos;
        drv = v;
      }
    };
    if (t.find("полный") != std::string::npos && (t.find("привод") != std::string::npos || t.find("awd") != std::string::npos))
      hit_d("полный", "awd");
    hit_d("передний привод", "fwd");
    hit_d("задний привод", "rwd");
    hit_d("quattro", "awd");
    if (!drv && t.find("передний") != std::string::npos && t.find("привод") != std::string::npos) drv = "fwd";
    if (!drv && t.find("задний") != std::string::npos && t.find("привод") != std::string::npos) drv = "rwd";
    if (drv) p.drive = *drv;
  }

  {
    size_t best = std::string::npos;
    std::string gb;
    auto hit_g = [&](const char* kw, const char* v) {
      size_t pos = t.rfind(kw);
      if (pos == std::string::npos) return;
      if (best == std::string::npos || pos >= best) {
        best = pos;
        gb = v;
      }
    };
    hit_g("мкпп", "mt");
    hit_g("механ", "mt");
    hit_g("вариатор", "cvt");
    hit_g("робот", "robot");
    hit_g("акпп", "at");
    hit_g("автомат", "at");
    if (best != std::string::npos) p.gearbox = gb;
  }

  {
    std::optional<double> cap_f;
    auto consider_fuel = [&](double x) {
      if (x < 3.0 || x > 30.0) return;
      if (!cap_f || x < *cap_f) cap_f = x;
    };
    auto try_fuel_token = [&](const std::string& raw) {
      try {
        std::string x = raw;
        for (char& c : x)
          if (c == ',') c = '.';
        consider_fuel(std::stod(x));
      } catch (...) {
      }
    };
    if (t.find("эконом") != std::string::npos || t.find("низкий расход") != std::string::npos) consider_fuel(7.0);
    // «расход до 8», «до 8,5 л/100» и т.п.
    if (std::regex_search(t, m, std::regex(R"rx((?:расход\s*)?(?:до|не\s*более)\s*(\d+([.,]\d+)?))rx"))) try_fuel_token(m[1].str());
    // Без слова «расход»: «до 9 л», «максимум 10 л/100»
    if (!cap_f && std::regex_search(t, m, std::regex(R"rx((?:^|[\s,;]+)(?:до|макс(?:имум)?|не\s*более|меньше|<)\s*(\d{1,2}(?:[.,]\d+)?)\s*(?:л|л/100|литр))rx")))
      try_fuel_token(m[1].str());
    // Одно число с единицами расхода
    if (!cap_f && std::regex_match(t, m, std::regex(R"rx(^(\d{1,2}(?:[.,]\d+)?)\s*(?:л|л/100км|л/100)\s*$)rx")))
      try_fuel_token(m[1].str());
    // Одно число без единиц — типичный ответ «8» / «12» на вопрос про л/100 км (4…30, иначе путаница с «7 мест»).
    if (!cap_f && std::regex_match(t, m, std::regex(R"rx(^(\d{1,2}(?:[.,]\d+)?)\s*$)rx"))) {
      try {
        std::string x = m[1].str();
        for (char& c : x) if (c == ',') c = '.';
        const double v = std::stod(x);
        if (v >= 4.0 && v <= 30.0) consider_fuel(v);
      } catch (...) {
      }
    }
    if (cap_f) p.max_fuel = *cap_f;
  }

  {
    const bool mentions_fuel = t.find("расход") != std::string::npos || t.find("потреблен") != std::string::npos ||
                               t.find("л/100") != std::string::npos ||
                               (t.find("литр") != std::string::npos && t.find("100") != std::string::npos);
    const bool dismiss = t.find("не важн") != std::string::npos || t.find("не критич") != std::string::npos ||
                         t.find("пофиг") != std::string::npos || t.find("без разницы") != std::string::npos ||
                         t.find("не огранич") != std::string::npos || t.find("не принципиал") != std::string::npos ||
                         t.find("не интерес") != std::string::npos ||
                         (t.find("любой") != std::string::npos && mentions_fuel) ||
                         (t.find("любая") != std::string::npos && mentions_fuel);
    if (mentions_fuel && dismiss) p.fuel_ignore_cap = true;
    // Короткий ответ на вопрос про расход без слова «расход» (как в подсказке «не критично»).
    if (!p.fuel_ignore_cap.has_value() && dismiss && t.size() <= 40 &&
        (t.find("не критич") != std::string::npos || t.find("пофиг") != std::string::npos ||
         t.find("без разницы") != std::string::npos))
      p.fuel_ignore_cap = true;
  }

  {
    const bool ctx_awaiting_trunk_pwr =
        ctx && ctx->min_seats.has_value() && *ctx->min_seats >= 4 && !ctx->min_trunk.has_value();
    const bool ctx_has_min_pwr = ctx && ctx->min_power.has_value();
    const bool ctx_needs_pwr_max = ctx && ctx_has_min_pwr && !power_max_settled(*ctx);

    std::optional<int> min_p;
    std::optional<int> max_p;
    auto consider_min = [&](int v) {
      if (v < 25 || v > 2500) return;
      if (!min_p || v > *min_p) min_p = v;
    };
    auto consider_max = [&](int v) {
      if (v < 25 || v > 2500) return;
      if (!max_p || v < *max_p) max_p = v;
    };
    static const char* pwr_sfx = R"rx((?:л\.?\s*с\.?|л\.?\s*с\b|лс\b|hp\b))rx";

    if (std::regex_search(t, m,
                          std::regex(R"rx(от\s+(\d{2,4})\s*(?:л\.?\s*с\.?|л\.?\s*с\b|лс\b|hp\b)?\s*до\s+(\d{2,4})\s*(?:л\.?\s*с\.?|л\.?\s*с\b|лс\b|hp\b)?)rx"))) {
      if (auto lo = chat_parse_int_strict(m[1].str())) consider_min(*lo);
      if (auto hi = chat_parse_int_strict(m[2].str())) consider_max(*hi);
    }
    if (std::regex_search(t, m,
                          std::regex(R"rx((\d{2,4})\s*(?:л\.?\s*с\.?|л\.?\s*с\b|лс\b|hp\b)?\s*[-–—]\s*(\d{2,4})\s*(?:л\.?\s*с\.?|л\.?\s*с\b|лс\b|hp\b)?)rx"))) {
      if (auto lo = chat_parse_int_strict(m[1].str())) consider_min(*lo);
      if (auto hi = chat_parse_int_strict(m[2].str())) consider_max(*hi);
    }

    if (std::regex_search(t, m, std::regex(std::string(R"rx(от\s+(\d{2,4})\s*)rx") + pwr_sfx))) {
      if (auto v = chat_parse_int_strict(m[1].str())) consider_min(*v);
    }
    if (std::regex_search(t, m, std::regex(std::string(R"rx((?:>|свыше|более|не\s*менее)\s*(\d{2,4})\s*)rx") + pwr_sfx))) {
      if (auto v = chat_parse_int_strict(m[1].str())) consider_min(*v);
    }
    if (std::regex_search(t, m, std::regex(std::string(R"rx((\d{2,4})\s*)rx") + pwr_sfx + R"rx(\s*(?:\+|плюс|и\s*выше|минимум))rx"))) {
      if (auto v = chat_parse_int_strict(m[1].str())) consider_min(*v);
    }
    if (std::regex_search(t, m, std::regex(std::string(R"rx(мощн(?:ости|ость)?\s*(?::|—|-)?\s*(?:>|от|свыше|более)?\s*(\d{2,4})\s*)rx") + pwr_sfx))) {
      if (auto v = chat_parse_int_strict(m[1].str())) consider_min(*v);
    }
    if (!ctx_awaiting_trunk_pwr &&
        std::regex_search(t, m, std::regex(std::string(R"rx((\d{2,4})\s*)rx") + pwr_sfx))) {
      if (auto v = chat_parse_int_strict(m[1].str())) {
        if (ctx_needs_pwr_max)
          consider_max(*v);
        else
          consider_min(*v);
      }
    }

    if (std::regex_search(t, m, std::regex(std::string(R"rx(до\s+(\d{2,4})\s*)rx") + pwr_sfx))) {
      if (auto v = chat_parse_int_strict(m[1].str())) consider_max(*v);
    }
    if (std::regex_search(t, m, std::regex(std::string(R"rx((?:<|менее|не\s*более|максимум)\s*(\d{2,4})\s*)rx") + pwr_sfx))) {
      if (auto v = chat_parse_int_strict(m[1].str())) consider_max(*v);
    }
    if (std::regex_search(t, m, std::regex(std::string(R"rx((\d{2,4})\s*)rx") + pwr_sfx + R"rx(\s*(?:максимум|не\s*более))rx"))) {
      if (auto v = chat_parse_int_strict(m[1].str())) consider_max(*v);
    }

    // Без числа: «мощнее», «хорошая динамика», «для обгонов» — нижняя планка по мощности (л.с.).
    if (!min_p) {
      const bool neg_more = t.find("больше не") != std::string::npos || t.find("более не") != std::string::npos;
      if (!neg_more) {
        bool want_more_power = false;
        if (t.find("мощнее") != std::string::npos) want_more_power = true;
        if (t.find("помощнее") != std::string::npos) want_more_power = true;
        if (t.find("более мощн") != std::string::npos) want_more_power = true;
        if (t.find("больше мощности") != std::string::npos) want_more_power = true;
        if (t.find("динамичнее") != std::string::npos) want_more_power = true;
        if (t.find("двигател") != std::string::npos && t.find("мощн") != std::string::npos &&
            t.find("не мощн") == std::string::npos && t.find("неочень мощн") == std::string::npos)
          want_more_power = true;
        if (t.find("динамик") != std::string::npos) {
          if (t.find("хорош") != std::string::npos || t.find("лучш") != std::string::npos ||
              (t.find("больше") != std::string::npos) || t.find("нужн") != std::string::npos ||
              t.find("надо") != std::string::npos || t.find("хочу") != std::string::npos ||
              t.find("важн") != std::string::npos || t.find("чтобы") != std::string::npos)
            want_more_power = true;
        }
        if (t.find("обгон") != std::string::npos &&
            (t.find("уверен") != std::string::npos || t.find("легче") != std::string::npos ||
             t.find("легко") != std::string::npos || t.find("хочу") != std::string::npos ||
             t.find("нужн") != std::string::npos || t.find("надо") != std::string::npos))
          want_more_power = true;
        if (want_more_power) consider_min(190);
      }
    }

    if (!max_p && (t.find("без верх") != std::string::npos || t.find("только нижн") != std::string::npos ||
                   t.find("без потолка") != std::string::npos || t.find("без лимита сверху") != std::string::npos ||
                   (t.find("не огранич") != std::string::npos && t.find("сверх") != std::string::npos) ||
                   (t.find("не важн") != std::string::npos &&
                    (t.find("мощн") != std::string::npos || t.find("л.с") != std::string::npos || t.find("лс") != std::string::npos ||
                     t.find("верх") != std::string::npos || t.find("потолок") != std::string::npos)))) {
      p.max_power_ceiling_opt_out = true;
    }

    // Короткий ответ «200» / «300» — нижняя или верхняя планка (если нижняя уже задана в сессии — это верх).
    if (!ctx_awaiting_trunk_pwr && !min_p && !max_p) {
      const bool want_max_explicit = t.find("до ") != std::string::npos || t.find("макс") != std::string::npos ||
                                     t.find("не более") != std::string::npos || t.find("менее") != std::string::npos ||
                                     t.find("верхн") != std::string::npos || t.find("потолок") != std::string::npos;
      std::smatch bare;
      if (std::regex_match(t, bare, std::regex(R"rx(\s*(\d{2,4})\s*(?:л\.?\s*с\.?|л\.?\s*с\b|лс\b|hp\b)?\s*)rx"))) {
        if (auto v = chat_parse_int_strict(bare[1].str())) {
          if (want_max_explicit || ctx_needs_pwr_max)
            consider_max(*v);
          else
            consider_min(*v);
        }
      }
    }

    if (min_p) p.min_power = *min_p;
    if (max_p) p.max_power = *max_p;
    if (p.min_power && p.max_power && *p.min_power > *p.max_power) {
      const int lo = *p.min_power;
      p.min_power = *p.max_power;
      p.max_power = lo;
    }
  }

  if (t.find("премиум") != std::string::npos || t.find("премиальн") != std::string::npos) p.segment = "premium";
  if (t.find("масс-маркет") != std::string::npos || t.find("массмаркет") != std::string::npos || t.find("средний класс") != std::string::npos)
    p.segment = "mass";
  if (t.find("эконом сегмент") != std::string::npos ||
      (t.find("бюджетн") != std::string::npos && t.find("сегмент") != std::string::npos))
    p.segment = "economy";

  {
    size_t best = std::string::npos;
    std::string tr;
    auto hit_t = [&](const char* kw, const char* v) {
      size_t pos = t.rfind(kw);
      if (pos == std::string::npos) return;
      if (best == std::string::npos || pos >= best) {
        best = pos;
        tr = v;
      }
    };
    hit_t("такси", "taxi");
    hit_t("коммерч", "taxi");
    hit_t("дач", "dacha");
    hit_t("бездорож", "dacha");
    hit_t("трас", "highway");
    hit_t("шосс", "highway");
    hit_t("далеко езжу", "highway");
    hit_t("по городу", "city");
    hit_t("в городе", "city");
    hit_t("городск", "city");
    hit_t("большая семья", "family");
    hit_t("дет", "family");
    hit_t("семей", "family");
    if (best != std::string::npos) p.trip_profile = tr;
  }

  if (t.find("желательно") != std::string::npos) {
    if ((t.find("полный") != std::string::npos && t.find("привод") != std::string::npos) ||
        t.find(" awd") != std::string::npos || t.find("awd") != std::string::npos || t.find("quattro") != std::string::npos)
      p.drive_soft = "awd";
    if (t.find("кроссовер") != std::string::npos || t.find("suv") != std::string::npos) p.body_soft = "crossover";
    if (t.find("автомат") != std::string::npos || t.find("акпп") != std::string::npos) p.gearbox_soft = "at";
    if (t.find("механ") != std::string::npos || t.find("мкпп") != std::string::npos) p.gearbox_soft = "mt";
    if (std::regex_search(t, m, std::regex(R"rx(желательно[^0-9]{0,20}(\d{1,2})\s*мест)rx"))) {
      if (auto v = chat_parse_int_strict(m[1].str()); v && *v >= 5 && *v <= 9) p.soft_min_seats = *v;
    }
  }

  if (std::regex_search(t, m, std::regex(R"rx((?:без|кроме|не\s*хочу|убери|исключи)\s+([a-zа-яё0-9\-]{2,22}))rx"))) {
    std::string ex = lower(m[1].str());
    if (!ex.empty()) p.brand_exclude_substr = ex;
  }

  if (auto br = chat_detect_brand(t)) p.brand_substr = *br;

  {
    size_t ev_pos = std::string::npos;
    size_t ice_pos = std::string::npos;
    auto mark = [&](const char* kw, size_t& slot) {
      size_t p = t.rfind(kw);
      if (p != std::string::npos && (slot == std::string::npos || p >= slot)) slot = p;
    };
    mark("электромоб", ev_pos);
    mark("электро", ev_pos);
    mark("только тесла", ev_pos);
    mark("только ev", ev_pos);
    mark("не электро", ice_pos);
    mark("без электро", ice_pos);
    mark("только бензин", ice_pos);
    mark("двс", ice_pos);
    mark("бензин", ice_pos);
    mark("дизель", ice_pos);
    if (ev_pos != std::string::npos || ice_pos != std::string::npos) {
      if (ev_pos != std::string::npos && (ice_pos == std::string::npos || ev_pos >= ice_pos))
        p.only_ev = true;
      else
        p.only_ev = false;
    }
  }

  return p;
}

Preferences prefs_from_chat_text(const std::string& text, const Preferences* ctx) {
  Preferences acc;
  std::string norm = trim(lower(text));
  if (norm.empty()) return acc;

  for (const std::string& part : chat_split_message(norm)) {
    Preferences part_ctx;
    if (ctx) part_ctx = *ctx;
    merge_chat_prefs(part_ctx, acc);
    Preferences d = prefs_from_chat_segment(part, &part_ctx);
    merge_chat_prefs(acc, d);
  }
  return acc;
}

static int prefs_depth(const Preferences& p) {
  int n = 0;
  if (p.min_price) n++;
  if (p.min_price_floor_opt_out.has_value() && *p.min_price_floor_opt_out) n++;
  if (p.max_price) n++;
  if (p.max_fuel) n++;
  if (p.fuel_ignore_cap.has_value() && *p.fuel_ignore_cap) n++;
  if (p.min_seats) n++;
  if (p.min_trunk) n++;
  if (p.min_power) n++;
  if (p.max_power) n++;
  if (p.max_power_ceiling_opt_out.has_value() && *p.max_power_ceiling_opt_out) n++;
  if (p.body_type) n++;
  if (p.gearbox) n++;
  if (p.drive) n++;
  if (p.brand_substr) n++;
  if (p.segment) n++;
  if (p.only_ev.has_value()) n++;
  if (p.trip_profile) n++;
  if (p.drive_soft) n++;
  if (p.body_soft) n++;
  if (p.gearbox_soft) n++;
  if (p.soft_min_seats) n++;
  if (p.brand_exclude_substr) n++;
  return n;
}

void merge_chat_prefs(Preferences& acc, const Preferences& delta) {
  auto max_int = [](std::optional<int>& a, const std::optional<int>& b) {
    if (!b) return;
    if (!a || *b > *a) a = *b;
  };
  auto min_int = [](std::optional<int>& a, const std::optional<int>& b) {
    if (!b) return;
    if (!a || *b < *a) a = *b;
  };
  max_int(acc.min_seats, delta.min_seats);
  max_int(acc.min_trunk, delta.min_trunk);
  max_int(acc.min_power, delta.min_power);
  if (delta.min_price) {
    acc.min_price_floor_opt_out = std::nullopt;
    max_int(acc.min_price, delta.min_price);
  }
  if (delta.min_price_floor_opt_out.has_value() && *delta.min_price_floor_opt_out) {
    acc.min_price_floor_opt_out = true;
    acc.min_price = std::nullopt;
  }
  min_int(acc.max_power, delta.max_power);
  if (delta.max_power_ceiling_opt_out.has_value() && *delta.max_power_ceiling_opt_out) {
    acc.max_power_ceiling_opt_out = true;
    acc.max_power = std::nullopt;
  }
  if (acc.min_power && acc.max_power && *acc.min_power > *acc.max_power) {
    const int lo = *acc.min_power;
    acc.min_power = *acc.max_power;
    acc.max_power = lo;
  }
  if (delta.max_price) {
    if (!acc.max_price || *delta.max_price < *acc.max_price) acc.max_price = delta.max_price;
  }
  if (acc.min_price && acc.max_price && *acc.min_price > *acc.max_price) acc.max_price = std::nullopt;
  if (delta.max_fuel) {
    acc.fuel_ignore_cap = std::nullopt;
    if (!acc.max_fuel || *delta.max_fuel < *acc.max_fuel) acc.max_fuel = delta.max_fuel;
  }
  if (delta.fuel_ignore_cap.has_value() && *delta.fuel_ignore_cap) {
    acc.fuel_ignore_cap = true;
    acc.max_fuel = std::nullopt;
  }
  if (delta.body_type) acc.body_type = delta.body_type;
  if (delta.gearbox) acc.gearbox = delta.gearbox;
  if (delta.drive) acc.drive = delta.drive;
  if (delta.brand_substr) acc.brand_substr = delta.brand_substr;
  if (delta.segment) acc.segment = delta.segment;
  if (delta.only_ev.has_value()) acc.only_ev = delta.only_ev;
  if (delta.trip_profile) acc.trip_profile = delta.trip_profile;
  if (delta.drive_soft) acc.drive_soft = delta.drive_soft;
  if (delta.body_soft) acc.body_soft = delta.body_soft;
  if (delta.gearbox_soft) acc.gearbox_soft = delta.gearbox_soft;
  max_int(acc.soft_min_seats, delta.soft_min_seats);
  if (delta.brand_exclude_substr) acc.brand_exclude_substr = delta.brand_exclude_substr;
}

static bool hard_fail(const CarRow& c, const Preferences& p) {
  if (p.min_price && c.price < *p.min_price) return true;
  if (p.max_price && c.price > *p.max_price) return true;
  if (p.max_fuel && c.fuel_consumption > *p.max_fuel) return true;
  if (p.min_seats && c.seats < *p.min_seats) return true;
  if (p.min_trunk && c.trunk < *p.min_trunk) return true;
  if (p.min_power && c.power < *p.min_power) return true;
  if (p.max_power && !(p.max_power_ceiling_opt_out.has_value() && *p.max_power_ceiling_opt_out) && c.power > *p.max_power)
    return true;
  if (p.body_type && c.body_type != *p.body_type) return true;
  if (p.gearbox && c.gearbox != *p.gearbox) return true;
  if (p.drive && c.drive != *p.drive) return true;
  if (p.segment && c.segment != *p.segment) return true;
  if (p.brand_substr && !brand_matches(c, *p.brand_substr)) return true;
  if (p.brand_exclude_substr && brand_matches(c, *p.brand_exclude_substr)) return true;
  if (p.only_ev.has_value()) {
    if (*p.only_ev && !c.is_electric) return true;
    if (!*p.only_ev && c.is_electric) return true;
  }
  return false;
}

double score_soft(const CarRow& c, const Preferences& p) {
  double s = 1.0;
  auto dist01 = [](double a, double b, double scale) { return std::min(1.0, std::abs(a - b) / scale); };

  if (p.max_price) s *= std::exp(-2.0 * dist01(double(c.price), double(*p.max_price), 2'500'000.0));
  if (p.min_price) s *= std::exp(-2.0 * dist01(double(c.price), double(*p.min_price), 2'000'000.0));
  if (p.max_fuel) s *= std::exp(-2.0 * dist01(double(c.fuel_consumption), double(*p.max_fuel), 8.0));
  if (p.min_seats) s *= std::exp(-2.0 * dist01(double(c.seats), double(*p.min_seats), 5.0));
  if (p.min_trunk) s *= std::exp(-2.0 * dist01(double(c.trunk), double(*p.min_trunk), 600.0));
  if (p.min_power) s *= std::exp(-2.0 * dist01(double(c.power), double(*p.min_power), 250.0));
  if (p.max_power) s *= std::exp(-2.0 * dist01(double(c.power), double(*p.max_power), 200.0));

  if (p.body_type && c.body_type == *p.body_type) s *= 1.15;
  if (p.gearbox && c.gearbox == *p.gearbox) s *= 1.10;
  if (p.drive && c.drive == *p.drive) s *= 1.10;
  if (p.segment && c.segment == *p.segment) s *= 1.12;
  if (p.brand_substr && brand_matches(c, *p.brand_substr)) s *= 1.18;

  if (p.trip_profile) {
    const std::string& tr = *p.trip_profile;
    if (tr == "family") {
      s *= (1.0 + 0.04 * std::max(0, c.seats - 4));
      s *= (1.0 + 0.00012 * std::max(0, c.trunk - 360));
    } else if (tr == "city") {
      double ref = 9.0;
      if (p.fuel_ignore_cap.has_value() && *p.fuel_ignore_cap)
        ref = 14.0;
      else if (p.max_fuel)
        ref = std::min(*p.max_fuel, 12.0);
      if (c.fuel_consumption > 0.01) s *= std::exp(0.35 * (ref - c.fuel_consumption) / 9.0);
      else
        s *= 1.05;
    } else if (tr == "highway") {
      s *= (1.0 + 0.00025 * double(std::max(0, c.power - 120)));
      if (c.fuel_consumption > 0.01) s *= std::exp(0.12 * (8.5 - c.fuel_consumption) / 8.5);
    } else if (tr == "dacha") {
      if (c.drive == "awd") s *= 1.16;
      if (c.body_type == "crossover" || c.body_type == "suv") s *= 1.08;
    } else if (tr == "taxi") {
      if (c.segment == "economy") s *= 1.12;
      if (c.fuel_consumption > 0.01 && c.fuel_consumption < 8.2) s *= 1.1;
    }
  }
  if (p.drive_soft && c.drive == *p.drive_soft) s *= 1.12;
  if (p.body_soft && c.body_type == *p.body_soft) s *= 1.1;
  if (p.gearbox_soft && c.gearbox == *p.gearbox_soft) s *= 1.07;
  if (p.soft_min_seats && c.seats >= *p.soft_min_seats) s *= 1.12;

  return std::min(1.0, s);
}

std::vector<ScoredCar> recommend(const std::vector<CarRow>& cars, const Preferences& p, int top_k) {
  std::vector<ScoredCar> scored;
  scored.reserve(cars.size());
  for (const auto& c : cars) {
    if (hard_fail(c, p)) continue;
    ScoredCar sc;
    sc.car = c;
    sc.score = score_soft(c, p);
    sc.explain_ru = explain_match_ru(c, p);
    scored.push_back(std::move(sc));
  }
  std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.score > b.score; });
  if ((int)scored.size() > top_k) scored.resize(top_k);
  return scored;
}

static std::unordered_set<int64_t> eligible_id_set(const std::vector<CarRow>& cars, const Preferences& p) {
  auto v = recommend(cars, p, 2'000'000);
  std::unordered_set<int64_t> s;
  s.reserve(v.size() * 2 + 8);
  for (const auto& sc : v) s.insert(sc.car.id);
  return s;
}

static int eligible_count_only(const std::vector<CarRow>& cars, const Preferences& p) {
  return static_cast<int>(eligible_id_set(cars, p).size());
}

std::vector<BudgetLiftRow> budget_lift_sensitivity(const std::vector<CarRow>& cars, const Preferences& p) {
  std::vector<BudgetLiftRow> rows;
  if (!p.max_price || *p.max_price <= 0) return rows;
  const int base_max = *p.max_price;
  const auto base_ids = eligible_id_set(cars, p);
  const int base_cnt = static_cast<int>(base_ids.size());
  (void)base_cnt;
  for (int pct : {5, 10, 15}) {
    Preferences p2 = p;
    const long long add = std::max(1LL, (long long)base_max * pct / 100LL);
    long long nm = (long long)base_max + add;
    if (nm > INT_MAX) nm = INT_MAX;
    p2.max_price = static_cast<int>(nm);
    auto ids2 = eligible_id_set(cars, p2);
    int novel = 0;
    for (int64_t id : ids2)
      if (!base_ids.count(id)) ++novel;
    BudgetLiftRow r;
    r.bump_percent = pct;
    r.simulated_max_price = *p2.max_price;
    r.eligible_count = static_cast<int>(ids2.size());
    r.newly_unblocked = novel;
    rows.push_back(r);
  }
  return rows;
}

std::vector<RelaxLeverRow> relax_one_filter_sensitivity(const std::vector<CarRow>& cars, const Preferences& p) {
  std::vector<RelaxLeverRow> cand;
  const int base = eligible_count_only(cars, p);
  const int universe = static_cast<int>(cars.size());
  if (base >= universe) return cand;

  auto try_push = [&](const char* key, const char* label_ru, const Preferences& relaxed) {
    const int after = eligible_count_only(cars, relaxed);
    const int gain = after - base;
    if (gain <= 0) return;
    RelaxLeverRow r;
    r.key = key;
    r.label_ru = label_ru;
    r.eligible_after = after;
    r.gain_vs_baseline = gain;
    cand.push_back(std::move(r));
  };

  if (p.max_price) {
    Preferences q = p;
    q.max_price = std::nullopt;
    try_push("max_price", "убрать верхний предел цены (оставить только «от», если был)", q);
  }
  if (p.min_price) {
    Preferences q = p;
    q.min_price = std::nullopt;
    try_push("min_price", "убрать нижний порог цены", q);
  }
  if (p.max_fuel && !(p.fuel_ignore_cap.has_value() && *p.fuel_ignore_cap)) {
    Preferences q = p;
    q.max_fuel = std::nullopt;
    try_push("max_fuel", "убрать потолок по расходу", q);
  }
  if (p.min_seats) {
    Preferences q = p;
    q.min_seats = std::nullopt;
    try_push("min_seats", "убрать минимум мест", q);
  }
  if (p.min_trunk) {
    Preferences q = p;
    q.min_trunk = std::nullopt;
    try_push("min_trunk", "убрать минимум объёма багажника", q);
  }
  if (p.min_power) {
    Preferences q = p;
    q.min_power = std::nullopt;
    try_push("min_power", "убрать нижний порог мощности", q);
  }
  if (p.max_power) {
    Preferences q = p;
    q.max_power = std::nullopt;
    try_push("max_power", "убрать верхний предел мощности", q);
  }
  if (p.max_power_ceiling_opt_out.has_value() && *p.max_power_ceiling_opt_out) {
    Preferences q = p;
    q.max_power_ceiling_opt_out = std::nullopt;
    try_push("max_power_ceiling", "снова спросить верхнюю планку по мощности", q);
  }
  if (p.body_type) {
    Preferences q = p;
    q.body_type = std::nullopt;
    try_push("body_type", "не фиксировать тип кузова", q);
  }
  if (p.gearbox) {
    Preferences q = p;
    q.gearbox = std::nullopt;
    try_push("gearbox", "не фиксировать тип КПП", q);
  }
  if (p.drive) {
    Preferences q = p;
    q.drive = std::nullopt;
    try_push("drive", "не фиксировать привод", q);
  }
  if (p.segment) {
    Preferences q = p;
    q.segment = std::nullopt;
    try_push("segment", "не фиксировать сегмент (economy/mass/premium)", q);
  }
  if (p.brand_substr) {
    Preferences q = p;
    q.brand_substr = std::nullopt;
    try_push("brand_substr", "убрать фильтр по марке (подстрока)", q);
  }
  if (p.only_ev.has_value()) {
    Preferences q = p;
    q.only_ev = std::nullopt;
    try_push("only_ev", "разрешить и ДВС, и электро (убрать ограничение по типу силовой установки)", q);
  }
  if (p.brand_exclude_substr) {
    Preferences q = p;
    q.brand_exclude_substr = std::nullopt;
    try_push("brand_exclude", "убрать исключение марки", q);
  }

  std::sort(cand.begin(), cand.end(), [](const RelaxLeverRow& a, const RelaxLeverRow& b) {
    if (a.gain_vs_baseline != b.gain_vs_baseline) return a.gain_vs_baseline > b.gain_vs_baseline;
    return a.label_ru < b.label_ru;
  });
  return cand;
}

CarRow synthetic_my_car_row(const MyCarProfile& m) {
  CarRow c;
  c.id = 0;
  c.name = "Мой автомобиль (ваш ввод)";
  c.brand = "—";
  int price = 1'400'000;
  if (m.approx_price_rub >= 400'000 && m.approx_price_rub <= 80'000'000) price = m.approx_price_rub;
  c.price = price;
  c.fuel_consumption = m.is_electric ? 0.0 : m.fuel_l100;
  c.body_type = "hatchback";
  c.gearbox = "at";
  c.drive = "fwd";
  c.seats = 5;
  c.trunk = std::max(50, std::min(1200, m.trunk_l));
  c.power = std::max(40, std::min(700, m.power_hp));
  c.is_electric = m.is_electric;
  c.max_speed_kmh = default_max_speed_kmh(c.power, c.is_electric, c.body_type);
  c.segment = "mass";
  return c;
}

static void push(std::vector<WizardQuestion>& out, WizardQuestion::Kind k, const std::string& t) {
  if (out.size() >= 3) return;
  out.push_back(WizardQuestion{k, t});
}

/// Пользователь задал потолок по расходу или явно сказал, что расход не важен — не дублировать вопросы про л/100.
static bool chat_fuel_criterion_settled(const Preferences& p) {
  if (p.max_fuel.has_value()) return true;
  if (p.fuel_ignore_cap.has_value() && *p.fuel_ignore_cap) return true;
  return false;
}

static bool min_price_floor_settled(const Preferences& p) {
  if (p.min_price.has_value()) return true;
  if (p.min_price_floor_opt_out.has_value() && *p.min_price_floor_opt_out) return true;
  return false;
}

std::vector<WizardQuestion> wizard_followups(const Preferences& p, int eligible_count) {
  std::vector<WizardQuestion> out;
  const int depth = prefs_depth(p);
  const bool big_family = p.min_seats && *p.min_seats >= 5;
  const int seats = p.min_seats ? *p.min_seats : 0;

  if (eligible_count == 0) {
    push(out, WizardQuestion::Kind::Lead,
         "Сейчас фильтры отсекают все автомобили в базе — нужно ослабить или переформулировать условия.");
    push(out, WizardQuestion::Kind::Detail,
         "С чего начнём: бюджет в рублях (или «до 2,5 млн»), класс (эконом / массмаркет / премиум), кузов, привод, марка, "
         "только электро или наоборот без электро?");
    return out;
  }

  if (depth <= 2 && eligible_count > 70) {
    push(out, WizardQuestion::Kind::Lead,
         "В базе сотни комбинаций кузова, коробки и класса. Чтобы не потеряться, сначала представьте типичную неделю: "
         "короткие городские поездки, школа+секции, дальние выезды на трассу или смешанный ритм?");
    push(out, WizardQuestion::Kind::Detail,
         "Добавьте хотя бы одно жёсткое ограничение цифрами: верх бюджета в рублях (или «до 2 млн»), минимум мест или «только электро / без электро».");
    return out;
  }

  if (big_family && !p.min_trunk) {
    push(out, WizardQuestion::Kind::Lead,
         "Для " + std::to_string(seats) + "+ мест чаще всего упираются не только в ряд сидений, но и в объём багажника при полной загрузке.");
    push(out, WizardQuestion::Kind::Detail,
         "Какой минимальный багажник в литрах вам нужен? Можно числом (например 480) или словами «обычный / большой / очень большой».");
    return out;
  }

  if (big_family && p.min_trunk && !p.body_type && eligible_count > 25) {
    push(out, WizardQuestion::Kind::Lead,
         "С таким числом людей и выбранным объёмом багажа разумно сузить класс кузова — от минивэна до большого кроссовера.");
    push(out, WizardQuestion::Kind::Detail,
         "Что ближе: минивэн (максимум мест и дверей), кроссовер/внедорожник или универсал?");
    return out;
  }

  if (eligible_count > 45) {
    if (!p.max_price && !p.segment) {
      push(out, WizardQuestion::Kind::Lead,
           "При широком бюджете и без класса сегмента остаётся очень много разных по уровню отделки и динамике машин.");
      push(out, WizardQuestion::Kind::Detail,
           "Уточните верх цены в рублях (например 3 000 000 или «до 3 млн») и/или сегмент: «эконом», «массмаркет» или «премиум».");
      return out;
    }
    if (!p.max_price) {
      push(out, WizardQuestion::Kind::Lead, "Цена всё ещё «плавает» в широком диапазоне — это главный рычаг сужения.");
      push(out, WizardQuestion::Kind::Detail,
           "Какой максимальный бюджет в рублях комфортен? (например 2 800 000 или «до 2,8 млн»)");
      return out;
    }
    if (!p.segment) {
      push(out, WizardQuestion::Kind::Lead,
           "В одном ценовом коридоре есть и максимально рациональные комплектации, и более статусные варианты.");
      push(out, WizardQuestion::Kind::Detail,
           "Вас ближе эконом‑сегмент, массмаркет или премиум? Можно одним словом или уточнить любимую марку.");
      return out;
    }
    if (!p.brand_substr && p.segment && *p.segment == "premium") {
      push(out, WizardQuestion::Kind::Lead, "В премиуме важен характер бренда — спокойный комфорт, спортивная динамика или технологичность.");
      push(out, WizardQuestion::Kind::Detail,
           "Есть ли предпочтительная марка (BMW, Mercedes, Audi, Lexus, Volvo и т.д.) или пока без привязки?");
      return out;
    }
    if (!p.body_type) {
      push(out, WizardQuestion::Kind::Lead, "Тип кузова определяет посадку, обзор и то, как машина ведёт себя в городе и на парковке.");
      push(out, WizardQuestion::Kind::Detail,
           big_family ? "Минивэн, кроссовер, универсал, седан или внедорожник — что откликается больше?"
                      : "Седан, хэтчбек, кроссовер, универсал, внедорожник или минивэн?");
      return out;
    }
    if (!p.gearbox) {
      push(out, WizardQuestion::Kind::Lead, "Коробка влияет на тягу в пробках, шум и стоимость обслуживания.");
      push(out, WizardQuestion::Kind::Detail, "Нужны автомат, вариатор, робот или механика — или допустимы несколько типов?");
      return out;
    }
    if (!p.drive) {
      push(out, WizardQuestion::Kind::Lead, "Привод важен для зимы, дачи и стиля езды — передний проще, полный увереннее на скользком.");
      push(out, WizardQuestion::Kind::Detail, "Передний, задний или полный (AWD)?");
      return out;
    }
    if (!p.only_ev.has_value()) {
      push(out, WizardQuestion::Kind::Lead, "В базе есть и электромобили с нулевым расходом по классике, и ДВС с разным аппетитом.");
      push(out, WizardQuestion::Kind::Detail, "Рассматриваете только электро, только ДВС или оба варианта?");
      return out;
    }
    if (!chat_fuel_criterion_settled(p) && (!p.only_ev.has_value() || !*p.only_ev)) {
      push(out, WizardQuestion::Kind::Lead, "Для ДВС расход сильно меняется по мотору и типу кузова.");
      push(out, WizardQuestion::Kind::Detail, "Нужен потолок по расходу (л/100 км)? Например «до 8,5».");
      return out;
    }
    return out;
  }

  if (eligible_count > 15) {
    if (!p.min_power && !power_max_settled(p)) {
      push(out, WizardQuestion::Kind::Lead, "В этом диапазоне остаётся заметный разброс по динамике и уверенности на обгонах.");
      push(out, WizardQuestion::Kind::Detail,
           "Задайте мощность: нижняя планка (например «от 150» или «200 лс»), верхняя («до 300»), диапазон "
           "«150–300 л.с.» — или только одну границу.");
      return out;
    }
    if (p.min_power && !power_max_settled(p)) {
      push(out, WizardQuestion::Kind::Lead, "С одной стороны хочется запас по мощности, с другой — не переплачивать за лишние л.с.");
      push(out, WizardQuestion::Kind::Detail,
           "Какая верхняя планка по мощности (л.с.)? Например «до 280», «300 лс» — или «без верха» / «только нижнюю границу».");
      return out;
    }
    if (!chat_fuel_criterion_settled(p) && (!p.only_ev.has_value() || !*p.only_ev)) {
      push(out, WizardQuestion::Kind::Lead, "Стоимость владения заметно зависит от среднего расхода.");
      push(out, WizardQuestion::Kind::Detail, "Укажите максимальный расход л/100 км или напишите «не критично».");
      return out;
    }
    if (!p.min_trunk && p.min_seats && *p.min_seats >= 4) {
      push(out, WizardQuestion::Kind::Lead, "Даже при пяти местах багажники сильно отличаются по форме и объёму.");
      push(out, WizardQuestion::Kind::Detail, "Какой минимальный объём багажника (л) или опишите словами?");
      return out;
    }
    if (!p.brand_substr) {
      push(out, WizardQuestion::Kind::Lead, "На этом этапе можно добавить предпочтение по марке — база большая, бренд сильно сужает.");
      push(out, WizardQuestion::Kind::Detail, "Есть любимая марка или страна сборки, на которую ориентируетесь?");
      return out;
    }
    return out;
  }

  if (eligible_count >= 1) {
    if (!min_price_floor_settled(p) && p.max_price) {
      push(out, WizardQuestion::Kind::Lead, "Чтобы не упираться в самые дешёвые комплектации у потолка бюджета, иногда задают и нижнюю границу.");
      push(out, WizardQuestion::Kind::Detail,
           "Нужен ли нижний порог цены в рублях или оставляем любой вариант до максимума?");
      return out;
    }
    if (p.min_power && !power_max_settled(p)) {
      push(out, WizardQuestion::Kind::Lead, "С одной стороны хочется запас по мощности, с другой — не переплачивать за лишние л.с.");
      push(out, WizardQuestion::Kind::Detail,
           "Какая верхняя планка по мощности (л.с.)? Например «до 280», «300 лс» — или «без верха» / «только нижнюю границу».");
      return out;
    }
    if (!chat_fuel_criterion_settled(p) && (!p.only_ev.has_value() || !*p.only_ev)) {
      push(out, WizardQuestion::Kind::Detail, "Осталось немного вариантов — уточните желаемый максимум расхода (л/100 км), если это важно.");
      return out;
    }
    push(out, WizardQuestion::Kind::Lead,
         "Критериев уже достаточно для осмысленного топа — можно смотреть таблицу рекомендаций и точечно крутить 1–2 параметра.");
    push(out, WizardQuestion::Kind::Detail,
         "Если хотите ещё глубже: напишите в чате приоритет «комфорт / экономия / динамика / надёжность» — добавим в следующих версиях.");
  }

  return out;
}

int estimated_annual_tco_rub(const CarRow& c, int km_per_year) {
  if (km_per_year < 3000) km_per_year = 3000;
  if (km_per_year > 60000) km_per_year = 60000;
  double rub = 0.0;
  if (c.is_electric) {
    rub += (km_per_year / 100.0) * 20.0 * 6.8;
    rub += c.price * 0.038;
    rub += std::min(12000.0, c.power * 12.0);
  } else {
    rub += (km_per_year / 100.0) * c.fuel_consumption * 72.0;
    double ins_k = (c.segment == "premium") ? 0.050 : (c.segment == "economy" ? 0.032 : 0.042);
    rub += c.price * ins_k;
    rub += std::min(75000.0, c.power * 42.0);
  }
  long long tot = static_cast<long long>(std::llround(rub));
  if (tot < 35000) tot = 35000;
  if (tot > 950000) tot = 950000;
  if (tot > INT_MAX) return INT_MAX;
  return static_cast<int>(tot);
}

static int body_clearance_proxy_mm(const std::string& body) {
  if (body == "suv") return 208;
  if (body == "crossover") return 188;
  if (body == "van") return 168;
  if (body == "wagon") return 156;
  if (body == "liftback") return 148;
  if (body == "hatchback") return 142;
  return 136;
}

SnowIndex snow_index_for_car(const CarRow& c, const Preferences& prefs) {
  SnowIndex out;
  out.clearance_proxy_mm = body_clearance_proxy_mm(c.body_type);
  double raw = 0.0;
  raw += std::clamp((out.clearance_proxy_mm - 125) / 22.0, 0.0, 4.2);
  if (c.drive == "awd")
    raw += 3.3;
  else if (c.drive == "fwd")
    raw += 1.7;
  else
    raw += 1.1;
  if (c.gearbox == "mt")
    raw += 1.35;
  else if (c.gearbox == "robot")
    raw += 1.05;
  else if (c.gearbox == "at")
    raw += 1.0;
  else
    raw += 0.85;
  if (c.body_type == "suv" || c.body_type == "crossover")
    raw += 1.25;
  else if (c.body_type == "wagon" || c.body_type == "van")
    raw += 0.45;
  if (prefs.trip_profile && *prefs.trip_profile == "dacha") raw += 0.55;
  if (prefs.trip_profile && *prefs.trip_profile == "family") raw += 0.25;
  raw = std::clamp(raw * (10.0 / 11.4), 0.0, 10.0);
  out.score_0_to_10 = static_cast<int>(std::lround(raw));
  if (out.score_0_to_10 < 0) out.score_0_to_10 = 0;
  if (out.score_0_to_10 > 10) out.score_0_to_10 = 10;
  if (out.score_0_to_10 <= 3)
    out.tier_ru = "скорее город и ровный асфальт";
  else if (out.score_0_to_10 <= 5)
    out.tier_ru = "обычная зима, без «по сугробам»";
  else if (out.score_0_to_10 <= 7)
    out.tier_ru = "регион и укатанный снег — нормально";
  else
    out.tier_ru = "сильный задел на плохую погоду и грунт";
  std::ostringstream ex;
  ex << "Прокси-клиренс ≈ " << out.clearance_proxy_mm << " мм по типу кузова «" << c.body_type << "»";
  if (c.drive == "awd")
    ex << ", полный привод";
  else if (c.drive == "fwd")
    ex << ", передний привод";
  else
    ex << ", задний привод";
  ex << ", КПП «" << c.gearbox << "»";
  if (prefs.trip_profile && *prefs.trip_profile == "dacha") ex << "; сценарий «дача» в фильтрах — чуть повышаем ожидание от проходимости";
  ex << ". Это не реальный замер клиренса, а игровая метрика для демо.";
  out.explain_ru = ex.str();
  return out;
}

LifeWithCar5y estimate_life_with_car_5y(const CarRow& c, int km_per_year) {
  LifeWithCar5y L;
  if (km_per_year < 3000) km_per_year = 3000;
  if (km_per_year > 60000) km_per_year = 60000;
  L.km_per_year = km_per_year;
  L.running_year_rub = estimated_annual_tco_rub(c, km_per_year);
  L.tires_year_rub = (c.segment == "premium") ? 16500 : 11500;
  int base_svc = 24000;
  if (c.segment == "economy") base_svc = 20000;
  if (c.segment == "premium") base_svc = 42000;
  L.service_year_rub = base_svc + std::min(14000, c.power / 3);
  double dep_share = 0.46;
  if (c.segment == "economy") dep_share = 0.54;
  if (c.segment == "premium") dep_share = 0.34;
  L.depreciation_5y_rub = static_cast<int>(std::llround(double(c.price) * dep_share));
  const int ops_y = L.running_year_rub + L.service_year_rub + L.tires_year_rub;
  L.operating_5y_rub = ops_y * 5;
  L.total_5y_rub = L.depreciation_5y_rub + L.operating_5y_rub;
  if (L.total_5y_rub < 1) L.total_5y_rub = 1;
  L.monthly_equiv_rub = (L.total_5y_rub + 59) / 60;
  std::ostringstream cm;
  cm << "Оценка «в среднем по рынку»: за 5 лет и ~" << (km_per_year * 5 / 1000) << " тыс. км пробега заложены ";
  cm << "потеря остаточной стоимости ≈ " << (int)std::lround(dep_share * 100) << "% от цены каталога, ";
  cm << "ежегодно — расходники как в TCO/год (топливо или электроэнергия, грубый аналог ОСАГО+налога), ";
  cm << "ТО/расходники ≈ " << L.service_year_rub << " ₽/год, резина (две сезонные линейки с амортизацией) ≈ ";
  cm << L.tires_year_rub << " ₽/год. КАСКО, кредит и поломки не включали — только ориентир для сравнения вариантов.";
  L.comment_ru = cm.str();
  return L;
}

std::string chat_insight_addon_ru(const std::string& user_message, const CarRow& top_car, const Preferences& prefs) {
  const std::string t = lower(trim(user_message));
  if (t.empty()) return "";
  auto want_life = [&]() {
    return t.find("5 лет") != std::string::npos || t.find("пять лет") != std::string::npos ||
           (t.find("жизн") != std::string::npos && t.find("машин") != std::string::npos) ||
           (t.find("полн") != std::string::npos && t.find("стоим") != std::string::npos) ||
           (t.find("стоимост") != std::string::npos && t.find("владен") != std::string::npos) ||
           t.find("осаго") != std::string::npos || t.find("техобслуж") != std::string::npos ||
           t.find("резин") != std::string::npos || t.find("руб/мес") != std::string::npos ||
           t.find("руб в месяц") != std::string::npos || t.find("в месяц") != std::string::npos;
  };
  auto want_snow = [&]() {
    return t.find("снежн") != std::string::npos || t.find("зим") != std::string::npos ||
           t.find("сугроб") != std::string::npos || t.find("гололёд") != std::string::npos ||
           t.find("гололед") != std::string::npos ||
           t.find("укатан") != std::string::npos || t.find("ледя") != std::string::npos;
  };
  const bool wl = want_life();
  const bool ws = want_snow();
  if (!wl && !ws) return "";
  std::ostringstream o;
  o << "\n\n— Дополнение по вашей формулировке (лидер топа «" << top_car.name << "»):\n";
  if (wl) {
    const auto L = estimate_life_with_car_5y(top_car, 15000);
    o << "Жизнь с машиной ~5 лет: эквивалент ≈ " << L.monthly_equiv_rub
      << " ₽/мес при усреднении (амортизация + эксплуатация). " << L.comment_ru;
  }
  if (ws) {
    const auto S = snow_index_for_car(top_car, prefs);
    if (wl) o << "\n\n";
    o << "Снежный индекс: " << S.score_0_to_10 << "/10 — " << S.tier_ru << ". " << S.explain_ru;
  }
  return o.str();
}

ExplainableMatch explainable_match(const CarRow& c, const Preferences& p) {
  ExplainableMatch m;
  if (p.max_price && c.price <= *p.max_price) m.strengths.push_back("цена укладывается в верх бюджета");
  if (p.min_price && c.price >= *p.min_price) m.strengths.push_back("цена не ниже заданного минимума");
  if (p.min_seats && c.seats >= *p.min_seats) m.strengths.push_back("мест достаточно");
  if (p.min_trunk && c.trunk >= *p.min_trunk) m.strengths.push_back("багажник по объёму подходит");
  if (p.min_power && c.power >= *p.min_power) m.strengths.push_back("мощность не ниже порога");
  if (p.max_power && c.power <= *p.max_power) m.strengths.push_back("мощность не выше потолка");
  if (p.max_fuel && c.fuel_consumption <= *p.max_fuel + 1e-6) m.strengths.push_back("расход в допуске");
  if (p.body_type && c.body_type == *p.body_type) m.strengths.push_back("кузов совпадает с фильтром");
  if (p.gearbox && c.gearbox == *p.gearbox) m.strengths.push_back("КПП совпадает");
  if (p.drive && c.drive == *p.drive) m.strengths.push_back("привод совпадает");
  if (p.segment && c.segment == *p.segment) m.strengths.push_back("сегмент совпадает");
  if (p.brand_substr && brand_matches(c, *p.brand_substr)) m.strengths.push_back("марка совпадает с запросом");

  if (p.fuel_ignore_cap.has_value() && *p.fuel_ignore_cap) m.preferences.push_back("расход в запросе не ограничивали");
  if (p.min_price_floor_opt_out.has_value() && *p.min_price_floor_opt_out)
    m.preferences.push_back("нижний порог цены не задавали");
  if (p.drive_soft && c.drive == *p.drive_soft) m.preferences.push_back("есть желательный привод");
  if (p.body_soft && c.body_type == *p.body_soft) m.preferences.push_back("есть желательный тип кузова");
  if (p.gearbox_soft && c.gearbox == *p.gearbox_soft) m.preferences.push_back("есть желательная КПП");
  if (p.soft_min_seats && c.seats >= *p.soft_min_seats) m.preferences.push_back("выполнено пожелание по местам");

  if (p.max_price && c.price > (static_cast<long long>(*p.max_price) * 9 / 10))
    m.cautions.push_back("цена близко к потолку бюджета");
  if (p.max_fuel && c.fuel_consumption > *p.max_fuel * 0.92) m.cautions.push_back("расход близок к лимиту");
  if (p.trip_profile && *p.trip_profile == "family" && c.seats < 6)
    m.cautions.push_back("для семейного сценария мест могло бы быть больше");
  if (p.trip_profile && *p.trip_profile == "dacha" && c.drive != "awd")
    m.cautions.push_back("для дачи/грунта часто смотрят полный привод");
  return m;
}

std::string explain_match_ru(const CarRow& c, const Preferences& p) {
  const auto m = explainable_match(c, p);
  std::ostringstream o;
  auto join = [](const std::vector<std::string>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) {
      if (i) s += " · ";
      s += v[i];
    }
    return s;
  };
  if (!m.strengths.empty()) o << "Плюсы: " << join(m.strengths) << ". ";
  if (!m.preferences.empty()) o << "Желательное: " << join(m.preferences) << ". ";
  if (!m.cautions.empty()) o << "На заметку: " << join(m.cautions) << ".";
  std::string out = trim(o.str());
  if (out.size() > 420) out = out.substr(0, 417) + "...";
  if (out.empty()) out = "Проходит по жёстким фильтрам; детали смотрите в таблице.";
  return out;
}

TcoBreakdown tco_breakdown(const CarRow& c, int km_per_year) {
  TcoBreakdown b;
  if (km_per_year < 3000) km_per_year = 3000;
  if (km_per_year > 60000) km_per_year = 60000;
  b.km_per_year = km_per_year;
  if (c.is_electric) {
    b.fuel_energy_year_rub =
        static_cast<int>(std::llround((double(km_per_year) / 100.0) * 20.0 * 6.8));
    b.tax_insurance_year_rub =
        static_cast<int>(std::llround(c.price * 0.038 + std::min(12000.0, double(c.power) * 12.0)));
  } else {
    b.fuel_energy_year_rub =
        static_cast<int>(std::llround((double(km_per_year) / 100.0) * c.fuel_consumption * 72.0));
    const double ins_k = (c.segment == "premium") ? 0.050 : (c.segment == "economy" ? 0.032 : 0.042);
    b.tax_insurance_year_rub =
        static_cast<int>(std::llround(c.price * ins_k + std::min(75000.0, double(c.power) * 42.0)));
  }
  b.total_year_rub = estimated_annual_tco_rub(c, km_per_year);
  return b;
}

static void append_kw(std::ostringstream& q, const std::string& kw) {
  if (kw.empty()) return;
  q << ' ' << kw;
}

static std::string strip_leading_brand(const std::string& brand, std::string text) {
  text = trim(std::move(text));
  if (brand.empty() || text.empty()) return text;
  const std::string lb = lower(brand);
  std::string lt = lower(text);
  if (lt.size() >= lb.size() && lt.compare(0, lb.size(), lb) == 0) {
    text = trim(text.substr(brand.size()));
    while (!text.empty() && (text.front() == ' ' || text.front() == '-' || text.front() == '\t')) text.erase(text.begin());
  }
  return text;
}

std::string car_display_title(const CarRow& car) {
  std::string rest = strip_leading_brand(car.brand, car.name);
  if (rest.empty()) return car.brand;
  const std::string lb = lower(car.brand);
  const std::string lr = lower(rest);
  if (lr.size() >= lb.size() && lr.compare(0, lb.size(), lb) == 0) return rest;
  return car.brand + " " + rest;
}

static std::string avito_brand_label(const std::string& brand) {
  if (brand == "Mercedes") return "Mercedes-Benz";
  if (brand == "Lada") return "LADA";
  if (brand == "UAZ") return "УАЗ";
  return brand;
}

// Как модель ищут на Авито (марка в запросе отдельно).
static std::string avito_model_phrase(const std::string& brand, const std::string& model) {
  struct Row {
    const char* b;
    const char* m;
    const char* avito;
  };
  static const Row k[] = {
      {"BMW", "320i", "3 серии"},
      {"BMW", "520i", "5 серии"},
      {"Mercedes", "C200", "C-класс"},
      {"Mercedes", "E220", "E-класс"},
      {"Mercedes", "V-Class", "V-класс"},
      {"Mercedes", "Sprinter", "Sprinter"},
      {"OMODA", "Omoda 5", "C5"},
      {"Chery", "Tiggo", "Tiggo 7"},
      {"Lada", "Vesta", "Vesta"},
      {"Lada", "Granta", "Granta"},
      {"Lada", "Niva Travel", "Niva Travel"},
      {"Volkswagen", "Multivan", "Multivan"},
      {"Kia", "Carnival", "Carnival"},
      {"Toyota", "Alphard", "Alphard"},
      {"Haval", "F7", "F7"},
      {"Haval", "F7x", "F7x"},
      {"Chery", "Tiggo 8", "Tiggo 8"},
      {"Chery", "Tiggo 8 Pro", "Tiggo 8 Pro"},
      {"Renault", "Megane E-Tech", "Megane E-Tech"},
      {"Toyota", "RAV4 Hybrid", "RAV4"},
      {"Lada", "Vesta SW", "Vesta"},
  };
  for (const auto& r : k) {
    if (brand == r.b && model == r.m) return r.avito;
  }
  return model;
}

// Год в конце строки вида «Astra 2018» — для сужения выдачи на Авито.
static std::optional<int> avito_trailing_year_token(const std::string& s) {
  const auto sp = s.rfind(' ');
  if (sp == std::string::npos || sp + 5 != s.size()) return std::nullopt;
  for (size_t i = sp + 1; i < s.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(s[i]))) return std::nullopt;
  }
  try {
    const int y = std::stoi(s.substr(sp + 1));
    if (y >= 1990 && y <= 2035) return y;
  } catch (...) {
  }
  return std::nullopt;
}

static const char* avito_body_ru(const std::string& body) {
  if (body == "sedan") return "седан";
  if (body == "hatchback") return "хэтчбек";
  if (body == "crossover") return "кроссовер";
  if (body == "wagon") return "универсал";
  if (body == "suv") return "внедорожник";
  if (body == "van") return "минивэн";
  if (body == "liftback") return "лифтбек";
  return nullptr;
}

static void avito_append_gearbox(std::ostringstream& q, const std::string& gb) {
  if (gb == "at")
    append_kw(q, "автомат");
  else if (gb == "mt")
    append_kw(q, "механика");
  else if (gb == "cvt")
    append_kw(q, "вариатор");
  else if (gb == "robot")
    append_kw(q, "робот");
}

static void avito_append_drive(std::ostringstream& q, const std::string& dr) {
  if (dr == "awd")
    append_kw(q, "полный привод");
  else if (dr == "fwd")
    append_kw(q, "передний привод");
  else if (dr == "rwd")
    append_kw(q, "задний привод");
}

std::string avito_search_url(const CarRow& car, const Preferences& prefs) {
  std::ostringstream qtext;
  qtext << avito_brand_label(car.brand);

  std::string nm = strip_leading_brand(car.brand, car.name);
  std::string model_part;
  std::optional<int> year;
  if (!nm.empty()) {
    if (const auto y = avito_trailing_year_token(nm)) {
      year = y;
      model_part = trim(nm.substr(0, nm.rfind(' ')));
    } else {
      model_part = nm;
    }
  }
  model_part = avito_model_phrase(car.brand, model_part);
  if (!model_part.empty()) qtext << ' ' << model_part;
  if (year) qtext << ' ' << *year;

  if (const char* br = avito_body_ru(car.body_type)) append_kw(qtext, br);

  if (car.seats >= 6) {
    if (car.seats == 6)
      append_kw(qtext, "6 мест");
    else if (car.seats == 7)
      append_kw(qtext, "7 мест");
    else if (car.seats >= 8)
      append_kw(qtext, "8 мест");
  }

  // В запросе — фактические поля строки каталога (а не фильтр пользователя), иначе на Авито
  // уезжаем на другую модификацию при расхождении фильтра и карточки.
  avito_append_gearbox(qtext, car.gearbox);
  avito_append_drive(qtext, car.drive);

  if (car.is_electric) append_kw(qtext, "электро");

  std::string qstr = trim(qtext.str());
  if (qstr.size() > 200) qstr = qstr.substr(0, 200);

  long long pmin =
      prefs.min_price ? static_cast<long long>(*prefs.min_price)
                      : std::max(80'000LL, static_cast<long long>(static_cast<double>(car.price) * 0.76));
  long long pmax = prefs.max_price ? static_cast<long long>(*prefs.max_price)
                                   : std::max(pmin + 250'000LL, static_cast<long long>(static_cast<double>(car.price) * 1.24));
  if (pmax <= pmin) pmax = pmin + 400'000LL;

  std::ostringstream u;
  u << "https://www.avito.ru/all/avtomobili?q=" << http::url_encode(qstr);
  u << "&pmin=" << pmin << "&pmax=" << pmax;
  return u.str();
}
