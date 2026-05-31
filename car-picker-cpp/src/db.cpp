#include "db.h"

#include <sqlite3.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

static bool exec_sql(sqlite3* db, const char* sql) {
  char* err = nullptr;
  int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
  if (rc != SQLITE_OK) {
    if (err) {
      std::cerr << "sqlite error: " << err << "\n";
      sqlite3_free(err);
    }
    return false;
  }
  return true;
}

static bool has_column(sqlite3* db, const char* table, const char* col) {
  std::string sql = "PRAGMA table_info(" + std::string(table) + ");";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) return false;
  bool found = false;
  while (sqlite3_step(st) == SQLITE_ROW) {
    const char* cn = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
    if (cn && std::strcmp(cn, col) == 0) found = true;
  }
  sqlite3_finalize(st);
  return found;
}

int default_max_speed_kmh(int power, bool is_electric, const std::string& body) {
  int v = 0;
  if (is_electric) {
    v = static_cast<int>(118.0 + static_cast<double>(power) * 0.38);
    if (power >= 380) v += 28;
  } else {
    v = static_cast<int>(96.0 + static_cast<double>(power) * 0.62);
  }
  if (body == "suv") v -= 7;
  else if (body == "crossover") v -= 4;
  else if (body == "van") v -= 12;
  else if (body == "hatchback") v += 2;
  else if (body == "sedan" || body == "liftback") v += 5;
  else if (body == "wagon") v += 3;
  v = std::clamp(v, 135, is_electric ? 275 : 335);
  return v;
}

static bool migrate_cars_table(sqlite3* db) {
  if (!has_column(db, "cars", "brand")) {
    if (!exec_sql(db, "ALTER TABLE cars ADD COLUMN brand TEXT NOT NULL DEFAULT '';")) return false;
  }
  if (!has_column(db, "cars", "is_electric")) {
    if (!exec_sql(db, "ALTER TABLE cars ADD COLUMN is_electric INTEGER NOT NULL DEFAULT 0;")) return false;
  }
  if (!has_column(db, "cars", "segment")) {
    if (!exec_sql(db, "ALTER TABLE cars ADD COLUMN segment TEXT NOT NULL DEFAULT 'mass';")) return false;
  }
  if (!has_column(db, "cars", "max_speed")) {
    if (!exec_sql(db, "ALTER TABLE cars ADD COLUMN max_speed INTEGER NOT NULL DEFAULT 0;")) return false;
    if (!exec_sql(db,
                  "UPDATE cars SET max_speed = CASE WHEN is_electric != 0 THEN "
                  "MIN(275, MAX(155, 118 + (power * 38) / 100 + CASE WHEN power >= 380 THEN 28 ELSE 0 END)) "
                  "ELSE MIN(330, MAX(145, 96 + (power * 62) / 100)) END;"))
      return false;
    if (!exec_sql(db,
                  "UPDATE cars SET max_speed = max_speed - 7 WHERE body_type = 'suv' AND max_speed > 140;"))
      return false;
    if (!exec_sql(db,
                  "UPDATE cars SET max_speed = max_speed - 4 WHERE body_type = 'crossover' AND max_speed > 140;"))
      return false;
    if (!exec_sql(db,
                  "UPDATE cars SET max_speed = max_speed - 12 WHERE body_type = 'van' AND max_speed > 140;"))
      return false;
    if (!exec_sql(db,
                  "UPDATE cars SET max_speed = max_speed + 5 WHERE body_type IN ('sedan','liftback') AND max_speed < 330;"))
      return false;
    if (!exec_sql(db,
                  "UPDATE cars SET max_speed = max_speed + 2 WHERE body_type = 'hatchback' AND max_speed < 330;"))
      return false;
    if (!exec_sql(db,
                  "UPDATE cars SET max_speed = max_speed + 3 WHERE body_type = 'wagon' AND max_speed < 330;"))
      return false;
  }
  return true;
}

// Старые базы хранили цену в тыс. руб — переводим в полные рубли один раз (PRAGMA user_version = 2).
static bool migrate_prices_to_rubles(sqlite3* db) {
  sqlite3_stmt* st = nullptr;
  int uv = 0;
  if (sqlite3_prepare_v2(db, "PRAGMA user_version;", -1, &st, nullptr) != SQLITE_OK) return false;
  if (sqlite3_step(st) == SQLITE_ROW) uv = sqlite3_column_int(st, 0);
  sqlite3_finalize(st);
  if (uv >= 2) return true;

  int mx = 0;
  if (sqlite3_prepare_v2(db, "SELECT COALESCE(MAX(price),0) FROM cars;", -1, &st, nullptr) != SQLITE_OK) return false;
  if (sqlite3_step(st) == SQLITE_ROW) mx = sqlite3_column_int(st, 0);
  sqlite3_finalize(st);

  if (mx > 0 && mx < 25000) {
    if (!exec_sql(db, "UPDATE cars SET price = price * 1000;")) return false;
  }
  return exec_sql(db, "PRAGMA user_version = 2;");
}

Db::Db(std::string path) : path_(std::move(path)) {}

Db::~Db() { close(); }

bool Db::open() {
  if (db_) return true;
  sqlite3* db = nullptr;
  if (sqlite3_open(path_.c_str(), &db) != SQLITE_OK) {
    if (db) sqlite3_close(db);
    return false;
  }
  db_ = db;
  return true;
}

void Db::close() {
  if (!db_) return;
  sqlite3_close(reinterpret_cast<sqlite3*>(db_));
  db_ = nullptr;
}

