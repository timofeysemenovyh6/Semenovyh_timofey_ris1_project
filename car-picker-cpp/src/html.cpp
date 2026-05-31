#include "html.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace html {

std::string escape(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out.push_back(c); break;
    }
  }
  return out;
}

static std::string badge(const std::string& text) { return "<span class=\"badge\">" + escape(text) + "</span>"; }

static std::string format_rub(int v) {
  long long x = v;
  bool neg = x < 0;
  if (neg) x = -x;
  std::string s = std::to_string(x);
  std::string o;
  int cnt = 0;
  for (int i = static_cast<int>(s.size()) - 1; i >= 0; --i) {
    if (cnt && cnt % 3 == 0) o.push_back(' ');
    o.push_back(s[static_cast<size_t>(i)]);
    cnt++;
  }
  std::reverse(o.begin(), o.end());
  return (neg ? "-" : "") + o + " руб.";
}

static std::string car_title_link(const CarRow& c, const Preferences& prefs) {
  const std::string av = avito_search_url(c, prefs);
  const std::string shown = car_display_title(c);
  std::ostringstream o;
  o << "<a class=\"extLink\" href=\"" << escape(av) << "\" target=\"_blank\" rel=\"noopener noreferrer\" "
    << "title=\"Поиск на Авито по этой строке каталога (марка, модель, год, кузов, КПП, привод, цена)\">" << escape(shown)
    << "<span class=\"extMark\" aria-hidden=\"true\"> ↗</span></a>";
  return o.str();
}

static std::string side_nav_link(const std::string& key, const std::string& href, const std::string& label,
                                  const std::string& active) {
  std::string cls = "sideLink";
  if (!key.empty() && key == active) cls += " isActive";
  return "<a class=\"" + cls + "\" href=\"" + escape(href) + "\">" + escape(label) + "</a>";
}

std::string layout(const std::string& title, const std::string& body, const std::string& nav_active) {
  std::ostringstream o;
  o << "<!doctype html><html lang=\"ru\"><head>"
    << "<meta charset=\"utf-8\"/>"
    << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>"
    << "<title>" << escape(title) << "</title>"
    << "<link rel=\"stylesheet\" href=\"/static/style.css\"/>"
    << "</head><body class=\"appBg appShell\">"
    << "<aside class=\"sidebar\" aria-label=\"Разделы сайта\">"
    << "<div class=\"sidebarTop\">"
    << "<a class=\"sideLogo\" href=\"/\">"
    << "<span class=\"sideLogoMark\" aria-hidden=\"true\"></span>"
    << "<span class=\"sideLogoText\"><span class=\"sideLogoName\">AutoSelect</span>"
    << "<span class=\"sideLogoSub\">подбор автомобилей</span></span>"
    << "</a></div>"
    << "<nav class=\"sideNav\">"
    << side_nav_link("home", "/", "Главная", nav_active)
    << side_nav_link("wizard", "/wizard", "Мастер‑подбор", nav_active)
    << side_nav_link("chat", "/chat", "Чат‑помощник", nav_active)
    << side_nav_link("compare", "/compare", "Сравнение", nav_active)
    << side_nav_link("favorites", "/favorites", "Избранное", nav_active)
    << side_nav_link("extras", "/extras", "Лаборатория", nav_active)
    << side_nav_link("tools", "/client-tools", "Справочник и отчёт", nav_active)
    << side_nav_link("admin", "/admin/cars", "База авто", nav_active)
    << "<span class=\"sideNavSep\" role=\"presentation\"></span>"
    << "<a class=\"sideLink sideLink--minor\" href=\"/#how\">Этапы работы</a>"
    << "</nav>"
    << "<div class=\"sidebarFoot\"><span class=\"sideMeta\">C++ · SQLite · локально</span></div>"
    << "</aside>"
    << "<div class=\"mainCol\">"
    << "<main class=\"content\">" << body << "</main>"
    << "<footer class=\"footer\"><div class=\"contentInner\">"
    << "Локальный сервис подбора автомобилей."
    << "</div></footer>"
    << "</div></body></html>";
  return o.str();
}

std::string home_page() {
  std::ostringstream b;
  b << "<!doctype html><html lang=\"ru\"><head>"
    << "<meta charset=\"utf-8\"/>"
    << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>"
    << "<title>Авто‑подбор</title>"
    << "<link rel=\"stylesheet\" href=\"/static/style.css\"/>"
    << "</head><body class=\"landingV2 appShell\">"
    << "<aside class=\"sidebar\" aria-label=\"Разделы сайта\">"
    << "<div class=\"sidebarTop\">"
    << "<a class=\"sideLogo\" href=\"/\">"
    << "<span class=\"sideLogoMark\" aria-hidden=\"true\"></span>"
    << "<span class=\"sideLogoText\"><span class=\"sideLogoName\">AutoSelect</span>"
    << "<span class=\"sideLogoSub\">подбор автомобилей</span></span>"
    << "</a></div>"
    << "<nav class=\"sideNav\">"
    << side_nav_link("home", "/", "Главная", "home")
    << side_nav_link("wizard", "/wizard", "Мастер‑подбор", "home")
    << side_nav_link("chat", "/chat", "Чат‑помощник", "home")
    << side_nav_link("compare", "/compare", "Сравнение", "home")
    << side_nav_link("favorites", "/favorites", "Избранное", "home")
    << side_nav_link("extras", "/extras", "Лаборатория", "home")
    << side_nav_link("tools", "/client-tools", "Справочник и отчёт", "home")
    << side_nav_link("admin", "/admin/cars", "База авто", "home")
    << "<span class=\"sideNavSep\" role=\"presentation\"></span>"
    << "<a class=\"sideLink sideLink--minor\" href=\"#services\">Услуги</a>"
    << "<a class=\"sideLink sideLink--minor\" href=\"#how\">Этапы работы</a>"
    << "<a class=\"sideLink sideLink--minor\" href=\"#reviews\">Отзывы</a>"
    << "<a class=\"sideLink sideLink--minor\" href=\"#about\">О компании</a>"
    << "</nav>"
    << "<div class=\"sidebarFoot\">"
    << "<a class=\"btn btn--outline btn--block\" href=\"/chat\">Связаться</a>"
    << "<span class=\"sideMeta\">C++ · SQLite</span></div>"
    << "</aside>"
    << "<div class=\"mainCol mainCol--landing\">"
    << "<main class=\"lHero\">"
    << "<div class=\"lWrap lHeroGrid\">"
    << "<section class=\"lLeft\">"
    << "<h1 class=\"lH1\">Подберём автомобиль<br/><span class=\"lAccent\">под ваши задачи</span></h1>"
    << "<p class=\"lLead\">Поможем найти автомобиль по критериям и бюджету — быстро и без лишних хлопот.</p>"
    << "<div class=\"lProps\">"
    << "<div class=\"lProp\"><span class=\"lIcon\" aria-hidden=\"true\">🧭</span><span>Тщательный<br/>подбор</span></div>"
    << "<div class=\"lProp\"><span class=\"lIcon\" aria-hidden=\"true\">🎛</span><span>Индивидуальный<br/>подход</span></div>"
    << "</div>"
    << "<div class=\"lCtas\">"
    << "<a class=\"btn btn--solid btn--lg\" href=\"/wizard\">Подобрать автомобиль</a>"
    << "<a class=\"btn btn--ghost btn--lg\" href=\"#how\">"
    << "<span class=\"lPlay\" aria-hidden=\"true\"></span>Как мы работаем"
    << "</a>"
    << "</div>"
    << "</section></div></main>"
    << "<section class=\"lSection\" id=\"services\"><div class=\"lWrap\"><h2>Услуги</h2><p>Место под описание услуг (можно заполнить позже).</p></div></section>"
    << "<section class=\"lSection\" id=\"how\"><div class=\"lWrap\"><h2>Этапы работы</h2><p>1) Уточняем требования → 2) задаём доп. вопросы → 3) выдаём топ вариантов.</p></div></section>"
    << "<section class=\"lSection\" id=\"reviews\"><div class=\"lWrap\"><h2>Отзывы</h2><p>Место под отзывы.</p></div></section>"
    << "<section class=\"lSection\" id=\"about\"><div class=\"lWrap\"><h2>О компании</h2><p>Место под описание.</p></div></section>"
    << "</div></body></html>";
  return b.str();
}

