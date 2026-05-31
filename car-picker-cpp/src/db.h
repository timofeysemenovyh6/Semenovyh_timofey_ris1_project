#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct CarRow {
  int64_t id = 0;
  std::string name;
  std::string brand;             // марка (для фильтра и списка)
  int price = 0;                 // руб (цена в объявлениях)
  double fuel_consumption = 0.0; // л/100км (0 — электро)
  std::string body_type;
  std::string gearbox;
  std::string drive;
  int seats = 0;
  int trunk = 0;
  int power = 0;
  int max_speed_kmh = 0;         // максимальная скорость, км/ч
  bool is_electric = false;
  std::string segment;           // economy | mass | premium
};

// Оценка Vmax по мощности/типу (если в форме не задали вручную).
int default_max_speed_kmh(int power, bool is_electric, const std::string& body_type);

class Db {
 public:
  explicit Db(std::string path);
  ~Db();

  Db(const Db&) = delete;
  Db& operator=(const Db&) = delete;

  bool open();
  void close();

  bool init_schema();
  // Синхронизирует каталог с текущей версией приложения (при обновлении — пересев без ручного удаления cars.db).
  bool sync_catalog();

  std::vector<CarRow> list_cars();
  std::optional<CarRow> get_car(int64_t id);
  bool create_car(const CarRow& car);
  bool update_car(int64_t id, const CarRow& car);
  bool delete_car(int64_t id);

 private:
  std::string path_;
  void* db_ = nullptr; // sqlite3*
};