bool Db::init_schema() {
  auto* db = reinterpret_cast<sqlite3*>(db_);
  assert(db);
  const char* sql =
      "CREATE TABLE IF NOT EXISTS cars ("
      " id INTEGER PRIMARY KEY AUTOINCREMENT,"
      " name TEXT NOT NULL,"
      " price INTEGER NOT NULL,"
      " fuel_consumption REAL NOT NULL,"
      " body_type TEXT NOT NULL,"
      " gearbox TEXT NOT NULL,"
      " drive TEXT NOT NULL,"
      " seats INTEGER NOT NULL,"
      " trunk INTEGER NOT NULL,"
      " power INTEGER NOT NULL"
      ");";
  if (!exec_sql(db, sql)) return false;
  if (!migrate_cars_table(db)) return false;
  return migrate_prices_to_rubles(db);
}

static int count_cb(void* out, int argc, char** argv, char**) {
  if (argc > 0 && argv[0]) *reinterpret_cast<int*>(out) = std::atoi(argv[0]);
  return 0;
}

static int mix(int i, int m) {
  if (m <= 0) return 0;
  uint32_t x = static_cast<uint32_t>(i) * 2654435761u;
  return static_cast<int>(x % static_cast<uint32_t>(m));
}

// Ориентировочный диапазон года выпуска для пары марка+модель (для сида и ссылок на Авито).
struct CatalogYearSpan {
  const char* brand;
  const char* model;
  int y_min;
  int y_max;
};

static int catalog_plausible_year(const char* brand, const char* model, int salt, bool is_ev) {
  static const CatalogYearSpan k[] = {
      // Недавние / новые поколения
      {"Toyota", "Land Cruiser 300", 2021, 2025},
      {"OMODA", "Omoda 5", 2022, 2025},
      {"Jaecoo", "J7", 2023, 2025},
      {"Tank", "300", 2021, 2025},
      {"Tank", "500", 2021, 2025},
      {"Jetour", "T2", 2023, 2025},
      {"Seres", "5", 2022, 2025},
      {"Zeekr", "001", 2021, 2025},
      {"Geely", "Monjaro", 2022, 2025},
      {"Changan", "UNI-K", 2021, 2025},
      {"Kia", "Seltos", 2020, 2025},
      {"Renault", "Arkana", 2019, 2025},
      {"Lada", "Niva Travel", 2020, 2025},
      {"Haval", "Jolion", 2020, 2025},
      {"Chery", "Tiggo 8", 2019, 2025},
      {"Exeed", "TXL", 2019, 2025},
      {"Exeed", "VX", 2021, 2025},
      {"GWM", "Poer", 2020, 2025},
      {"Hongqi", "H5", 2019, 2025},
      // Электромобили
      {"Tesla", "Model 3", 2019, 2025},
      {"Tesla", "Model Y", 2021, 2025},
      {"Nissan", "Leaf", 2018, 2024},
      {"BYD", "Seal", 2022, 2025},
      {"BYD", "Han", 2020, 2025},
      {"Volkswagen", "ID.4", 2021, 2025},
      {"Volkswagen", "ID.3", 2020, 2025},
      {"Hyundai", "Ioniq 5", 2021, 2025},
      {"Kia", "EV6", 2021, 2025},
      {"BMW", "i4", 2021, 2025},
      {"Mercedes", "EQC", 2019, 2024},
      {"Renault", "Zoe", 2018, 2023},
      {"Renault", "Megane E-Tech", 2022, 2025},
      {"MG", "MG4", 2022, 2025},
      {"Toyota", "bZ4X", 2022, 2025},
      {"Audi", "Q4 e-tron", 2021, 2025},
      // Массовый сегмент (актуальные поколения на вторичке)
      {"Kia", "Rio", 2016, 2024},
      {"Hyundai", "Solaris", 2017, 2025},
      {"Hyundai", "Creta", 2016, 2025},
      {"Kia", "Sportage", 2016, 2025},
      {"Toyota", "Corolla", 2013, 2025},
      {"Toyota", "Camry", 2015, 2025},
      {"Toyota", "RAV4", 2013, 2025},
      {"Toyota", "Highlander", 2014, 2025},
      {"Toyota", "Yaris", 2014, 2024},
      {"Volkswagen", "Polo", 2015, 2025},
      {"Volkswagen", "Golf", 2013, 2025},
      {"Volkswagen", "Tiguan", 2016, 2025},
      {"Volkswagen", "Passat", 2014, 2024},
      {"Skoda", "Octavia", 2013, 2025},
      {"Skoda", "Karoq", 2017, 2025},
      {"Skoda", "Kodiaq", 2016, 2025},
      {"Skoda", "Superb", 2015, 2024},
      {"Nissan", "Qashqai", 2014, 2025},
      {"Nissan", "X-Trail", 2014, 2025},
      {"Nissan", "Murano", 2015, 2024},
      {"Nissan", "Pathfinder", 2014, 2024},
      {"Nissan", "Juke", 2019, 2025},
      {"Mazda", "CX-5", 2012, 2025},
      {"Mazda", "CX-9", 2012, 2024},
      {"Mazda", "3", 2013, 2025},
      {"Mazda", "6", 2013, 2024},
      {"Mitsubishi", "Outlander", 2012, 2025},
      {"Mitsubishi", "ASX", 2012, 2024},
      {"Ford", "Focus", 2011, 2022},
      {"Ford", "Mondeo", 2014, 2022},
      {"Ford", "Kuga", 2012, 2025},
      {"Opel", "Corsa", 2015, 2025},
      {"Opel", "Astra", 2015, 2025},
      {"Peugeot", "308", 2014, 2025},
      {"Peugeot", "3008", 2016, 2025},
      {"Peugeot", "5008", 2017, 2025},
      {"Peugeot", "208", 2019, 2025},
      {"Citroen", "C4", 2016, 2025},
      {"Citroen", "C5", 2010, 2017},
      {"Citroen", "C5 Aircross", 2017, 2025},
      {"Honda", "Civic", 2012, 2025},
      {"Honda", "Accord", 2013, 2024},
      {"Honda", "CR-V", 2012, 2025},
      {"Subaru", "Impreza", 2012, 2024},
      {"Subaru", "Forester", 2012, 2025},
      {"Subaru", "XV", 2012, 2024},
      {"Suzuki", "SX4", 2013, 2022},
      {"Suzuki", "Swift", 2013, 2024},
      {"Suzuki", "Vitara", 2015, 2025},
      // Премиум
      {"BMW", "320i", 2015, 2025},
      {"BMW", "520i", 2016, 2025},
      {"BMW", "X3", 2014, 2025},
      {"BMW", "X5", 2014, 2025},
      {"BMW", "X1", 2015, 2025},
      {"Mercedes", "C200", 2014, 2025},
      {"Mercedes", "E220", 2016, 2025},
      {"Mercedes", "GLC", 2015, 2025},
      {"Mercedes", "Sprinter", 2014, 2024},
      {"Audi", "A4", 2015, 2025},
      {"Audi", "A6", 2015, 2025},
      {"Audi", "Q5", 2012, 2025},
      {"Audi", "Q3", 2014, 2025},
      {"Lexus", "NX", 2014, 2025},
      {"Lexus", "ES", 2015, 2025},
      {"Volvo", "XC60", 2013, 2025},
      {"Volvo", "XC90", 2015, 2025},
      {"Genesis", "G70", 2017, 2025},
      {"Infiniti", "QX50", 2015, 2024},
      {"Toyota", "Alphard", 2015, 2024},
      {"Volkswagen", "Multivan", 2015, 2025},
      {"Mercedes", "V-Class", 2014, 2024},
      {"Kia", "Carnival", 2015, 2025},
      {"Volkswagen", "Caddy", 2012, 2024},
      // Китайский сегмент
      {"Chery", "Tiggo", 2016, 2023},
      {"Chery", "Tiggo 8 Pro", 2020, 2025},
      {"Geely", "Coolray", 2019, 2025},
      {"Haval", "F7", 2019, 2024},
      {"Haval", "F7x", 2019, 2024},
      {"Haval", "H9", 2014, 2023},
      {"Changan", "CS75", 2018, 2025},
      // Бюджет / отечественный
      {"Renault", "Duster", 2011, 2025},
      {"Renault", "Logan", 2010, 2022},
      {"Renault", "Sandero", 2014, 2025},
      {"Lada", "Vesta", 2015, 2025},
      {"Lada", "Granta", 2013, 2025},
      {"Lada", "Vesta SW", 2015, 2025},
      {"UAZ", "Patriot", 2014, 2024},
      {"UAZ", "Hunter", 2012, 2023},
      {"Hyundai", "Tucson", 2015, 2025},
      {"Hyundai", "Santa Fe", 2012, 2025},
      {"Kia", "Ceed", 2012, 2025},
      {"Kia", "Cerato", 2013, 2025},
      {"Ford", "Transit", 2014, 2024},
      {"Citroen", "Berlingo", 2012, 2024},
      {"Toyota", "RAV4 Hybrid", 2019, 2025},
  };
  for (const auto& e : k) {
    if (std::strcmp(brand, e.brand) == 0 && std::strcmp(model, e.model) == 0) {
      const int span = e.y_max - e.y_min + 1;
      return e.y_min + mix(salt, span);
    }
  }
  if (is_ev) return 2020 + mix(salt, 6);
  return 2016 + mix(salt, 8);
}