std::string cars_list_page(const std::vector<CarRow>& cars, const std::string& flash) {
  std::ostringstream b;
  b << "<h1>База автомобилей</h1>";
  if (!flash.empty()) b << "<div class=\"flash\">" << escape(flash) << "</div>";
  b << "<div class=\"actions\"><a class=\"btn btn--solid btn--icon\" href=\"/admin/cars/new\"><span class=\"btn__ico\" aria-hidden=\"true\">+</span>Добавить авто</a></div>";
  b << "<div class=\"tableWrap\"><table><thead><tr>"
    << "<th>ID</th><th>Название</th><th>Марка</th><th>Цена</th><th>Кузов</th><th>КПП</th><th>Привод</th><th>Мест</th><th>Багажник</th><th>Мощн.</th><th>Vmax</th><th>Эл.</th><th>Сегм.</th><th></th>"
    << "</tr></thead><tbody>";
  for (const auto& c : cars) {
    b << "<tr>"
      << "<td>" << c.id << "</td>"
      << "<td>" << escape(c.name) << "</td>"
      << "<td>" << escape(c.brand) << "</td>"
      << "<td>" << format_rub(c.price) << "</td>"
      << "<td>" << badge(c.body_type) << "</td>"
      << "<td>" << badge(c.gearbox) << "</td>"
      << "<td>" << badge(c.drive) << "</td>"
      << "<td>" << c.seats << "</td>"
      << "<td>" << c.trunk << "</td>"
      << "<td>" << c.power << "</td>"
      << "<td>" << (c.max_speed_kmh > 0 ? c.max_speed_kmh : default_max_speed_kmh(c.power, c.is_electric, c.body_type))
      << "</td>"
      << "<td>" << (c.is_electric ? "да" : "—") << "</td>"
      << "<td>" << badge(c.segment) << "</td>"
      << "<td class=\"right\">"
      << "<a class=\"btn btn--outline btn--sm\" href=\"/admin/cars/edit?id=" << c.id << "\">Изменить</a>"
      << "<form class=\"inline\" method=\"POST\" action=\"/admin/cars/delete\">"
      << "<input type=\"hidden\" name=\"id\" value=\"" << c.id << "\"/>"
      << "<button class=\"btn btn--danger btn--sm\" type=\"submit\">Удалить</button>"
      << "</form>"
      << "</td>"
      << "</tr>";
  }
  b << "</tbody></table></div>";
  return layout("База авто", b.str(), "admin");
}

static std::string input(const std::string& label, const std::string& name, const std::string& value) {
  const std::string id = "fld_" + name;
  std::ostringstream o;
  o << "<div class=\"fieldFloat\">"
    << "<input class=\"fieldFloat__input\" id=\"" << escape(id) << "\" name=\"" << escape(name) << "\" value=\""
    << escape(value) << "\" placeholder=\" \"/>"
    << "<label class=\"fieldFloat__label\" for=\"" << escape(id) << "\">" << escape(label) << "</label>"
    << "</div>";
  return o.str();
}

std::string car_form_page(const std::string& title, const std::string& action, const CarRow* car, const std::string& error) {
  CarRow c;
  if (car) c = *car;
  std::ostringstream b;
  b << "<h1>" << escape(title) << "</h1>";
  if (!error.empty()) b << "<div class=\"flash warn\">" << escape(error) << "</div>";
  b << "<form class=\"card\" method=\"POST\" action=\"" << escape(action) << "\">";
  if (car) b << "<input type=\"hidden\" name=\"id\" value=\"" << c.id << "\"/>";
  b << "<div class=\"grid2\">"
    << input("Название", "name", c.name)
    << input("Марка (кратко)", "brand", c.brand)
    << input("Цена (руб, целое число)", "price", std::to_string(c.price))
    << input("Расход (л/100км)", "fuel_consumption", std::to_string(c.fuel_consumption))
    << input("Сегмент (economy/mass/premium)", "segment", c.segment)
    << input("Тип кузова (sedan/hatchback/crossover/suv/van/...) ", "body_type", c.body_type)
    << input("Коробка (at/mt/cvt/robot)", "gearbox", c.gearbox)
    << input("Привод (fwd/rwd/awd)", "drive", c.drive)
    << input("Места", "seats", std::to_string(c.seats))
    << input("Багажник (л)", "trunk", std::to_string(c.trunk))
    << input("Мощность (л.с.)", "power", std::to_string(c.power))
    << input("Макс. скорость (км/ч, 0 = авто)", "max_speed", c.max_speed_kmh > 0 ? std::to_string(c.max_speed_kmh) : "")
    << "</div>"
    << "<div class=\"fieldCheck\"><label class=\"checkLbl\"><input type=\"checkbox\" name=\"is_electric\" value=\"1\""
    << (c.is_electric ? " checked" : "") << "/> Электромобиль (расход можно оставить 0)</label></div>"
    << "<div class=\"actions\">"
    << "<button class=\"btn btn--solid\" type=\"submit\">Сохранить</button>"
    << "<a class=\"btn btn--outline\" href=\"/admin/cars\">Отмена</a>"
    << "</div>"
    << "</form>";
  return layout(title, b.str(), "admin");
}

static std::string prefs_badges(const Preferences& p) {
  std::ostringstream o;
  o << "<div class=\"badges\">";
  if (p.min_price) o << badge("от " + format_rub(*p.min_price));
  if (p.max_price) o << badge("до " + format_rub(*p.max_price));
  if (p.min_price_floor_opt_out.has_value() && *p.min_price_floor_opt_out) o << badge("без нижней границы цены");
  if (p.max_fuel) o << badge("расход ≤ " + std::to_string(*p.max_fuel));
  if (p.fuel_ignore_cap.has_value() && *p.fuel_ignore_cap) o << badge("расход не ограничивали");
  if (p.min_seats) o << badge("мест ≥ " + std::to_string(*p.min_seats));
  if (p.min_trunk) o << badge("багажник ≥ " + std::to_string(*p.min_trunk));
  if (p.min_power) o << badge("мощн. ≥ " + std::to_string(*p.min_power));
  if (p.max_power) o << badge("мощн. ≤ " + std::to_string(*p.max_power));
  if (p.body_type) o << badge("кузов " + *p.body_type);
  if (p.gearbox) o << badge("кпп " + *p.gearbox);
  if (p.drive) o << badge("привод " + *p.drive);
  if (p.brand_substr) o << badge("марка " + *p.brand_substr);
  if (p.segment) o << badge("класс " + *p.segment);
  if (p.only_ev.has_value()) o << badge(*p.only_ev ? "только электро" : "без электро");
  if (p.trip_profile) o << badge("сценарий " + *p.trip_profile);
  if (p.drive_soft) o << badge("желат. привод " + *p.drive_soft);
  if (p.body_soft) o << badge("желат. кузов " + *p.body_soft);
  if (p.gearbox_soft) o << badge("желат. кпп " + *p.gearbox_soft);
  if (p.soft_min_seats) o << badge("желат. мест ≥ " + std::to_string(*p.soft_min_seats));
  if (p.brand_exclude_substr) o << badge("исключить " + *p.brand_exclude_substr);
  o << "</div>";
  return o.str();
}

static std::string energy_radios(const Preferences& p) {
  const std::string ca = !p.only_ev.has_value() ? " checked" : "";
  const std::string ce = (p.only_ev.has_value() && *p.only_ev) ? " checked" : "";
  const std::string ci = (p.only_ev.has_value() && !*p.only_ev) ? " checked" : "";
  std::ostringstream o;
  o << "<div class=\"fieldRadios\"><div class=\"lbl\">Электромобили</div>"
    << "<label class=\"radioLbl\"><input type=\"radio\" name=\"energy\" value=\"any\"" << ca << "/> любые</label> "
    << "<label class=\"radioLbl\"><input type=\"radio\" name=\"energy\" value=\"ev\"" << ce << "/> только электро</label> "
    << "<label class=\"radioLbl\"><input type=\"radio\" name=\"energy\" value=\"ice\"" << ci << "/> без электро</label></div>";
  return o.str();
}

