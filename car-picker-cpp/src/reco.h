#pragma once

#include "db.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

struct Preferences {
  std::optional<int> min_price;             // руб, нижняя граница
  std::optional<int> max_price;             // руб, верх бюджета
  /// Нижняя граница цены не нужна — не повторять вопрос про min_price при уже заданном max_price.
  std::optional<bool> min_price_floor_opt_out;
  std::optional<double> max_fuel;          // л/100км
  /// Пользователь явно сказал, что расход не важен — не спрашивать снова и не вводить потолок max_fuel.
  std::optional<bool> fuel_ignore_cap;
  std::optional<int> min_seats;
  std::optional<int> min_trunk;
  std::optional<int> min_power;
  std::optional<int> max_power;            // л.с. верх
  /// Верхняя планка по мощности не нужна — не повторять вопрос после заданного min_power.
  std::optional<bool> max_power_ceiling_opt_out;
  std::optional<std::string> body_type;
  std::optional<std::string> gearbox;
  std::optional<std::string> drive;
  std::optional<std::string> brand_substr;  // подстрока марки
  std::optional<std::string> segment;      // economy | mass | premium
  std::optional<bool> only_ev;             // true — только электро; false — без электро

  // Профиль поездки: city | highway | family | dacha | taxi — влияет на веса в score_soft
  std::optional<std::string> trip_profile;
  // «Желательно» (не жёсткий фильтр): усиливают балл, если совпало
  std::optional<std::string> drive_soft;
  std::optional<std::string> body_soft;
  std::optional<std::string> gearbox_soft;
  std::optional<int> soft_min_seats;
  // Исключить марку (подстрока в нижнем регистре) — жёстко убирает из подбора
  std::optional<std::string> brand_exclude_substr;
};

/// Параметры текущего авто пользователя для сравнения «мой → кандидат» (не строка каталога).
struct MyCarProfile {
  bool configured = false;
  int trunk_l = 380;
  double fuel_l100 = 8.0;
  int power_hp = 120;
  bool is_electric = false;
  /// 0 — взять типовую оценку для TCO (~1.4 млн ₽).
  int approx_price_rub = 0;
};

struct WizardQuestion {
  enum class Kind { Lead, Detail };
  Kind kind = Kind::Detail;
  std::string text;
};

struct ScoredCar {
  CarRow car;
  double score = 0.0;
  std::string explain_ru;  // короткое пояснение совпадений / компромиссов
};

Preferences prefs_from_form(const std::map<std::string, std::string>& form);
Preferences prefs_from_chat_text(const std::string& text, const Preferences* ctx = nullptr);
std::vector<ScoredCar> recommend(const std::vector<CarRow>& cars, const Preferences& p, int top_k);

// Мягкий балл (0..1) — используется в рекомендациях и на странице сравнения
double score_soft(const CarRow& car, const Preferences& p);

// До 3 вопросов: наводящие (сценарий) и уточняющие (метрики); порядок и текст зависят от профиля и числа кандидатов.
std::vector<WizardQuestion> wizard_followups(const Preferences& p, int eligible_count);

// Слияние ответов в чате: минимумы/«жёстче» накапливаются, строки — последнее явное значение.
void merge_chat_prefs(Preferences& acc, const Preferences& delta);

// Заголовок в UI: «Марка Модель [год]» без дублирования марки.
std::string car_display_title(const CarRow& car);

// Поиск на Авито: марка/модель + параметры подбора (цена, КПП, привод, электро и т.д.)
std::string avito_search_url(const CarRow& car, const Preferences& prefs);

// Ориентировочная стоимость владения в год (руб., грубая модель: топливо/электроэнергия, ОСАГО-оценка, налог)
int estimated_annual_tco_rub(const CarRow& car, int km_per_year = 15000);

// «Жизнь с машиной» ~5 лет: амортизационная потеря + эксплуатация (как TCO/год) + ТО + резина; не офер.
struct LifeWithCar5y {
  int km_per_year = 15000;
  int running_year_rub = 0;
  int service_year_rub = 0;
  int tires_year_rub = 0;
  int depreciation_5y_rub = 0;
  int operating_5y_rub = 0;
  int total_5y_rub = 0;
  int monthly_equiv_rub = 0;
  std::string comment_ru;
};
LifeWithCar5y estimate_life_with_car_5y(const CarRow& car, int km_per_year = 15000);

// «Снежный индекс»: 0–10 по прокси-клиренсу, приводу, КПП и кузову (+учёт сценария в prefs, если задан).
struct SnowIndex {
  int score_0_to_10 = 0;
  int clearance_proxy_mm = 0;
  std::string tier_ru;
  std::string explain_ru;
};
SnowIndex snow_index_for_car(const CarRow& car, const Preferences& prefs = Preferences{});

// Дополнение к ответу чата по ключевым фразам (топ‑1 из выдачи); пустая строка — молчим.
std::string chat_insight_addon_ru(const std::string& user_message, const CarRow& top_car, const Preferences& prefs);

// «Честный лифт бюджета» и «один рычаг ослабления» — анализ без изменения prefs.
struct BudgetLiftRow {
  int bump_percent = 0;
  int simulated_max_price = 0;
  int eligible_count = 0;
  int newly_unblocked = 0;
};
struct RelaxLeverRow {
  std::string key;
  std::string label_ru;
  int eligible_after = 0;
  int gain_vs_baseline = 0;
};
std::vector<BudgetLiftRow> budget_lift_sensitivity(const std::vector<CarRow>& cars, const Preferences& prefs);
std::vector<RelaxLeverRow> relax_one_filter_sensitivity(const std::vector<CarRow>& cars, const Preferences& prefs);

// Синтетическая строка каталога для расчётов TCO / «5 лет» по введённым параметрам «мой авто».
CarRow synthetic_my_car_row(const MyCarProfile& m);

// Текст пояснения для строки рекомендации (без нейросети, по правилам)
std::string explain_match_ru(const CarRow& car, const Preferences& prefs);

/// Разбор «почему в топе» по категориям (для карточек и API).
struct ExplainableMatch {
  std::vector<std::string> strengths;
  std::vector<std::string> preferences;
  std::vector<std::string> cautions;
};
ExplainableMatch explainable_match(const CarRow& car, const Preferences& prefs);

/// Детализация TCO/год (та же модель, что estimated_annual_tco_rub).
struct TcoBreakdown {
  int km_per_year = 15000;
  int fuel_energy_year_rub = 0;
  int tax_insurance_year_rub = 0;
  int total_year_rub = 0;
};
TcoBreakdown tco_breakdown(const CarRow& car, int km_per_year = 15000);