// Марка+модель из сида, у которых на рынке реально бывают 7-местные SUV/кроссоверы (не «7-местный Rio»).
static bool catalog_seven_suv_model(const char* brand, const char* model) {
  static const struct {
    const char* b;
    const char* m;
  } k[] = {
      {"Toyota", "Highlander"},   {"Toyota", "Land Cruiser 300"}, {"Nissan", "Pathfinder"},
      {"Nissan", "X-Trail"},       {"Mitsubishi", "Outlander"},   {"Skoda", "Kodiaq"},
      {"Mazda", "CX-9"},           {"Peugeot", "5008"},           {"Volkswagen", "Tiguan"},
      {"Hyundai", "Santa Fe"},     {"Volvo", "XC90"},             {"BMW", "X5"},
      {"Chery", "Tiggo 8"},        {"Geely", "Monjaro"},          {"Haval", "H9"},
      {"Exeed", "VX"},             {"Tank", "300"},               {"Tank", "500"},
      {"UAZ", "Patriot"},
  };
  for (const auto& e : k) {
    if (std::strcmp(brand, e.b) == 0 && std::strcmp(model, e.m) == 0) return true;
  }
  return false;
}

// Реальный тип кузова для пар из paired[] / ev_only[] (раньше body_type брался случайно → «crossover» у Polo/320i).
static const char* catalog_body_for_pair(const char* brand, const char* model) {
  static const struct {
    const char* b;
    const char* m;
    const char* body;
  } t[] = {
      {"Kia", "Rio", "hatchback"},
      {"Hyundai", "Solaris", "sedan"},
      {"Hyundai", "Creta", "crossover"},
      {"Kia", "Sportage", "crossover"},
      {"Kia", "Seltos", "crossover"},
      {"Toyota", "Corolla", "sedan"},
      {"Toyota", "Camry", "sedan"},
      {"Toyota", "RAV4", "crossover"},
      {"Toyota", "Highlander", "crossover"},
      {"Volkswagen", "Polo", "hatchback"},
      {"Skoda", "Octavia", "liftback"},
      {"Skoda", "Karoq", "crossover"},
      {"Skoda", "Kodiaq", "crossover"},
      {"Nissan", "X-Trail", "crossover"},
      {"Nissan", "Murano", "crossover"},
      {"Nissan", "Qashqai", "crossover"},
      {"Nissan", "Pathfinder", "suv"},
      {"Mazda", "CX-5", "crossover"},
      {"Mazda", "CX-9", "crossover"},
      {"Mitsubishi", "Outlander", "crossover"},
      {"Mitsubishi", "ASX", "crossover"},
      {"Ford", "Focus", "hatchback"},
      {"Ford", "Mondeo", "liftback"},
      {"Ford", "Kuga", "crossover"},
      {"Opel", "Corsa", "hatchback"},
      {"Opel", "Astra", "hatchback"},
      {"Peugeot", "308", "hatchback"},
      {"Peugeot", "3008", "crossover"},
      {"Peugeot", "5008", "crossover"},
      {"Citroen", "C4", "hatchback"},
      {"Citroen", "C5", "sedan"},
      {"Citroen", "C5 Aircross", "crossover"},
      {"Honda", "Civic", "hatchback"},
      {"Honda", "Accord", "sedan"},
      {"Honda", "CR-V", "crossover"},
      {"Subaru", "Impreza", "hatchback"},
      {"Subaru", "Forester", "crossover"},
      {"Suzuki", "SX4", "crossover"},
      {"Suzuki", "Swift", "hatchback"},
      {"BMW", "320i", "sedan"},
      {"BMW", "520i", "sedan"},
      {"BMW", "X3", "crossover"},
      {"BMW", "X5", "suv"},
      {"Mercedes", "C200", "sedan"},
      {"Mercedes", "E220", "sedan"},
      {"Mercedes", "GLC", "crossover"},
      {"Audi", "A4", "sedan"},
      {"Audi", "A6", "sedan"},
      {"Audi", "Q5", "crossover"},
      {"Lexus", "NX", "crossover"},
      {"Lexus", "ES", "sedan"},
      {"Volvo", "XC60", "crossover"},
      {"Volvo", "XC90", "suv"},
      {"Chery", "Tiggo", "crossover"},
      {"Chery", "Tiggo 8", "crossover"},
      {"Geely", "Coolray", "crossover"},
      {"Geely", "Monjaro", "crossover"},
      {"Haval", "Jolion", "crossover"},
      {"Haval", "F7", "crossover"},
      {"Haval", "H9", "suv"},
      {"Changan", "CS75", "crossover"},
      {"Changan", "UNI-K", "crossover"},
      {"Exeed", "TXL", "crossover"},
      {"Exeed", "VX", "crossover"},
      {"Renault", "Duster", "crossover"},
      {"Renault", "Arkana", "crossover"},
      {"Lada", "Vesta", "sedan"},
      {"Lada", "Granta", "sedan"},
      {"Lada", "Niva Travel", "suv"},
      {"UAZ", "Patriot", "suv"},
      {"UAZ", "Hunter", "suv"},
      {"GWM", "Poer", "suv"},
      {"Tank", "300", "suv"},
      {"Tank", "500", "suv"},
      {"OMODA", "Omoda 5", "crossover"},
      {"Jaecoo", "J7", "crossover"},
      {"BYD", "Han", "liftback"},
      {"BYD", "Seal", "sedan"},
      {"Tesla", "Model 3", "sedan"},
      {"Zeekr", "001", "liftback"},
      {"Volkswagen", "Tiguan", "crossover"},
      {"Volkswagen", "Passat", "wagon"},
      {"Hyundai", "Tucson", "crossover"},
      {"Hyundai", "Santa Fe", "crossover"},
      {"Kia", "Ceed", "hatchback"},
      {"Kia", "Cerato", "sedan"},
      {"Toyota", "Land Cruiser 300", "suv"},
      {"Nissan", "Juke", "crossover"},
      {"Mazda", "3", "hatchback"},
      {"Mazda", "6", "sedan"},
      {"Renault", "Logan", "sedan"},
      {"Renault", "Sandero", "hatchback"},
      {"Volkswagen", "Golf", "hatchback"},
      {"Skoda", "Superb", "liftback"},
      {"Mercedes", "Sprinter", "van"},
      {"Ford", "Transit", "van"},
      {"Peugeot", "208", "hatchback"},
      {"Citroen", "Berlingo", "van"},
      {"Toyota", "Yaris", "hatchback"},
      {"Suzuki", "Vitara", "crossover"},
      {"Subaru", "XV", "crossover"},
      {"BMW", "X1", "crossover"},
      {"Audi", "Q3", "crossover"},
      {"Genesis", "G70", "sedan"},
      {"Infiniti", "QX50", "crossover"},
      {"Hongqi", "H5", "sedan"},
      {"Seres", "5", "sedan"},
      {"Jetour", "T2", "suv"},
      {"Tesla", "Model Y", "crossover"},
      {"Nissan", "Leaf", "hatchback"},
      {"Volkswagen", "ID.4", "crossover"},
      {"Hyundai", "Ioniq 5", "crossover"},
      {"Kia", "EV6", "crossover"},
      {"BMW", "i4", "liftback"},
      {"Mercedes", "EQC", "crossover"},
      {"Renault", "Zoe", "hatchback"},
      {"Renault", "Megane E-Tech", "hatchback"},
      {"MG", "MG4", "hatchback"},
      {"Volkswagen", "ID.3", "hatchback"},
      {"Toyota", "bZ4X", "crossover"},
      {"Audi", "Q4 e-tron", "crossover"},
      {"Toyota", "Alphard", "van"},
      {"Volkswagen", "Multivan", "van"},
      {"Mercedes", "V-Class", "van"},
      {"Kia", "Carnival", "van"},
      {"Volkswagen", "Caddy", "van"},
      {"Chery", "Tiggo 8 Pro", "crossover"},
      {"Haval", "F7x", "crossover"},
      {"Lada", "Vesta SW", "wagon"},
      {"Toyota", "RAV4 Hybrid", "crossover"},
  };
  for (const auto& e : t) {
    if (std::strcmp(brand, e.b) == 0 && std::strcmp(model, e.m) == 0) return e.body;
  }
  return nullptr;
}