static bool fav_contains_id(const std::vector<int64_t>& fav, int64_t id) {
  return std::find(fav.begin(), fav.end(), id) != fav.end();
}

static std::string fav_heart_cell(int64_t id, const std::vector<int64_t>& fav, const std::string& next_path) {
  const bool on = fav_contains_id(fav, id);
  std::ostringstream o;
  o << "<td class=\"tdHeart\">"
    << "<form method=\"POST\" action=\"/favorite/toggle\" class=\"heartForm\">"
    << "<input type=\"hidden\" name=\"id\" value=\"" << id << "\"/>"
    << "<input type=\"hidden\" name=\"next\" value=\"" << escape(next_path) << "\"/>"
    << "<button type=\"submit\" class=\"heartBtn" << (on ? " heartBtn--on" : "")
    << "\" aria-pressed=\"" << (on ? "true" : "false") << "\" title=\"" << (on ? "Убрать из избранного" : "В избранное") << "\">"
    << (on ? "♥" : "♡") << "</button></form></td>";
  return o.str();
}

static std::string reco_list(const std::vector<ScoredCar>& recs, const Preferences& prefs, const std::vector<int64_t>& favorite_ids,
                             const std::string& heart_next) {
  std::ostringstream o;
  o << "<p class=\"muted avitoHint\">По ссылке в названии — поиск на <strong>Авито</strong>: в текст запроса идут марка, модель, год (если указан в названии), кузов, КПП и привод <strong>этой строки каталога</strong>; по цене — ваши «от–до», если заданы в мастере, иначе узкий диапазон вокруг цены варианта.</p>";
  o << "<p class=\"muted recTableHint\"><strong>Vmax (км/ч)</strong> — максимальная скорость по данным каталога для этой строки (ориентир, не «разгон» и не ограничение на дороге). "
    << "<strong>Соответствие (%)</strong> — насколько автомобиль близок к вашим численным критериям (бюджет, расход, места, багажник, мощность и т.д.): "
    << "это не скорость движения и не «оценка машины», а внутренняя метрика подбора — чем выше процент, тем лучше совпадение с запросом среди уже подходящих по жёстким фильтрам вариантов; типично удобные кандидаты попадают в диапазон примерно 70–100. "
    << "<strong>TCO/год</strong> — грубая оценка стоимости владения (топливо или «электричество», налог, страховка), не офер. Колонка <strong>♥</strong> — в избранное (список в разделе «Избранное»).</p>";
  o << "<div class=\"tableWrap\"><table><thead><tr>"
    << "<th class=\"thHeart\" title=\"Избранное\">♥</th>"
    << "<th>Марка и модель</th><th>Марка</th><th>Цена</th><th>Расход</th><th>Кузов</th><th>КПП</th><th>Привод</th><th>Мест</th><th>Багажник</th><th>Мощн.</th>"
    << "<th title=\"Максимальная скорость по каталогу, км/ч\">Vmax, км/ч</th>"
    << "<th title=\"Насколько вариант близок к вашим численным критериям (не скорость)\">Соответствие, %</th>"
    << "<th title=\"Ориентировочная стоимость владения в год, грубая модель\">TCO/год</th>"
    << "<th>Пояснение</th>"
    << "</tr></thead><tbody>";
  for (const auto& r : recs) {
    const auto& c = r.car;
    const int vmax =
        c.max_speed_kmh > 0 ? c.max_speed_kmh : default_max_speed_kmh(c.power, c.is_electric, c.body_type);
    o << "<tr>"
      << fav_heart_cell(c.id, favorite_ids, heart_next)
      << "<td class=\"tdTitle\">" << car_title_link(c, prefs) << "</td>"
      << "<td>" << escape(c.brand) << "</td>"
      << "<td>" << format_rub(c.price) << "</td>"
      << "<td>" << c.fuel_consumption << "</td>"
      << "<td>" << badge(c.body_type) << "</td>"
      << "<td>" << badge(c.gearbox) << "</td>"
      << "<td>" << badge(c.drive) << "</td>"
      << "<td>" << c.seats << "</td>"
      << "<td>" << c.trunk << "</td>"
      << "<td>" << c.power << "</td>"
      << "<td>" << vmax << "</td>"
      << "<td>" << (int)std::round(r.score * 100) << "</td>"
      << "<td>" << format_rub(estimated_annual_tco_rub(c)) << "</td>"
      << "<td class=\"explainCell\">" << escape(r.explain_ru) << "</td>"
      << "</tr>";
  }
  o << "</tbody></table></div>";
  return o.str();
}

static std::string compare_top_link(const std::vector<ScoredCar>& recs) {
  if (recs.size() < 2) return "";
  std::ostringstream o;
  o << "<p class=\"actions\"><a class=\"btn btn--outline\" href=\"/compare\">Сравнить выданные сейчас (до 3)</a>"
    << "<span class=\"muted compareTopHint\"> — те же автомобили, что в таблице топа выше; раздел «Сравнение» в меню открывает этот же набор, если не заданы <code>?ids=</code>.</span></p>";
  return o.str();
}

static std::string md_inline_escape(std::string s) {
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\n' || s[i] == '\r') s[i] = ' ';
  }
  return s;
}

std::string markdown_export_report(const Preferences& prefs, const std::vector<ScoredCar>& top_recs) {
  std::ostringstream md;
  md << "# AutoSelect — отчёт подбора\n\n";
  md << "_Локальная генерация, без внешних API. Оценки ориентировочные._\n\n## Текущие фильтры\n\n";
  auto line_opt_i = [&](const char* label, const std::optional<int>& v) {
    if (v) md << "- **" << label << ":** " << *v << "\n";
  };
  auto line_opt_d = [&](const char* label, const std::optional<double>& v) {
    if (v) md << "- **" << label << ":** " << *v << "\n";
  };
  auto line_opt_s = [&](const char* label, const std::optional<std::string>& v) {
    if (v && !v->empty()) md << "- **" << label << ":** " << md_inline_escape(*v) << "\n";
  };
  line_opt_i("Мин. цена (руб)", prefs.min_price);
  line_opt_i("Макс. бюджет (руб)", prefs.max_price);
  line_opt_d("Макс. расход л/100км", prefs.max_fuel);
  line_opt_i("Мин. мест", prefs.min_seats);
  line_opt_i("Мин. багажник (л)", prefs.min_trunk);
  line_opt_i("Мин. мощность (л.с.)", prefs.min_power);
  line_opt_i("Макс. мощность (л.с.)", prefs.max_power);
  line_opt_s("Кузов", prefs.body_type);
  line_opt_s("КПП", prefs.gearbox);
  line_opt_s("Привод", prefs.drive);
  line_opt_s("Сегмент", prefs.segment);
  line_opt_s("Марка (подстрока)", prefs.brand_substr);
  line_opt_s("Исключить марку", prefs.brand_exclude_substr);
  line_opt_s("Профиль поездки", prefs.trip_profile);
  if (prefs.only_ev.has_value()) md << "- **Энергия:** " << (*prefs.only_ev ? "только электро" : "без электро") << "\n";
  md << "\n## Топ варианты\n\n";
  if (top_recs.empty()) {
    md << "_Нет строк, подходящих по жёстким фильтрам._\n";
    return md.str();
  }
  int idx = 1;
  for (const auto& sc : top_recs) {
    const auto& c = sc.car;
    md << "### " << idx++ << ". " << md_inline_escape(car_display_title(c)) << "\n\n";
    md << "- **Марка:** " << md_inline_escape(c.brand) << "\n";
    md << "- **Цена:** " << c.price << " ₽\n";
    md << "- **Расход:** " << c.fuel_consumption << " л/100км" << (c.is_electric ? " (электро)" : "") << "\n";
    md << "- **Кузов / КПП / привод:** " << c.body_type << " / " << c.gearbox << " / " << c.drive << "\n";
    md << "- **Мест / багажник / мощн.:** " << c.seats << " / " << c.trunk << " л / " << c.power << " л.с.\n";
    const std::string av = avito_search_url(c, prefs);
    md << "- [Поиск на Авито](" << av << ")\n";
    const auto L = estimate_life_with_car_5y(c, 15000);
    md << "- **«Жизнь ~5 лет» (эквивалент):** ~" << L.monthly_equiv_rub << " ₽/мес\n";
    const auto S = snow_index_for_car(c, prefs);
    md << "- **Снежный индекс:** " << S.score_0_to_10 << "/10 — " << md_inline_escape(S.tier_ru) << "\n\n";
  }
  return md.str();
}

