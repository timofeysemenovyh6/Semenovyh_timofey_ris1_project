#include "session.h"

#include <random>
#include <sstream>

std::string SessionStore::new_id() {
  static std::mt19937_64 rng(std::random_device{}());
  static std::uniform_int_distribution<uint64_t> dist;
  uint64_t a = dist(rng);
  uint64_t b = dist(rng);
  std::ostringstream o;
  o << std::hex << a << b;
  return o.str();
}

SessionState& SessionStore::get_or_create(const std::map<std::string, std::string>& cookies,
                                          std::string& set_cookie_header_out) {
  set_cookie_header_out.clear();
  auto it = cookies.find("sid");
  if (it != cookies.end()) {
    auto jt = store_.find(it->second);
    if (jt != store_.end()) return jt->second;
  }

  std::string sid = new_id();
  auto [ins, ok] = store_.insert({sid, SessionState{}});
  (void)ok;
  set_cookie_header_out = "sid=" + sid + "; Path=/; HttpOnly; SameSite=Lax";

  auto& st = ins->second;
  st.history.push_back({"assistant",
                        "Задавайте по одному уточнению за раз — я буду обновлять фильтры и задавать следующий наводящий вопрос. "
                        "Можно начать с бюджета или с задачи (например: «большая семья, 7 мест»)."});
  return st;
}