static bool seed_big(sqlite3* db) {
  const char* sql =
      "INSERT INTO cars(name,brand,price,fuel_consumption,body_type,gearbox,drive,seats,trunk,power,max_speed,is_electric,segment) "
      "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?);";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return false;

  // Согласованные пары марка+модель (раньше марка и модель брались из разных массивов → «BMW Rio» и т.п.).
  struct BrandModel {
    const char* brand;
    const char* model;
  };
  static const BrandModel paired[] = {
      {"Kia", "Rio"},           {"Hyundai", "Solaris"},   {"Hyundai", "Creta"},     {"Kia", "Sportage"},
      {"Kia", "Seltos"},        {"Toyota", "Corolla"},    {"Toyota", "Camry"},      {"Toyota", "RAV4"},
      {"Toyota", "Highlander"}, {"Volkswagen", "Polo"},   {"Skoda", "Octavia"},     {"Skoda", "Karoq"},
      {"Skoda", "Kodiaq"},      {"Nissan", "X-Trail"},    {"Nissan", "Murano"},     {"Nissan", "Qashqai"},
      {"Nissan", "Pathfinder"}, {"Mazda", "CX-5"},        {"Mazda", "CX-9"},        {"Mitsubishi", "Outlander"},
      {"Mitsubishi", "ASX"},    {"Ford", "Focus"},        {"Ford", "Mondeo"},       {"Ford", "Kuga"},
      {"Opel", "Corsa"},        {"Opel", "Astra"},        {"Peugeot", "308"},      {"Peugeot", "3008"},
      {"Peugeot", "5008"},      {"Citroen", "C4"},        {"Citroen", "C5"},        {"Citroen", "C5 Aircross"},
      {"Honda", "Civic"},       {"Honda", "Accord"},      {"Honda", "CR-V"},        {"Subaru", "Impreza"},
      {"Subaru", "Forester"},   {"Suzuki", "SX4"},        {"Suzuki", "Swift"},      {"BMW", "320i"},
      {"BMW", "520i"},          {"BMW", "X3"},            {"BMW", "X5"},            {"Mercedes", "C200"},
      {"Mercedes", "E220"},     {"Mercedes", "GLC"},      {"Audi", "A4"},         {"Audi", "A6"},
      {"Audi", "Q5"},           {"Lexus", "NX"},          {"Lexus", "ES"},        {"Volvo", "XC60"},
      {"Volvo", "XC90"},        {"Chery", "Tiggo"},       {"Chery", "Tiggo 8"},    {"Geely", "Coolray"},
      {"Geely", "Monjaro"},     {"Haval", "Jolion"},      {"Haval", "F7"},        {"Haval", "H9"},
      {"Changan", "CS75"},      {"Changan", "UNI-K"},     {"Exeed", "TXL"},       {"Exeed", "VX"},
      {"Renault", "Duster"},    {"Renault", "Arkana"},    {"Lada", "Vesta"},      {"Lada", "Granta"},
      {"Lada", "Niva Travel"},  {"UAZ", "Patriot"},       {"UAZ", "Hunter"},      {"GWM", "Poer"},
      {"Tank", "300"},          {"Tank", "500"},          {"OMODA", "Omoda 5"},   {"Jaecoo", "J7"},
      {"BYD", "Han"},           {"BYD", "Seal"},          {"Tesla", "Model 3"},   {"Zeekr", "001"},
      {"Volkswagen", "Tiguan"}, {"Volkswagen", "Passat"}, {"Hyundai", "Tucson"},  {"Hyundai", "Santa Fe"},
      {"Kia", "Ceed"},          {"Kia", "Cerato"},        {"Toyota", "Land Cruiser 300"}, {"Nissan", "Juke"},
      {"Mazda", "3"},           {"Mazda", "6"},           {"Renault", "Logan"},   {"Renault", "Sandero"},
      {"Volkswagen", "Golf"},   {"Skoda", "Superb"},      {"Mercedes", "Sprinter"}, {"Ford", "Transit"},
      {"Peugeot", "208"},       {"Citroen", "Berlingo"},  {"Toyota", "Yaris"},    {"Suzuki", "Vitara"},
      {"Subaru", "XV"},         {"BMW", "X1"},            {"Audi", "Q3"},         {"Genesis", "G70"},
      {"Infiniti", "QX50"},     {"Hongqi", "H5"},         {"Seres", "5"},         {"Jetour", "T2"},
  };
  static const int NP = static_cast<int>(sizeof paired / sizeof paired[0]);
  // Только реальные BEV/осознанно «электро» модели — иначе в базе оказываются «электро Дастер» и
  // поиск на Авито по марке+«электро» уводит на другие модели (Koleos E-Tech и т.д.).
  static const BrandModel ev_only[] = {
      {"Tesla", "Model 3"},       {"Tesla", "Model Y"},      {"Nissan", "Leaf"},
      {"BYD", "Seal"},            {"BYD", "Han"},           {"Zeekr", "001"},
      {"Volkswagen", "ID.4"},   {"Hyundai", "Ioniq 5"},   {"Kia", "EV6"},
      {"BMW", "i4"},            {"Mercedes", "EQC"},      {"Renault", "Zoe"},
      {"Renault", "Megane E-Tech"}, {"MG", "MG4"},       {"Volkswagen", "ID.3"},
      {"Toyota", "bZ4X"},       {"Audi", "Q4 e-tron"},
  };
  static const int NEV = static_cast<int>(sizeof ev_only / sizeof ev_only[0]);
  const char* bodies[] = {"sedan", "hatchback", "crossover", "wagon", "suv", "van", "liftback"};
  const int NBodies = static_cast<int>(sizeof bodies / sizeof bodies[0]);

  auto step_ins = [&](const char* name, const char* brand, int price, double fuel, const char* body, const char* gb,
                      const char* dr, int seats, int trunk, int power, int max_sp, int ev, const char* seg) -> bool {
    sqlite3_bind_text(st, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, brand, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 3, price);
    sqlite3_bind_double(st, 4, fuel);
    sqlite3_bind_text(st, 5, body, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 6, gb, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 7, dr, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 8, seats);
    sqlite3_bind_int(st, 9, trunk);
    sqlite3_bind_int(st, 10, power);
    sqlite3_bind_int(st, 11, max_sp);
    sqlite3_bind_int(st, 12, ev);
    sqlite3_bind_text(st, 13, seg, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    sqlite3_reset(st);
    return rc == SQLITE_DONE;
  };

  if (sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr) != SQLITE_OK) {
    sqlite3_finalize(st);
    return false;
  }

  // Небольшой набор «узнаваемых» конфигураций вперемешку с генерацией
  struct Row {
    const char* name;
    const char* brand;
    int price;
    double fuel;
    const char* body;
    const char* gb;
    const char* drive;
    int seats;
    int trunk;
    int power;
    int max_speed;
    int ev;
    const char* seg;
  };
  // name = «модель год»; brand — отдельно (без «BMW Rio» и дубля марки в ссылке Авито).
  const Row curated[] = {
      {"Solaris 2022", "Hyundai", 1580000, 7.0, "sedan", "at", "fwd", 5, 480, 123, 188, 0, "mass"},
      {"Rio 2021", "Kia", 1620000, 7.2, "hatchback", "at", "fwd", 5, 480, 123, 186, 0, "mass"},
      {"Camry 2020", "Toyota", 3150000, 8.3, "sedan", "at", "fwd", 5, 493, 181, 214, 0, "mass"},
      {"Multivan 2019", "Volkswagen", 5450000, 7.8, "van", "at", "fwd", 7, 900, 204, 200, 0, "premium"},
      {"Model Y 2023", "Tesla", 6200000, 0.0, "crossover", "at", "awd", 5, 854, 351, 250, 1, "premium"},
      {"Leaf 2020", "Nissan", 2550000, 0.0, "hatchback", "at", "fwd", 5, 370, 150, 158, 1, "mass"},
      {"Vesta SW 2021", "Lada", 1280000, 7.4, "wagon", "mt", "fwd", 5, 495, 106, 178, 0, "economy"},
      {"Alphard 2018", "Toyota", 6850000, 9.5, "van", "at", "fwd", 7, 700, 275, 196, 0, "premium"},
      {"Tiggo 8 Pro 2022", "Chery", 2380000, 8.1, "crossover", "robot", "fwd", 7, 515, 186, 198, 0, "mass"},
      {"F7x 2021", "Haval", 1990000, 8.4, "crossover", "robot", "awd", 5, 415, 190, 198, 0, "mass"},
      {"Seal 2023", "BYD", 4150000, 0.0, "sedan", "at", "rwd", 5, 450, 313, 222, 1, "premium"},
      {"X5 2020", "BMW", 6150000, 8.5, "suv", "at", "awd", 5, 650, 286, 228, 0, "premium"},
      {"Octavia 2021", "Skoda", 1780000, 6.4, "liftback", "at", "fwd", 5, 640, 150, 206, 0, "mass"},
      {"CX-5 2019", "Mazda", 2850000, 8.2, "crossover", "at", "awd", 5, 505, 150, 191, 0, "mass"},
      {"Logan 2020", "Renault", 1050000, 7.2, "sedan", "mt", "fwd", 5, 510, 82, 157, 0, "economy"},
      {"V-Class 2019", "Mercedes", 5900000, 8.0, "van", "at", "rwd", 7, 850, 239, 198, 0, "premium"},
      {"Carnival 2021", "Kia", 3850000, 8.6, "van", "at", "fwd", 8, 720, 249, 204, 0, "premium"},
      {"Caddy 2018", "Volkswagen", 2650000, 6.7, "van", "mt", "fwd", 7, 530, 110, 182, 0, "mass"},
      {"RAV4 2022", "Toyota", 3380000, 5.1, "crossover", "cvt", "awd", 5, 580, 178, 191, 0, "mass"},
      {"001 2023", "Zeekr", 5150000, 0.0, "liftback", "at", "awd", 5, 540, 400, 268, 1, "premium"},
  };
  for (const auto& r : curated) {
    if (!step_ins(r.name, r.brand, r.price, r.fuel, r.body, r.gb, r.drive, r.seats, r.trunk, r.power, r.max_speed, r.ev,
                  r.seg)) {
      sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
      sqlite3_finalize(st);
      return false;
    }
  }

  for (int i = 0; i < 220; ++i) {
    int evroll = mix(i + 19, 100);
    bool ev = evroll < 7;
    const BrandModel& pm = ev ? ev_only[mix(i * 29 + 3, NEV)] : paired[mix(i * 13 + 5, NP)];
    const char* brand = pm.brand;
    char name[200];
    const int year = catalog_plausible_year(brand, pm.model, i, ev);
    std::snprintf(name, sizeof name, "%s %d", pm.model, year);
    int price = 780000 + mix(i * 17 + 5, 7200000);
    if (price < 920000) price += 180000;
    double fuel = ev ? 0.0 : (5.0 + mix(i + 1, 42) / 10.0);
    const char* body = catalog_body_for_pair(brand, pm.model);
    if (!body) body = bodies[mix(i + 2, NBodies)];
    const char* gb = mix(i, 10) < 5 ? "at" : (mix(i, 10) < 8 ? "mt" : (mix(i, 10) < 9 ? "cvt" : "robot"));
    const char* dr = mix(i + 4, 10) < 6 ? "fwd" : (mix(i + 4, 10) < 9 ? "awd" : "rwd");
    int seats = 5;
    if (std::strcmp(body, "van") == 0) {
      seats = 7 + mix(i, 2);
      if (seats > 8) seats = 8;
      dr = mix(i, 3) == 0 ? "fwd" : "awd";
    } else if ((std::strcmp(body, "suv") == 0 || std::strcmp(body, "crossover") == 0) &&
               catalog_seven_suv_model(brand, pm.model) && mix(i + 31, 4) == 0) {
      // Часть строк — 7 мест только для моделей с реальным третьим рядом.
      seats = 7;
    }
    int trunk = 300 + mix(i + 11, 520);
    if (std::strcmp(body, "van") == 0) trunk = 480 + mix(i + 5, 420);
    if (std::strcmp(body, "hatchback") == 0) trunk = 260 + mix(i + 9, 200);
    if (std::strcmp(body, "sedan") == 0) trunk = 380 + mix(i + 21, 220);
    int power = 85 + mix(i + 13, 320);
    if (ev) power = 110 + mix(i, 290);
    const int vmax = default_max_speed_kmh(power, ev, std::string(body));
    const char* seg = price < 1600000 ? "economy" : (price < 4200000 ? "mass" : "premium");
    if (!step_ins(name, brand, price, fuel, body, gb, dr, seats, trunk, power, vmax, ev ? 1 : 0, seg)) {
      sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
      sqlite3_finalize(st);
      return false;
    }
  }

  if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
    sqlite3_finalize(st);
    return false;
  }
  sqlite3_finalize(st);
  return true;
}