std::string build_sensitivity_export_block(const Preferences& prefs, const std::vector<CarRow>& all_cars,
                                          const std::vector<ScoredCar>& recs_context) {
  if (all_cars.empty()) return "";
  const int universe = static_cast<int>(all_cars.size());
  const int elig = static_cast<int>(recommend(all_cars, prefs, 2'000'000).size());
  std::ostringstream o;
  o << "<section class=\"card senseCard\" id=\"sense\">";
  o << "<div class=\"senseCard__head\">";
  o << "<div class=\"senseCard__titles\"><h2 class=\"senseCard__title\">Чувствительность подбора</h2>";
  o << "<p class=\"senseCard__sub muted\">Прозрачный анализ на вашей базе: сценарии <strong>лифта бюджета</strong> и "
    << "<strong>одного рычага</strong> (снять ровно одно жёсткое ограничение). Подбор в мастере и чате <em>не меняется</em> — это подсказка.</p></div>";
  o << "<div class=\"senseKpi\" aria-label=\"Число кандидатов\"><span class=\"senseKpi__val\">" << elig
    << "</span><span class=\"senseKpi__lbl\">кандидатов сейчас</span><span class=\"senseKpi__of\">из " << universe
    << "</span></div></div>";

  const auto lifts = budget_lift_sensitivity(all_cars, prefs);
  if (!lifts.empty()) {
    o << "<div class=\"senseBlock\"><div class=\"senseBlock__hd\"><h3>Честный лифт бюджета</h3>";
    o << "<span class=\"sensePill\">к текущему максимуму цены</span></div>";
    o << "<p class=\"muted senseHint\">Показано, сколько станет <strong>жёстких</strong> кандидатов, если увеличить только потолок цены на 5%, 10% и 15% "
      << "(остальные фильтры без изменений).</p>";
    o << "<div class=\"senseTableWrap\"><table class=\"senseTable\"><thead><tr><th>Прирост к макс.</th><th>Новый потолок</th>"
      << "<th>Кандидатов</th><th>Новых моделей*</th></tr></thead><tbody>";
    for (const auto& r : lifts) {
      o << "<tr><td>+" << r.bump_percent << "%</td><td>" << format_rub(r.simulated_max_price) << "</td><td>" << r.eligible_count
        << "</td><td>" << r.newly_unblocked << "</td></tr>";
    }
    o << "</tbody></table></div>";
    o << "<p class=\"muted senseNote\">* «Новых» — строк каталога, которых не было в наборе при исходном максимуме цены.</p></div>";
  } else {
    o << "<div class=\"senseBlock senseBlock--muted\"><h3>Честный лифт бюджета</h3>";
    o << "<p class=\"muted\">Укажите <strong>максимальный бюджет</strong> в мастере или в чате (например «до 2 млн») — тогда здесь появятся сценарии +5% / +10% / +15% к потолку.</p></div>";
  }

  const auto levers = relax_one_filter_sensitivity(all_cars, prefs);
  if (!levers.empty()) {
    o << "<div class=\"senseBlock\"><div class=\"senseBlock__hd\"><h3>Один рычаг ослабления</h3>";
    if (elig == 0)
      o << "<span class=\"sensePill sensePill--alert\">0 кандидатов</span></div>";
    else
      o << "<span class=\"sensePill\">+к кандидатам</span></div>";
    if (elig == 0) {
      o << "<p class=\"senseAlert\">Сейчас жёсткие фильтры отсекают всё — ниже рычаги в порядке убывания эффекта (каждая строка: «убрать только это»).</p>";
    } else {
      o << "<p class=\"muted senseHint\">Если убрать <em>ровно одно</em> из ограничений — сколько кандидатов станет доступно и насколько это расширит набор относительно текущего.</p>";
    }
    o << "<ol class=\"senseLeverList\">";
    const size_t maxn = std::min<size_t>(levers.size(), 6u);
    for (size_t i = 0; i < maxn; ++i) {
      const auto& L = levers[i];
      o << "<li><span class=\"senseGain\">+" << L.gain_vs_baseline << "</span> к кандидатам — " << escape(L.label_ru)
        << " <span class=\"senseArrow\">→</span> всего <strong>" << L.eligible_after << "</strong></li>";
    }
    o << "</ol></div>";
  } else if (elig < universe) {
    o << "<div class=\"senseBlock senseBlock--muted\"><h3>Один рычаг ослабления</h3>";
    o << "<p class=\"muted\">Нет жёстких ограничений, снятие которых дало бы прирост — либо фильтры пустые, либо уже почти вся база проходит.</p></div>";
  }

  o << "<div class=\"senseExport\">";
  o << "<a class=\"btn btn--outline senseDl\" href=\"/export/client-report.md\" download=\"client-report.md\">Отчёт для клиента (.md)</a> "
    << "<a class=\"btn btn--ghost senseDl\" href=\"/export/report.md\" download=\"autoselect-report.md\">Краткий техотчёт</a>";
  o << "<span class=\"muted senseExportHint\">Фильтры, топ‑7, ссылки на Авито, «5 лет» и снежный индекс — удобно переслать в мессенджер.</span>";
  if (!recs_context.empty()) {
    o << "<span class=\"senseExportMeta muted\">В отчёт войдут до " << std::min<size_t>(recs_context.size(), 7u)
      << " поз. из текущего топа.</span>";
  }
  o << "</div>";

  o << "</section>";
  return o.str();
}

static std::string delta_badge_int(int delta, bool more_is_better) {
  const bool good = more_is_better ? (delta > 0) : (delta < 0);
  const bool bad = more_is_better ? (delta < 0) : (delta > 0);
  std::string cls = "myDelta myDelta--neu";
  if (delta != 0) {
    if (good) cls = "myDelta myDelta--good";
    else if (bad) cls = "myDelta myDelta--bad";
  }
  std::ostringstream t;
  if (delta > 0) t << "+" << delta;
  else t << delta;
  return "<span class=\"" + cls + "\">" + t.str() + "</span>";
}

static std::string delta_badge_money(int cand, int myv, bool lower_is_better) {
  return delta_badge_int(cand - myv, !lower_is_better);
}

