#pragma once

#include "client_tools.h"
#include "html.h"
#include "reco.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct SessionState {
  Preferences prefs;
  std::vector<html::ChatMessage> history;
  /// Последние до 3 id из топа рекомендаций (мастер/чат) — для /compare без параметров.
  std::vector<int64_t> last_top_rec_ids;
  /// Избранное (порядок добавления), сердечко в подборе.
  std::vector<int64_t> favorite_car_ids;

  /// Слепой тест на странице «Лаборатория»: три id и последний выбор (0..2 или −1).
  std::vector<int64_t> blindtest_ids;
  int blindtest_pick = -1;

  /// «Мой авто» для блока сравнения с топом (мастер / чат).
  MyCarProfile my_car;

  /// Данные клиента, режим обучения, заметки менеджера (не влияют на подбор).
  ClientProfile client;
};

class SessionStore {
 public:
  SessionState& get_or_create(const std::map<std::string, std::string>& cookies, std::string& set_cookie_header_out);

 private:
  std::map<std::string, SessionState> store_;
  std::string new_id();
};