// Увеличьте, чтобы при следующем запуске заново залить каталог (цены в рублях, ~240 авто и т.д.).
static constexpr int kCatalogVersion = 11;

static bool upsert_meta_int(sqlite3* db, const char* key, int val) {
  const char* sql = "INSERT INTO app_meta(k,v) VALUES(?,?) ON CONFLICT(k) DO UPDATE SET v=excluded.v;";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(st, 1, key, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(st, 2, val);
  bool ok = (sqlite3_step(st) == SQLITE_DONE);
  sqlite3_finalize(st);
  return ok;
}

static int read_meta_int(sqlite3* db, const char* key, int def_val) {
  const char* sql = "SELECT v FROM app_meta WHERE k=?;";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return def_val;
  sqlite3_bind_text(st, 1, key, -1, SQLITE_TRANSIENT);
  int out = def_val;
  if (sqlite3_step(st) == SQLITE_ROW) out = sqlite3_column_int(st, 0);
  sqlite3_finalize(st);
  return out;
}

bool Db::sync_catalog() {
  auto* db = reinterpret_cast<sqlite3*>(db_);
  assert(db);
  if (!exec_sql(db, "CREATE TABLE IF NOT EXISTS app_meta (k TEXT PRIMARY KEY, v INTEGER NOT NULL);")) return false;

  const int stored = read_meta_int(db, "catalog_v", 0);
  if (stored < kCatalogVersion) {
    if (!exec_sql(db, "DELETE FROM cars;")) return false;
    if (!seed_big(db)) return false;
    if (!upsert_meta_int(db, "catalog_v", kCatalogVersion)) return false;
    return true;
  }

  int cnt = 0;
  char* err = nullptr;
  if (sqlite3_exec(db, "SELECT COUNT(*) FROM cars;", count_cb, &cnt, &err) != SQLITE_OK) {
    if (err) sqlite3_free(err);
    return false;
  }
  if (cnt == 0) return seed_big(db);
  return true;
}

std::vector<CarRow> Db::list_cars() {
  std::vector<CarRow> out;
  auto* db = reinterpret_cast<sqlite3*>(db_);
  assert(db);

  const char* sql =
      "SELECT id,name,brand,price,fuel_consumption,body_type,gearbox,drive,seats,trunk,power,max_speed,is_electric,segment "
      "FROM cars ORDER BY id DESC;";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return out;

  while (sqlite3_step(st) == SQLITE_ROW) {
    CarRow c;
    c.id = sqlite3_column_int64(st, 0);
    c.name = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
    c.brand = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));
    c.price = sqlite3_column_int(st, 3);
    c.fuel_consumption = sqlite3_column_double(st, 4);
    c.body_type = reinterpret_cast<const char*>(sqlite3_column_text(st, 5));
    c.gearbox = reinterpret_cast<const char*>(sqlite3_column_text(st, 6));
    c.drive = reinterpret_cast<const char*>(sqlite3_column_text(st, 7));
    c.seats = sqlite3_column_int(st, 8);
    c.trunk = sqlite3_column_int(st, 9);
    c.power = sqlite3_column_int(st, 10);
    c.max_speed_kmh = sqlite3_column_int(st, 11);
    c.is_electric = sqlite3_column_int(st, 12) != 0;
    c.segment = reinterpret_cast<const char*>(sqlite3_column_text(st, 13));
    out.push_back(std::move(c));
  }

  sqlite3_finalize(st);
  return out;
}