std::string my_car_vs_top_block(const MyCarProfile& mc, const Preferences& prefs, const std::vector<ScoredCar>& top_recs,
                                 const std::string& save_action, const std::string& clear_action) {
  std::ostringstream b;
  b << "<section class=\"card myCarCard\" id=\"mycar\">";
  b << "<div class=\"myCarCard__head\"><h2 class=\"myCarCard__title\">Мой авто → кандидаты</h2>";
  b << "<p class=\"muted myCarCard__lead\">Введите параметры текущей машины — сравним с <strong>топ‑3</strong> подбора по багажнику, расходу, "
    << "TCO/год и эквиваленту «жизнь ~5 лет» (₽/мес). Для расчётов ваша машина моделируется как типовой <strong>hatchback · AT · FWD · mass</strong> — это упрощение, не VIN.</p></div>";

  b << "<form class=\"myCarForm card card--inner\" method=\"POST\" action=\"" << escape(save_action) << "\">";
  b << "<div class=\"myCarFormGrid\">"
    << "<div class=\"fieldFloat\"><input class=\"fieldFloat__input\" id=\"my_trunk\" name=\"my_trunk\" type=\"number\" min=\"80\" max=\"1200\" value=\""
    << (mc.configured ? std::to_string(mc.trunk_l) : "") << "\" placeholder=\" \"/><label class=\"fieldFloat__label\" for=\"my_trunk\">Багажник (л)</label></div>"
    << "<div class=\"fieldFloat\"><input class=\"fieldFloat__input\" id=\"my_fuel\" name=\"my_fuel\" type=\"text\" value=\""
    << (mc.configured ? [&]() {
         std::ostringstream f;
         f << mc.fuel_l100;
         return f.str();
       }()
                              : "")
    << "\" placeholder=\" \"/><label class=\"fieldFloat__label\" for=\"my_fuel\">Расход л/100км (0 если электро)</label></div>"
    << "<div class=\"fieldFloat\"><input class=\"fieldFloat__input\" id=\"my_power\" name=\"my_power\" type=\"number\" min=\"40\" max=\"700\" value=\""
    << (mc.configured ? std::to_string(mc.power_hp) : "") << "\" placeholder=\" \"/><label class=\"fieldFloat__label\" for=\"my_power\">Мощность (л.с.)</label></div>"
    << "<div class=\"fieldFloat\"><input class=\"fieldFloat__input\" id=\"my_price\" name=\"my_price\" type=\"number\" min=\"0\" value=\""
    << (mc.configured && mc.approx_price_rub > 0 ? std::to_string(mc.approx_price_rub) : "")
    << "\" placeholder=\" \"/><label class=\"fieldFloat__label\" for=\"my_price\">Оценка цены (₽, необязательно)</label></div>"
    << "</div>";
  b << "<div class=\"fieldCheck myCarEvRow\"><label class=\"checkLbl\"><input type=\"checkbox\" name=\"my_ev\" value=\"1\""
    << (mc.configured && mc.is_electric ? " checked" : "") << "/> Электромобиль (расход не учитываем)</label></div>";
  b << "<div class=\"myCarFormActions\"><button class=\"btn btn--solid\" type=\"submit\">Сохранить и сравнить</button>";
  b << "<button class=\"btn btn--outline\" type=\"submit\" formaction=\"" << escape(clear_action) << "\" formmethod=\"POST\">Сбросить</button></div>";
  b << "</form>";

  if (top_recs.empty()) {
    b << "<p class=\"muted myCarHint\">Сначала выполните подбор выше — сравнение строится относительно строк топа.</p>";
    b << "</section>";
    return b.str();
  }

  if (!mc.configured) {
    b << "<p class=\"muted myCarHint\">Заполните форму и нажмите «Сохранить и сравнить» — появится таблица «что меняется».</p>";
    b << "</section>";
    return b.str();
  }

  const CarRow my = synthetic_my_car_row(mc);
  const int my_tco = estimated_annual_tco_rub(my, 15000);
  const int my_mo = estimate_life_with_car_5y(my, 15000).monthly_equiv_rub;

  b << "<div class=\"tableWrap myCarTableWrap\"><table class=\"myCarTable\"><thead><tr>"
    << "<th>Кандидат</th><th>Багажник</th><th>Δ к вам</th><th>Расход</th><th>Δ</th><th>TCO/год</th><th>Δ</th><th>«5 лет» ₽/мес</th><th>Δ</th>"
    << "</tr></thead><tbody>";

  const size_t n = std::min<size_t>(top_recs.size(), 3u);
  for (size_t i = 0; i < n; ++i) {
    const auto& c = top_recs[i].car;
    const int ct = estimated_annual_tco_rub(c, 15000);
    const int cm = estimate_life_with_car_5y(c, 15000).monthly_equiv_rub;
    b << "<tr><td class=\"myCarTdTitle\">" << car_title_link(c, prefs) << "</td>";
    b << "<td>" << c.trunk << " л</td><td>" << delta_badge_int(c.trunk - my.trunk, true) << "</td>";

    if (my.is_electric && !c.is_electric) {
      b << "<td>" << c.fuel_consumption << "</td><td><span class=\"myDelta myDelta--neu\" title=\"ДВС vs ваш электро\">—</span></td>";
    } else if (!my.is_electric && c.is_electric) {
      b << "<td>0 (электро)</td><td><span class=\"myDelta myDelta--neu\" title=\"Электро vs ваш ДВС\">—</span></td>";
    } else if (my.is_electric && c.is_electric) {
      b << "<td>0 / 0</td><td>" << delta_badge_int(0, false) << "</td>";
    } else {
      const double df = c.fuel_consumption - my.fuel_consumption;
      std::ostringstream dfstr;
      dfstr.setf(std::ios::fixed);
      dfstr << std::setprecision(1);
      if (df > 1e-9) dfstr << "+";
      dfstr << df;
      const char* cls = "myDelta myDelta--neu";
      if (df < -1e-6) cls = "myDelta myDelta--good";
      else if (df > 1e-6) cls = "myDelta myDelta--bad";
      b << "<td>" << c.fuel_consumption << "</td><td><span class=\"" << cls << "\">" << dfstr.str() << " л</span></td>";
    }

    b << "<td>" << format_rub(ct) << "</td><td>" << delta_badge_money(ct, my_tco, true) << "</td>";
    b << "<td>" << format_rub(cm) << "</td><td>" << delta_badge_money(cm, my_mo, true) << "</td>";
    b << "</tr>";
  }

  b << "<tr class=\"myCarRowRef\"><td><strong>" << escape(my.name) << "</strong></td>";
  b << "<td>" << my.trunk << " л</td><td>—</td>";
  {
    std::string myfuel = "0 (электро)";
    if (!my.is_electric) {
      std::ostringstream u;
      u << my.fuel_consumption;
      myfuel = u.str();
    }
    b << "<td>" << myfuel << "</td><td>—</td>";
  }
  b << "<td>" << format_rub(my_tco) << "</td><td>—</td>";
  b << "<td>" << format_rub(my_mo) << "</td><td>—</td></tr>";

  b << "</tbody></table></div>";
  b << "<p class=\"muted myCarLegend\"><span class=\"myDelta myDelta--good\">зелёный</span> — улучшение для вас по этой метрике, "
    << "<span class=\"myDelta myDelta--bad\">красный</span> — ухудшение; для расхода меньше л/100км лучше. TCO и «5 лет» — чем ниже, тем дешевле в модели.</p>";
  b << "</section>";
  return b.str();
}

