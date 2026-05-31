#pragma once

#include "client_tools.h"
#include "db.h"
#include "reco.h"

#include <string>
#include <vector>

namespace html {

std::string escape(const std::string& s);
// nav_active: "home" | "wizard" | "chat" | "compare" | "favorites" | "admin"
std::string layout(const std::string& title, const std::string& body, const std::string& nav_active);

std::string home_page();

std::string cars_list_page(const std::vector<CarRow>& cars, const std::string& flash = "");
std::string car_form_page(const std::string& title, const std::string& action, const CarRow* car, const std::string& error = "");

std::string wizard_page(const Preferences& prefs, const std::vector<ScoredCar>& recs, const std::vector<WizardQuestion>& followups,
                         const std::vector<int64_t>& favorite_ids, const std::string& sensitivity_html = "",
                         const MyCarProfile& my_car = MyCarProfile{}, const std::string& my_car_save = "/wizard/my-car",
                         const std::string& my_car_clear = "/wizard/my-car/clear",
                         const ClientProfile& client = ClientProfile{});

struct ChatMessage {
  std::string role; // "user" or "assistant"
  std::string text;
};

std::string chat_page(const std::vector<ChatMessage>& history, const Preferences& prefs, const std::vector<ScoredCar>& recs,
                      const std::vector<int64_t>& favorite_ids, const std::string& sensitivity_html = "",
                      const MyCarProfile& my_car = MyCarProfile{}, const std::string& my_car_save = "/chat/my-car",
                      const std::string& my_car_clear = "/chat/my-car/clear",
                      const ClientProfile& client = ClientProfile{});

// Избранные авто (сердечко в подборе); на странице — сравнение до трёх первых.
std::string favorites_page(const std::vector<CarRow>& cars, const Preferences& prefs, const std::vector<int64_t>& favorite_ids);

// Сравнение до 3 машин. from_session_top — взяты id последнего топа из сессии (раздел «Сравнение» без ?ids=).
std::string compare_page(const std::vector<CarRow>& cars, const Preferences& prefs, bool from_session_top = false);

// Доп. оценки и мини-игра — не меняют подбор в мастере/чате.
std::string extras_page(const Preferences& prefs, const std::vector<ScoredCar>& insight_top3, const std::vector<CarRow>& blind_three,
                         int blind_pick, const std::string& sensitivity_html = "");

// «Лифт бюджета» + «один рычаг» + ссылка на экспорт (разметка без изменения prefs).
std::string build_sensitivity_export_block(const Preferences& prefs, const std::vector<CarRow>& all_cars,
                                          const std::vector<ScoredCar>& recs_context);

// Markdown-отчёт для скачивания (фильтры, топ, Авито, 5 лет, снежный индекс).
std::string markdown_export_report(const Preferences& prefs, const std::vector<ScoredCar>& top_recs);

// «Мой авто → топ»: форма + таблица отличий (багажник, расход, TCO, «5 лет»).
std::string my_car_vs_top_block(const MyCarProfile& mc, const Preferences& prefs, const std::vector<ScoredCar>& top_recs,
                                 const std::string& save_action, const std::string& clear_action);

} // namespace html