std::optional<CarRow> Db::get_car(int64_t id) {
  auto* db = reinterpret_cast<sqlite3*>(db_);
  assert(db);

  const char* sql =
      "SELECT id,name,brand,price,fuel_consumption,body_type,gearbox,drive,seats,trunk,power,max_speed,is_electric,segment "
      "FROM cars WHERE id=?;";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return std::nullopt;
  sqlite3_bind_int64(st, 1, id);
  if (sqlite3_step(st) != SQLITE_ROW) {
    sqlite3_finalize(st);
    return std::nullopt;
  }

  CarRow c;
  c.id = sqlite3_column_int64(st, 0);
  c.name = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
  c.brand = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));
  c.price = sqlite3_column_int(st, 3);
  c.fuel_consumption = sqlite3_column_double(st, 4);
  c.body_type = reinterpret_cast<const char*>(sqlite3_column_text(st, 5));
  c.gearbox = reinterpret_cast<const char*>(sqlite3_column_text(st, 6));
  c.drive = reinterpret_cast<const char*>(sqlite3_column_text(st, 7));
  c.seats = sqlite3_column_int(st, 8);
  c.trunk = sqlite3_column_int(st, 9);
  c.power = sqlite3_column_int(st, 10);
  c.max_speed_kmh = sqlite3_column_int(st, 11);
  c.is_electric = sqlite3_column_int(st, 12) != 0;
  c.segment = reinterpret_cast<const char*>(sqlite3_column_text(st, 13));
  sqlite3_finalize(st);
  return c;
}