std::string wizard_page(const Preferences& prefs, const std::vector<ScoredCar>& recs, const std::vector<WizardQuestion>& followups,
                         const std::vector<int64_t>& favorite_ids, const std::string& sensitivity_html, const MyCarProfile& my_car,
                         const std::string& my_car_save, const std::string& my_car_clear, const ClientProfile& client) {
  std::ostringstream b;
  b << "<h1>Мастер‑подбор</h1>";
  b << "<p class=\"muted\">Критериев стало больше: бюджет «от–до», класс сегмента, марка, мощность сверху, электро/ДВС. После отправки — наводящие и уточняющие вопросы по контексту.</p>";
  b << "<form class=\"card\" method=\"POST\" action=\"/wizard\">";
  b << "<div class=\"grid3\">"
    << input("Мин. цена (руб)", "min_price", prefs.min_price ? std::to_string(*prefs.min_price) : "")
    << input("Макс. бюджет (руб)", "max_price", prefs.max_price ? std::to_string(*prefs.max_price) : "")
    << input("Сегмент (economy/mass/premium)", "segment", prefs.segment ? *prefs.segment : "")
    << input("Марка (подстрока)", "brand_substr", prefs.brand_substr ? *prefs.brand_substr : "")
    << input("Макс. расход (л/100км)", "max_fuel", prefs.max_fuel ? std::to_string(*prefs.max_fuel) : "")
    << input("Мин. мест", "min_seats", prefs.min_seats ? std::to_string(*prefs.min_seats) : "")
    << input("Мин. багажник (л)", "min_trunk", prefs.min_trunk ? std::to_string(*prefs.min_trunk) : "")
    << input("Мин. мощность (л.с.)", "min_power", prefs.min_power ? std::to_string(*prefs.min_power) : "")
    << input("Макс. мощность (л.с.)", "max_power", prefs.max_power ? std::to_string(*prefs.max_power) : "")
    << input("Тип кузова", "body_type", prefs.body_type ? *prefs.body_type : "")
    << input("КПП (at/mt/cvt/robot)", "gearbox", prefs.gearbox ? *prefs.gearbox : "")
    << input("Привод (fwd/rwd/awd)", "drive", prefs.drive ? *prefs.drive : "")
    << "</div>"
    << "<div class=\"grid2\">"
    << "<div class=\"fieldRadios\"><div class=\"lbl\">Профиль поездки (влияет на «мягкий» балл)</div>"
    << "<select class=\"fieldFloat__input\" name=\"trip_profile\">";
  {
    const std::string tr = prefs.trip_profile ? *prefs.trip_profile : "";
    auto sel = [&](const char* v) { return tr == v ? " selected" : ""; };
    b << "<option value=\"\"" << (tr.empty() ? " selected" : "") << ">не задан</option>"
      << "<option value=\"city\"" << sel("city") << ">город — экономия на топливе</option>"
      << "<option value=\"highway\"" << sel("highway") << ">трасса — запас по мощности</option>"
      << "<option value=\"family\"" << sel("family") << ">семья — места и багажник</option>"
      << "<option value=\"dacha\"" << sel("dacha") << ">дача/грунт — AWD и кроссовер в приоритете</option>"
      << "<option value=\"taxi\"" << sel("taxi") << ">такси/пробег — эконом-сегмент и расход</option>";
  }
  b << "</select></div>"
    << input("Исключить марку (подстрока, жёстко)", "brand_exclude", prefs.brand_exclude_substr ? *prefs.brand_exclude_substr : "")
    << "</div>"
    << "<div class=\"fieldCheck\"><div class=\"lbl\">Желательно (не жёсткий фильтр — только балл)</div>"
    << "<label class=\"checkLbl\"><input type=\"checkbox\" name=\"soft_awd\" value=\"1\""
    << (prefs.drive_soft && *prefs.drive_soft == "awd" ? " checked" : "") << "/> полный привод AWD</label> "
    << "<label class=\"checkLbl\"><input type=\"checkbox\" name=\"soft_crossover\" value=\"1\""
    << (prefs.body_soft && *prefs.body_soft == "crossover" ? " checked" : "") << "/> кузов кроссовер</label> "
    << "<label class=\"checkLbl\"><input type=\"checkbox\" name=\"soft_at\" value=\"1\""
    << (prefs.gearbox_soft && *prefs.gearbox_soft == "at" ? " checked" : "") << "/> автомат AT</label></div>"
    << input("Желательно минимум мест (5–9)", "soft_min_seats", prefs.soft_min_seats ? std::to_string(*prefs.soft_min_seats) : "")
    << energy_radios(prefs)
    << "<div class=\"actions\">"
    << "<button class=\"btn btn--solid btn--wide\" type=\"submit\">Подобрать</button> "
    << "<button class=\"btn btn--danger btn--outline\" type=\"submit\" formaction=\"/wizard/reset\" formmethod=\"POST\">Сбросить фильтры</button>"
    << "</div>"
    << "</form>";

  b << "<section class=\"card\"><h2>Текущие фильтры</h2>" << prefs_badges(prefs) << "</section>";

  if (!followups.empty()) {
    b << "<section class=\"card card--followup\"><h2>Подсказки по подбору</h2>";
    for (const auto& wq : followups) {
      const char* tag = (wq.kind == WizardQuestion::Kind::Lead) ? "Наводящий" : "Уточняющий";
      b << "<div class=\"qBlock qBlock--" << (wq.kind == WizardQuestion::Kind::Lead ? "lead" : "detail") << "\">"
        << "<div class=\"qTag\">" << tag << "</div>"
        << "<p class=\"followupQ\">" << escape(wq.text) << "</p></div>";
    }
    b << "</section>";
  }

  b << "<section class=\"card\"><h2>Рекомендации</h2>";
  if (recs.empty()) b << "<p>Нет подходящих автомобилей по текущим критериям.</p>";
  else {
    b << reco_list(recs, prefs, favorite_ids, "/wizard");
    b << compare_top_link(recs);
    b << top3_decision_insights_html(prefs, recs);
  }
  b << "</section>";

  if (!sensitivity_html.empty()) b << sensitivity_html;

  b << client_tools_panel_html(client, "/wizard/client-profile");

  b << my_car_vs_top_block(my_car, prefs, recs, my_car_save, my_car_clear);

  return layout("Мастер‑подбор", b.str(), "wizard");
}

std::string chat_page(const std::vector<ChatMessage>& history, const Preferences& prefs, const std::vector<ScoredCar>& recs,
                      const std::vector<int64_t>& favorite_ids, const std::string& sensitivity_html, const MyCarProfile& my_car,
                      const std::string& my_car_save, const std::string& my_car_clear, const ClientProfile& client) {
  std::ostringstream b;
  b << "<h1>Чат‑помощник</h1>";
  b << "<p class=\"muted recTableHint\">Пишите критерии в свободной форме (можно несколько через запятую или точку). "
    << "Бот выдёргивает из текста бюджет, кузов, привод, КПП, мощность, марку, электро/ДВС, сценарий поездки, «желательно …» и исключение марок; "
    << "числовые «минимумы» усиливаются, «максимумы» (цена, расход) — сужаются. "
    << "Фразы вроде «сбрось фильтры» или кнопка справа обнуляют критерии. "
    << "Если спросить про <strong>зиму</strong> или <strong>стоимость владения ~5 лет</strong>, к ответу добавится короткая оценка по лидеру топа; "
    << "полные карточки и <strong>слепой тест</strong> — в разделе <a href=\"/extras\">Лаборатория</a>. "
    << "Под таблицей топа — блок <strong>«Чувствительность подбора»</strong> (лифт бюджета, один рычаг) и ссылка на отчёт <code>.md</code>.</p>";
  b << "<div class=\"split\">";
  b << "<section class=\"card chat\">"
    << "<div class=\"chatHistory\">";
  for (const auto& m : history) {
    b << "<div class=\"msg " << (m.role == "user" ? "user" : "assistant") << "\">"
      << "<div class=\"role\">" << (m.role == "user" ? "Вы" : "Помощник") << "</div>"
      << "<div class=\"text\">" << escape(m.text) << "</div>"
      << "</div>";
  }
  b << "</div>"
    << "<form class=\"chatForm\" method=\"POST\" action=\"/chat\">"
    << "<div class=\"chatBar\">"
    << "<input class=\"chatBar__input\" name=\"message\" type=\"text\" autocomplete=\"off\" "
    << "placeholder=\"Например: семья 5 человек, кроссовер, автомат, до 2 млн\"/>"
    << "<button class=\"btn btn--solid chatBar__btn\" type=\"submit\">Отправить</button>"
    << "</div></form>"
    << "</section>";

  b << "<aside class=\"card\">"
    << "<form method=\"POST\" action=\"/chat/reset\" class=\"chatResetRow\">"
    << "<button class=\"btn btn--danger btn--outline btn--sm\" type=\"submit\">Сбросить фильтры</button></form>"
    << "<h2>Текущие фильтры</h2>" << prefs_badges(prefs)
    << "<h2>Топ‑подбор</h2>";
  if (recs.empty()) b << "<p>Пока нет подходящих вариантов — уточните требования.</p>";
  else {
    b << reco_list(recs, prefs, favorite_ids, "/chat");
    b << compare_top_link(recs);
    b << top3_decision_insights_html(prefs, recs);
  }
  if (!sensitivity_html.empty()) b << sensitivity_html;
  b << client_tools_panel_html(client, "/chat/client-profile");
  b << my_car_vs_top_block(my_car, prefs, recs, my_car_save, my_car_clear);
  b << "</aside>";

  b << "</div>";
  return layout("Чат‑помощник", b.str(), "chat");
}

