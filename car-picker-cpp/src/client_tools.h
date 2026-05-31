#pragma once

#include "reco.h"

#include <map>
#include <string>
#include <vector>

/// Данные клиента и заметки менеджера для отчёта (не влияют на подбор).
struct ClientProfile {
  std::string name;
  std::string phone;
  std::string email;
  std::string notes;
  std::string manager_notes;
  bool education_mode = false;
};

struct EducationTopic {
  std::string id;
  std::string title;
  std::string teaser;
  std::string body_html;
};

std::vector<EducationTopic> education_topics();

const EducationTopic* education_topic_by_id(const std::string& id);

/// Краткая подсказка по теме, если режим обучения включён (пустая — не показывать).
std::string education_hint_for_message_ru(const std::string& user_message, bool education_mode);

/// Текстовое описание критериев для отчёта (маркированный список, markdown).
std::string prefs_criteria_markdown(const Preferences& prefs);

/// Полный отчёт для клиента (markdown).
std::string build_client_report_markdown(const ClientProfile& client, const Preferences& prefs,
                                         const std::vector<ScoredCar>& top_recs);

/// HTML-панель: справочник + форма клиента + ссылки на отчёт (свёрнута по умолчанию).
std::string client_tools_panel_html(const ClientProfile& client, const std::string& save_action);

/// Отдельная страница «Справочник и отчёт».
std::string client_tools_page(const ClientProfile& client, const Preferences& prefs, const std::vector<ScoredCar>& top3);

void apply_client_profile_form(ClientProfile& client, const std::map<std::string, std::string>& form);

/// Карточки «анализ TCO + объяснимые рекомендации» для топ‑3 (после подбора).
std::string top3_decision_insights_html(const Preferences& prefs, const std::vector<ScoredCar>& recs);