bool Db::create_car(const CarRow& car) {
  auto* db = reinterpret_cast<sqlite3*>(db_);
  assert(db);

  const char* sql =
      "INSERT INTO cars(name,brand,price,fuel_consumption,body_type,gearbox,drive,seats,trunk,power,max_speed,is_electric,segment) "
      "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?);";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return false;

  sqlite3_bind_text(st, 1, car.name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 2, car.brand.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(st, 3, car.price);
  sqlite3_bind_double(st, 4, car.fuel_consumption);
  sqlite3_bind_text(st, 5, car.body_type.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 6, car.gearbox.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 7, car.drive.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(st, 8, car.seats);
  sqlite3_bind_int(st, 9, car.trunk);
  sqlite3_bind_int(st, 10, car.power);
  sqlite3_bind_int(st, 11, car.max_speed_kmh);
  sqlite3_bind_int(st, 12, car.is_electric ? 1 : 0);
  sqlite3_bind_text(st, 13, car.segment.c_str(), -1, SQLITE_TRANSIENT);

  bool ok = (sqlite3_step(st) == SQLITE_DONE);
  sqlite3_finalize(st);
  return ok;
}

bool Db::update_car(int64_t id, const CarRow& car) {
  auto* db = reinterpret_cast<sqlite3*>(db_);
  assert(db);

  const char* sql =
      "UPDATE cars SET name=?,brand=?,price=?,fuel_consumption=?,body_type=?,gearbox=?,drive=?,seats=?,trunk=?,power=?,"
      "max_speed=?,is_electric=?,segment=? WHERE id=?;";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return false;

  sqlite3_bind_text(st, 1, car.name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 2, car.brand.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(st, 3, car.price);
  sqlite3_bind_double(st, 4, car.fuel_consumption);
  sqlite3_bind_text(st, 5, car.body_type.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 6, car.gearbox.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 7, car.drive.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(st, 8, car.seats);
  sqlite3_bind_int(st, 9, car.trunk);
  sqlite3_bind_int(st, 10, car.power);
  sqlite3_bind_int(st, 11, car.max_speed_kmh);
  sqlite3_bind_int(st, 12, car.is_electric ? 1 : 0);
  sqlite3_bind_text(st, 13, car.segment.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(st, 14, id);

  bool ok = (sqlite3_step(st) == SQLITE_DONE);
  sqlite3_finalize(st);
  return ok;
}

bool Db::delete_car(int64_t id) {
  auto* db = reinterpret_cast<sqlite3*>(db_);
  assert(db);

  const char* sql = "DELETE FROM cars WHERE id=?;";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_int64(st, 1, id);
  bool ok = (sqlite3_step(st) == SQLITE_DONE);
  sqlite3_finalize(st);
  return ok;
}