// Общий блок сравнения (фильтры, лидер, таблица, ссылки) — cars не пустой.
static std::string compare_main_content_html(const std::vector<CarRow>& cars, const Preferences& prefs, bool from_session_top) {
  size_t winner = 0;
  double best = -1.0;
  for (size_t i = 0; i < cars.size(); ++i) {
    const double s = score_soft(cars[i], prefs);
    if (s > best) {
      best = s;
      winner = i;
    }
  }
  std::ostringstream b;
  if (from_session_top) {
    b << "<section class=\"card compareSessionBanner\"><p>Сравниваются <strong>последние варианты из вашего топа</strong> (мастер или чат). Чтобы обновить набор — снова нажмите «Подобрать» или отправьте сообщение в чате.</p></section>";
  }
  b << "<p class=\"muted\">Таблица и баллы считаются по <strong>текущим фильтрам сессии</strong> (общие для мастера и чата). "
    << "Колонка со звёздочкой — лидер по мягкому баллу (как «Соответствие» в топе).</p>"
    << "<section class=\"card\"><h2>Учтённые фильтры</h2>" << prefs_badges(prefs) << "</section>";

  b << "<section class=\"card\"><h2>Почему лидер</h2><p class=\"compareExplain\">"
    << escape(explain_match_ru(cars[winner], prefs))
    << "</p><p class=\"muted\">Лидер: <strong>" << escape(car_display_title(cars[winner])) << "</strong> — соответствие ≈ "
    << (int)std::round(best * 100.0) << "%.</p></section>";

  b << "<section class=\"card\"><h2>По ключевым параметрам</h2>"
    << "<p class=\"muted avitoHint\">По ссылке в названии колонки — поиск на <strong>Авито</strong> по этой строке каталога (марка, модель, год, кузов, КПП, привод, цена).</p>"
    << "<div class=\"tableWrap\"><table class=\"compareTable\"><thead><tr><th>Параметр</th>";
  for (size_t i = 0; i < cars.size(); ++i) {
    b << "<th" << (i == winner ? " class=\"thWinner\"" : "") << ">" << car_title_link(cars[i], prefs);
    if (i == winner) b << " ★";
    b << "</th>";
  }
  b << "</tr></thead><tbody>";

  b << "<tr><th scope=\"row\">Цена</th>";
  for (const auto& c : cars) b << "<td>" << format_rub(c.price) << "</td>";
  b << "</tr>";

  b << "<tr><th scope=\"row\">Расход, л/100км</th>";
  for (const auto& c : cars) b << "<td>" << c.fuel_consumption << "</td>";
  b << "</tr>";

  b << "<tr><th scope=\"row\">Кузов</th>";
  for (const auto& c : cars) b << "<td>" << badge(c.body_type) << "</td>";
  b << "</tr>";

  b << "<tr><th scope=\"row\">КПП</th>";
  for (const auto& c : cars) b << "<td>" << badge(c.gearbox) << "</td>";
  b << "</tr>";

  b << "<tr><th scope=\"row\">Привод</th>";
  for (const auto& c : cars) b << "<td>" << badge(c.drive) << "</td>";
  b << "</tr>";

  b << "<tr><th scope=\"row\">Места</th>";
  for (const auto& c : cars) b << "<td>" << c.seats << "</td>";
  b << "</tr>";

  b << "<tr><th scope=\"row\">Багажник, л</th>";
  for (const auto& c : cars) b << "<td>" << c.trunk << "</td>";
  b << "</tr>";

  b << "<tr><th scope=\"row\">Мощность, л.с.</th>";
  for (const auto& c : cars) b << "<td>" << c.power << "</td>";
  b << "</tr>";

  b << "<tr><th scope=\"row\">Vmax, км/ч</th>";
  for (const auto& c : cars) {
    const int vmax =
        c.max_speed_kmh > 0 ? c.max_speed_kmh : default_max_speed_kmh(c.power, c.is_electric, c.body_type);
    b << "<td>" << vmax << "</td>";
  }
  b << "</tr>";

  b << "<tr><th scope=\"row\">Сегмент</th>";
  for (const auto& c : cars) b << "<td>" << badge(c.segment) << "</td>";
  b << "</tr>";

  b << "<tr><th scope=\"row\">Балл, %</th>";
  for (const auto& c : cars) {
    const int pct = (int)std::round(score_soft(c, prefs) * 100.0);
    b << "<td>" << pct << "</td>";
  }
  b << "</tr>";

  b << "<tr><th scope=\"row\">TCO/год (оценка)</th>";
  for (const auto& c : cars) b << "<td>" << format_rub(estimated_annual_tco_rub(c)) << "</td>";
  b << "</tr>";

  b << "<tr><th scope=\"row\">Пояснение подборщика</th>";
  for (const auto& c : cars) b << "<td class=\"explainCell\">" << escape(explain_match_ru(c, prefs)) << "</td>";
  b << "</tr>";

  b << "</tbody></table></div></section>";

  b << "<p class=\"actions\"><a class=\"btn btn--outline\" href=\"/wizard\">Мастер‑подбор</a> "
    << "<a class=\"btn btn--ghost\" href=\"/chat\">Чат</a> <a class=\"btn btn--ghost\" href=\"/favorites\">Избранное</a></p>";

  return b.str();
}

std::string compare_page(const std::vector<CarRow>& cars, const Preferences& prefs, bool from_session_top) {
  if (cars.empty()) {
    std::ostringstream b;
    b << "<h1>Сравнение</h1>";
    if (from_session_top) {
      b << "<p class=\"muted\">Пока нечего сравнивать: сначала откройте <strong>мастер</strong> или <strong>чат</strong> и получите хотя бы два варианта в топе — тогда сюда подставятся последние до трёх id из выдачи.</p>";
    } else {
      b << "<p class=\"muted\">Нет данных для сравнения. Задайте в адресе параметр <code>?ids=1,2,3</code> (id из каталога) или сначала получите подбор в мастере/чате и откройте эту страницу без параметров — подставятся последние лидеры из топа.</p>";
    }
    b << "<p class=\"actions\"><a class=\"btn btn--outline\" href=\"/wizard\">Мастер‑подбор</a> "
      << "<a class=\"btn btn--ghost\" href=\"/chat\">Чат</a></p>";
    return layout("Сравнение", b.str(), "compare");
  }

  std::ostringstream b;
  b << "<h1>Сравнение финалистов</h1>";
  b << compare_main_content_html(cars, prefs, from_session_top);
  return layout("Сравнение", b.str(), "compare");
}

std::string favorites_page(const std::vector<CarRow>& cars, const Preferences& prefs, const std::vector<int64_t>& favorite_ids) {
  std::ostringstream b;
  b << "<h1>Избранное</h1>";
  b << "<p class=\"muted\">Отметьте автомобиль сердечком (♥) в таблице подбора в <a href=\"/wizard\">мастере</a> или <a href=\"/chat\">чате</a>. "
    << "Список хранится в сессии на сервере и пропадёт после перезапуска <code>car_picker</code>.</p>";

  if (cars.empty()) {
    b << "<section class=\"card\"><p>Пока пусто.</p></section>";
    return layout("Избранное", b.str(), "favorites");
  }

  b << "<section class=\"card\"><h2>Сохранённые модели</h2>";
  b << "<form method=\"POST\" action=\"/favorites/clear\" class=\"favClearRow\" onsubmit=\"return confirm('Убрать все из избранного?');\">"
    << "<button type=\"submit\" class=\"btn btn--outline btn--sm\">Очистить список</button></form>";
  b << "<div class=\"tableWrap\"><table class=\"favTable\"><thead><tr>"
    << "<th class=\"thHeart\" title=\"Избранное\">♥</th><th>Автомобиль</th><th>Цена</th><th>Кузов</th><th>Места</th><th>Привод</th></tr></thead><tbody>";
  for (const auto& c : cars) {
    b << "<tr>" << fav_heart_cell(c.id, favorite_ids, "/favorites") << "<td class=\"tdTitle\">" << car_title_link(c, prefs) << "</td>"
      << "<td>" << format_rub(c.price) << "</td>"
      << "<td>" << badge(c.body_type) << "</td>"
      << "<td>" << c.seats << "</td>"
      << "<td>" << badge(c.drive) << "</td></tr>";
  }
  b << "</tbody></table></div></section>";

  if (cars.size() >= 2) {
    std::vector<CarRow> subset;
    for (size_t i = 0; i < cars.size() && i < 3; ++i) subset.push_back(cars[i]);
    std::ostringstream idq;
    idq << "ids=";
    for (size_t i = 0; i < subset.size(); ++i) {
      if (i) idq << ',';
      idq << subset[i].id;
    }
    b << "<section class=\"card favCompareSection\"><h2>Сравнение из избранного</h2>"
      << "<p class=\"muted\">До трёх позиций в порядке добавления. Та же таблица на отдельной странице: "
      << "<a href=\"/compare?" << idq.str() << "\">раздел «Сравнение»</a>.</p>";
    b << compare_main_content_html(subset, prefs, false);
    b << "</section>";
  } else {
    b << "<section class=\"card\"><p class=\"muted\">Добавьте ещё одну модель в ♥, чтобы увидеть сравнение здесь.</p></section>";
  }

  return layout("Избранное", b.str(), "favorites");
}

std::string extras_page(const Preferences& prefs, const std::vector<ScoredCar>& insight_top3, const std::vector<CarRow>& blind_three,
                        int blind_pick, const std::string& sensitivity_html) {
  std::ostringstream b;
  b << "<h1>Лаборатория</h1>";
  b << "<p class=\"muted labLead\">Дополнительные метрики для защиты проекта и «вау»-эффекта. Они <strong>не участвуют</strong> в скоринге "
    << "мастера и чата — только помогают интерпретировать варианты.</p>";

  b << "<section class=\"card labBlock\" id=\"life\"><h2>Жизнь с машиной ~5 лет</h2>";
  b << "<p class=\"muted\">В ₽/мес — усреднение за 60 месяцев: амортизационная потеря (доля от цены по сегменту) + 5×(TCO/год из каталога + ТО + резина). "
    << "КАСКО, кредит и внезапные ремонты сюда не входят.</p>";
  if (insight_top3.empty()) {
    b << "<p>Сначала получите топ в <a href=\"/wizard\">мастере</a> или <a href=\"/chat\">чате</a> — тогда здесь появятся те же лидеры с разбором.</p>";
  } else {
    b << "<div class=\"labGrid\">";
    for (const auto& sc : insight_top3) {
      const auto& c = sc.car;
      const auto L = estimate_life_with_car_5y(c, 15000);
      b << "<article class=\"labCard\"><h3 class=\"labCard__title\">" << escape(car_display_title(c)) << "</h3>";
      b << "<p class=\"labHero\">≈ " << format_rub(L.monthly_equiv_rub) << "<span class=\"labHeroSub\"> усреднённо в месяц</span></p>";
      b << "<ul class=\"labList\">";
      b << "<li>Потеря стоимости за 5 лет: " << format_rub(L.depreciation_5y_rub) << "</li>";
      b << "<li>Эксплуатация 5×(топливо/эл. + ОСАГО+налог): " << format_rub(L.running_year_rub) << "/год → "
        << format_rub(L.running_year_rub * 5) << "</li>";
      b << "<li>ТО и расходники: " << format_rub(L.service_year_rub) << "/год → " << format_rub(L.service_year_rub * 5) << "</li>";
      b << "<li>Резина (две линейки, амортизация): " << format_rub(L.tires_year_rub) << "/год → " << format_rub(L.tires_year_rub * 5)
        << "</li>";
      b << "<li><strong>Всего за 5 лет:</strong> " << format_rub(L.total_5y_rub) << "</li>";
      b << "</ul>";
      b << "<p class=\"muted labFoot\">" << escape(L.comment_ru) << "</p></article>";
    }
    b << "</div>";
  }
  b << "</section>";

  b << "<section class=\"card labBlock\" id=\"snow\"><h2>Снежный индекс</h2>";
  b << "<p class=\"muted\">Шкала 0–10: прокси клиренса по кузову, привод, тип КПП, плюс небольшой бонус, если в фильтрах выбран сценарий «дача». "
    << "Это игровая оценка, не замер с каталога.</p>";
  if (insight_top3.empty()) {
    b << "<p class=\"muted\">Появится вместе с подбором выше.</p>";
  } else {
    b << "<div class=\"labGrid\">";
    for (const auto& sc : insight_top3) {
      const auto& c = sc.car;
      const auto S = snow_index_for_car(c, prefs);
      b << "<article class=\"labCard labCard--snow\"><div class=\"labSnowTop\"><h3 class=\"labCard__title\">"
        << escape(car_display_title(c)) << "</h3>";
      b << "<div class=\"labSnowDial\" role=\"img\" aria-label=\"Индекс " << S.score_0_to_10 << " из 10\">"
        << "<span class=\"labSnowNum\">" << S.score_0_to_10 << "</span><span class=\"labSnowMax\">/10</span></div></div>";
      b << "<p class=\"labTier\">" << escape(S.tier_ru) << "</p>";
      b << "<p class=\"muted labFoot\">" << escape(S.explain_ru) << "</p></article>";
    }
    b << "</div>";
  }
  b << "</section>";

  if (!sensitivity_html.empty()) b << sensitivity_html;

  b << "<section class=\"card labBlock\" id=\"blind\"><h2>Слепой тест</h2>";
  b << "<p class=\"muted\">Три анонимных профиля из вашей базы. Выберите тот, что ближе вашему идеалу по цифрам — и увидите марки. "
    << "Можно начать заново в любой момент.</p>";

  if (blind_pick >= 0 && blind_three.size() == 3) {
    b << "<div class=\"labReveal\"><p class=\"labRevealTitle\">Вы выбрали вариант "
      << char('A' + blind_pick) << "</p><div class=\"labGrid labGrid--blind\">";
    for (size_t i = 0; i < blind_three.size(); ++i) {
      const auto& c = blind_three[i];
      const bool hi = static_cast<int>(i) == blind_pick;
      b << "<article class=\"labCard labCard--blindReveal" << (hi ? " labCard--picked" : "") << "\">";
      b << "<div class=\"labBlindMark\">" << char('A' + i) << "</div>";
      b << "<h3 class=\"labCard__title\">" << escape(car_display_title(c)) << "</h3>";
      b << "<p class=\"muted\">" << escape(c.brand) << " · " << badge(c.body_type) << " · " << badge(c.drive) << "</p></article>";
    }
    b << "</div></div>";
    b << "<form method=\"POST\" action=\"/extras/blind/new\" class=\"labNewRound\"><button type=\"submit\" class=\"btn btn--outline\">Новая тройка</button></form>";
  } else if (blind_three.size() == 3) {
    b << "<div class=\"labGrid labGrid--blind\">";
    const char* letters = "ABC";
    for (size_t i = 0; i < blind_three.size(); ++i) {
      const auto& c = blind_three[i];
      b << "<article class=\"labCard labCard--blind\">";
      b << "<div class=\"labBlindMark\">" << letters[i] << "</div>";
      b << "<ul class=\"labBlindStats\">";
      b << "<li>Цена: <strong>" << format_rub(c.price) << "</strong></li>";
      b << "<li>Расход: <strong>" << c.fuel_consumption << "</strong> л/100км" << (c.is_electric ? " (электро)" : "") << "</li>";
      b << "<li>Кузов: " << badge(c.body_type) << "</li>";
      b << "<li>Багажник: <strong>" << c.trunk << "</strong> л</li>";
      b << "<li>Мощность: <strong>" << c.power << "</strong> л.с.</li>";
      b << "</ul>";
      b << "<form method=\"POST\" action=\"/extras/blind/pick\" class=\"labPickForm\">"
        << "<input type=\"hidden\" name=\"pick\" value=\"" << i << "\"/>"
        << "<button type=\"submit\" class=\"btn btn--solid btn--block\">Выбрать " << letters[i] << "</button></form></article>";
    }
    b << "</div>";
  } else {
    b << "<p class=\"muted\">В каталоге меньше трёх автомобилей — мини-игра недоступна.</p>";
  }

  b << "<p class=\"actions labNav\"><a class=\"btn btn--ghost\" href=\"/wizard\">Мастер</a> "
    << "<a class=\"btn btn--ghost\" href=\"/chat\">Чат</a></p>";

  return layout("Лаборатория", b.str(), "extras");
}

} // namespace html

