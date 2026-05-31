#include "client_tools.h"
#include "html.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <initializer_list>
#include <sstream>

namespace {

std::string md_escape(std::string s) {
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\n' || s[i] == '\r') s[i] = ' ';
  }
  return s;
}

static std::string drive_label_ru(const std::string& d) {
  if (d == "awd") return "полный (AWD)";
  if (d == "fwd") return "передний (FWD)";
  if (d == "rwd") return "задний (RWD)";
  return d;
}

static std::string gearbox_label_ru(const std::string& g) {
  if (g == "cvt") return "вариатор (CVT)";
  if (g == "at") return "автомат (AT)";
  if (g == "mt") return "механика (MT)";
  if (g == "robot") return "робот (AMT/DCT)";
  return g;
}

static std::string default_manager_notes_ru(const Preferences& prefs, const std::vector<ScoredCar>& top) {
  std::ostringstream o;
  o << "Рекомендации менеджера (автоматически, уточните при встрече):\n\n";
  if (top.empty()) {
    o << "- Сейчас жёсткие фильтры не дают кандидатов — предложите клиенту ослабить 1–2 параметра (бюджет, кузов, расход).\n";
    return o.str();
  }
  const auto& lead = top.front().car;
  o << "- Лидер подбора: **" << md_escape(car_display_title(lead)) << "** — обсудите комплектацию и историю обслуживания на осмотре.\n";
  if (prefs.max_price) o << "- Держите переговоры в коридоре до **" << *prefs.max_price << " ₽**; заложите 5–10% на торг и оформление.\n";
  if (prefs.min_seats && *prefs.min_seats >= 5)
    o << "- Для семьи проверьте реальную загрузку багажника (коляска, чемоданы) и крепления ISOFIX.\n";
  if (prefs.drive && *prefs.drive == "awd")
    o << "- Запрошен полный привод — уточните, нужен ли он круглый год или достаточно зимней резины на FWD.\n";
  if (prefs.trip_profile && *prefs.trip_profile == "city")
    o << "- Городской сценарий: акцент на расход, парктроники/камеры, удобную посадку.\n";
  o << "- Перед сделкой: диагностика, юридическая чистота, тест-драйв по маршруту клиента.\n";
  return o.str();
}

static std::string compare_table_markdown(const std::vector<ScoredCar>& top) {
  if (top.empty()) return "_Нет данных для сравнения._\n";
  const size_t n = std::min<size_t>(top.size(), 3);
  std::ostringstream md;
  md << "| Параметр |";
  for (size_t i = 0; i < n; ++i) md << " " << (i + 1) << ". " << md_escape(car_display_title(top[i].car)) << " |";
  md << "\n| --- |";
  for (size_t i = 0; i < n; ++i) md << " --- |";
  md << "\n";

  auto row_i = [&](const char* label, auto fn) {
    md << "| " << label << " |";
    for (size_t i = 0; i < n; ++i) md << " " << fn(top[i].car) << " |";
    md << "\n";
  };
  auto row_d = [&](const char* label, auto fn) {
    md << "| " << label << " |";
    for (size_t i = 0; i < n; ++i) {
      std::ostringstream v;
      v << std::fixed << std::setprecision(1) << fn(top[i].car);
      md << " " << v.str() << " |";
    }
    md << "\n";
  };

  row_i("Цена, ₽", [](const CarRow& c) { return std::to_string(c.price); });
  row_d("Расход, л/100", [](const CarRow& c) { return c.fuel_consumption; });
  row_i("Кузов", [](const CarRow& c) { return c.body_type; });
  row_i("КПП", [](const CarRow& c) { return gearbox_label_ru(c.gearbox); });
  row_i("Привод", [](const CarRow& c) { return drive_label_ru(c.drive); });
  row_i("Мест", [](const CarRow& c) { return std::to_string(c.seats); });
  row_i("Багажник, л", [](const CarRow& c) { return std::to_string(c.trunk); });
  row_i("Мощность, л.с.", [](const CarRow& c) { return std::to_string(c.power); });
  row_i("TCO/год, ₽", [](const CarRow& c) { return std::to_string(estimated_annual_tco_rub(c)); });
  row_i("«5 лет», ₽/мес", [](const CarRow& c) {
    return std::to_string(estimate_life_with_car_5y(c, 15000).monthly_equiv_rub);
  });
  row_i("Снежный индекс", [](const CarRow& c) {
    const auto S = snow_index_for_car(c, Preferences{});
    return std::to_string(S.score_0_to_10) + "/10";
  });
  return md.str();
}

} // namespace

std::vector<EducationTopic> education_topics() {
  return {
      {"gearbox_overview",
       "Коробки передач — обзор",
       "AT, MT, CVT, робот: чем отличаются и кому что подходит.",
       "<p><strong>Коробка передач (КПП)</strong> передаёт крутящий момент от двигателя на колёса. "
       "В нашем каталоге встречаются обозначения <code>at</code>, <code>mt</code>, <code>cvt</code>, <code>robot</code> — ниже по каждому отдельная карточка.</p>"
       "<p><strong>Кратко:</strong></p><ul>"
       "<li><strong>AT (автомат)</strong> — классические переключения «ступеньками», привычно и универсально.</li>"
       "<li><strong>MT (механика)</strong> — сцепление и рычаг; дешевле в обслуживании, нужен навык.</li>"
       "<li><strong>CVT (вариатор)</strong> — без фиксированных передач, плавно и экономично в городе.</li>"
       "<li><strong>Robot (робот)</strong> — механика с электронным управлением: AMT (медленнее) или DCT (быстрее, «робот с двумя сцеплениями»).</li>"
       "</ul>"
       "<p class=\"muted\">Для города чаще выбирают AT или CVT; для экономии — MT; роботы сильно зависят от поколения — на тест-драйве оцените плавность.</p>"},
      {"gearbox_at",
       "Автомат (AT)",
       "Классическая «автоматическая коробка» — переключения передач без педали сцепления.",
       "<p><strong>Автомат (AT)</strong> использует гидротрансформатор или современные варианты с пакетами фрикционов. "
       "Вы нажимаете газ и тормоз — передачи переключает электроника.</p>"
       "<p><strong>Плюсы:</strong> удобно в пробках, предсказуемое поведение, много сервисов умеют обслуживать.</p>"
       "<p><strong>Минусы:</strong> обычно дороже в покупке и чуть выше расход, чем у механики; старые авто — проверьте, менялось ли масло в АКПП.</p>"
       "<p><strong>Кому подходит:</strong> город, семья, тем, кто не хочет учиться на «механике».</p>"
       "<p class=\"muted\">В каталоге: <code>at</code>.</p>"},
      {"gearbox_mt",
       "Механика (MT)",
       "Ручная коробка: сцепление + рычаг, полный контроль над передачами.",
       "<p><strong>Механика (MT)</strong> — вы сами выбираете передачу. Сцепление выжимаете левой ногой (или педалью), "
       "переключение — рычагом.</p>"
       "<p><strong>Плюсы:</strong> проще и дешевле ремонт, часто ниже расход, надёжность при аккуратной езде.</p>"
       "<p><strong>Минусы:</strong> усталость в плотном городе; на подъёме и в пробке нужен опыт; при покупке б/у — износ сцепления.</p>"
       "<p><strong>Кому подходит:</strong> опытные водители, бюджетный сегмент, редкие пробки.</p>"
       "<p class=\"muted\">В каталоге: <code>mt</code>.</p>"},
      {"gearbox_cvt",
       "Вариатор (CVT)",
       "Плавный разгон без «ступеней»; особенности при обгонах.",
       "<p><strong>Вариатор (CVT)</strong> — нет фиксированных «первой, второй…»: передаточное число меняется плавно. "
       "Мотор часто держит обороты в экономичной зоне.</p>"
       "<p><strong>Плюсы:</strong> мягкий ход, низкий расход в спокойной езде, удобно в городе.</p>"
       "<p><strong>Минусы:</strong> при резком газе возможен «рев»; не всем нравится ощущение на трассе; масло CVT меняют строго по регламенту.</p>"
       "<p class=\"muted\">В каталоге: <code>cvt</code>. Не путать с обычным автоматом!</p>"},
      {"gearbox_robot",
       "Робот (AMT / DCT)",
       "«Механика на автомате» — от бюджетных AMT до быстрых двухсцепочных DCT.",
       "<p><strong>Роботизированная КПП (robot)</strong> — обычная механическая коробка, но переключением управляет актуатор. "
       "Два подтипа:</p><ul>"
       "<li><strong>AMT</strong> — один сцепитель, переключения заметнее (типичны для недорогих моделей).</li>"
       "<li><strong>DCT / DSG</strong> — два сцепления, переключения быстрее, ближе к «спортивному» автомату.</li></ul>"
       "<p><strong>Плюсы:</strong> часто дешевле классического AT при заводской установке, расход ближе к механике.</p>"
       "<p><strong>Минусы:</strong> качество сильно зависит от поколения; на тест-драйве оцените рывки с места и в пробке.</p>"
       "<p class=\"muted\">В каталоге: <code>robot</code>.</p>"},
      {"awd_fwd",
       "AWD и FWD — в чём разница",
       "Полный привод увереннее на скользком; передний проще и дешевле в содержании.",
       "<p><strong>FWD (передний привод)</strong> — тяга на передние колёса. Меньше деталей, обычно ниже масса и расход. "
       "На зиме критичны качественные шины и спокойный стиль.</p>"
       "<p><strong>AWD (полный привод)</strong> — крутящий момент распределяется на обе оси (постоянно или подключая заднюю). "
       "Лучше трогание на снегу/грунте, но выше расход и сложнее обслуживание (муфта, кардан).</p>"
       "<p><strong>Когда что выбирать:</strong> город + редкий снег — часто хватает FWD с зимней резиной; "
       "дача, уклоны, частые зимние поездки — AWD или кроссовер с высоким клиренсом.</p>"},
      {"clearance",
       "Зачем нужен клиренс",
       "Дорожный просвет — запас до «царапания» бампером и днищем.",
       "<p><strong>Клиренс (дорожный просвет)</strong> — расстояние от земли до нижних элементов кузова. "
       "Чем он больше, тем спокойнее заезды на бордюр, снег, грунтовку.</p>"
       "<p><strong>Ориентиры:</strong> седан ~140–160&nbsp;мм; кроссовер ~190–220&nbsp;мм; внедорожник выше.</p>"
       "<p>В подборе «снежный индекс» использует прокси по типу кузова и приводу — это не замена замера реального клиренса на конкретной машине.</p>"
       "<p><strong>На что смотреть при осмотре:</strong> не только цифра в паспорте, но и длине свесов, юбках и защите картера.</p>"},
      {"body_types",
       "Типы кузова",
       "Седан, хэтчбек, кроссовер, универсал — что удобнее под ваши задачи.",
       "<p><strong>Седан</strong> — отдельный багажник, тихо на трассе, меньше высота (клиренс обычно ниже).</p>"
       "<p><strong>Хэтчбек</strong> — короткий кузов, багажник с откидной дверью; удобен в городе, места для вещей гибко.</p>"
       "<p><strong>Универсал</strong> — длинный багажник при тех же местах, что у седана; хорош для семьи и поездок.</p>"
       "<p><strong>Кроссовер / SUV</strong> — выше посадка и клиренс, удобнее садиться; расход чуть выше, на рынке очень популярен.</p>"
       "<p><strong>Минивэн</strong> — максимум мест и дверей; для большой семьи и редких «пятёрных» поездок.</p>"
       "<p class=\"muted\">В подборе: <code>sedan</code>, <code>hatchback</code>, <code>wagon</code>, <code>crossover</code>, <code>suv</code>, <code>van</code> и др.</p>"},
      {"fuel_ev",
       "Расход и электромобили",
       "л/100 км, «только электро» и «без электро» в фильтрах.",
       "<p><strong>Расход (л/100 км)</strong> — сколько литров топлива в среднем на 100 км. "
       "Чем меньше число, тем дешевле езда. Реальный расход зависит от зимы, стиля и пробок (+15–25% к паспорту — норма).</p>"
       "<p><strong>ДВС</strong> — бензин или дизель с привычной заправкой.</p>"
       "<p><strong>Электромобиль (EV)</strong> — расход в каталоге часто «0» по топливу; считайте стоимость зарядки и запас хода. "
       "Для города удобны, для дальних поездок — планируйте зарядки.</p>"
       "<p>В чате можно написать «только электро» или «без электро», «расход до 8».</p>"},
      {"power_hp",
       "Мощность (л.с.)",
       "Зачем смотреть на лошадиные силы и как не переплатить.",
       "<p><strong>Лошадиные силы (л.с., hp)</strong> — запас ускорения и уверенности на обгоне, а не «максимальная скорость».</p>"
       "<p><strong>Ориентиры для город + трасса:</strong></p><ul>"
       "<li>~90–120 л.с. — спокойная езда, один-два человека.</li>"
       "<li>~120–180 л.с. — комфортный семейный запас, обгоны без стресса.</li>"
       "<li>200+ л.с. — динамика и цена выше; переплата, если не нужна.</li></ul>"
       "<p>В подборе можно задать «от … л.с.» и «до … л.с.» — чтобы не переплачивать за лишний мотор.</p>"},
      {"budget_tco",
       "Бюджет и стоимость владения",
       "Цена покупки — только часть расходов.",
       "<p><strong>Цена в объявлении</strong> — разовый платёж. Заложите 5–10% на торг, оформление и мелкий ремонт после покупки.</p>"
       "<p><strong>TCO/год</strong> в таблице — грубая оценка: топливо или электроэнергия, налог, аналог страховки. "
       "Не включает кредит, КАСКО и крупные поломки.</p>"
       "<p><strong>«5 лет»</strong> в разделе «Лаборатория» и в отчёте — амортизация + эксплуатация; удобно сравнивать два похожих авто.</p>"
       "<p class=\"muted\">Совет новичку: сначала жёсткий потолок бюджета, потом кузов и расход — так проще не «уплыть» в класс дороже.</p>"},
      {"seats_trunk",
       "Места и багажник",
       "Сколько мест реально нужно и что значают литры багажника.",
       "<p><strong>Места</strong> — обычно 4–5 в легковых, 6–7 в минивэнах. «5 человек» в паспорте ≠ комфорт на дальняк: считайте с детскими креслами.</p>"
       "<p><strong>Багажник (л)</strong> — объём задней зоны. Ориентиры:</p><ul>"
       "<li>300–380 л — город, сумки и закупки.</li>"
       "<li>450–550 л — семья, коляска или чемоданы на отпуск.</li>"
       "<li>600+ л — универсал, минивэн или SUV с большим проёмом.</li></ul>"
       "<p>Форма важнее цифры: узкий проём или высокий пол багажника мешают больше, чем «минус 30 л».</p>"},
      {"winter_tires",
       "Зима: шины важнее «полного привода»",
       "Миф про AWD и простое правило для новичка.",
       "<p><strong>Зимняя резина</strong> на всех четырёх колёсах часто важнее, чем полный привод на летней резине. "
       "FWD + хорошие шины — нормальный вариант для города.</p>"
       "<p><strong>AWD</strong> помогает трогаться со снега и на укатанном снегу, но не отменяет тормозной путь.</p>"
       "<p>В подборе «снежный индекс» — игровая оценка по кузову и приводу; реальная зима = шины + стиль езды.</p>"},
      {"buy_basics",
       "С чего начать выбор авто",
       "Пять шагов для тех, кто не разбирается в технике.",
       "<ol>"
       "<li><strong>Задача:</strong> город / семья / дача / трасса — одной фразой в чате.</li>"
       "<li><strong>Бюджет «до …»</strong> в рублях или «до N млн».</li>"
       "<li><strong>Места и багажник</strong> цифрами, если важно.</li>"
       "<li><strong>Коробка и привод</strong> — по справочнику выше; для города часто AT/CVT и FWD.</li>"
       "<li><strong>Топ‑3 из подборщика</strong> — смотрите «Разбор топ‑3»: TCO и объяснимые рекомендации, потом Авито и осмотр.</li>"
       "</ol>"
       "<p class=\"muted\">Не гонитесь за максимальной мощностью и «полным приводом», пока не поняли бюджет и расход.</p>"},
  };
}

const EducationTopic* education_topic_by_id(const std::string& id) {
  for (const auto& t : education_topics()) {
    if (t.id == id) return &t;
  }
  return nullptr;
}

static bool edu_msg_has(const std::string& t, std::initializer_list<const char*> keys) {
  for (const char* k : keys) {
    if (t.find(k) != std::string::npos) return true;
  }
  return false;
}

std::string education_hint_for_message_ru(const std::string& user_message, bool education_mode) {
  if (!education_mode) return {};
  std::string t = user_message;
  for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

  if (edu_msg_has(t, {"вариатор", "cvt"}))
    return "\n\n📘 Справка: вариатор (CVT) — в «Справочник» → «Вариатор (CVT)»; общий обзор КПП — «Коробки передач — обзор».";
  if (edu_msg_has(t, {"автомат", "акпп", "автоматическ"}))
    return "\n\n📘 Справка: автомат (AT) — в «Справочник» → «Автомат (AT)».";
  if (edu_msg_has(t, {"механик", "мкпп", "сцеплен", "ручная короб"}))
    return "\n\n📘 Справка: механика (MT) — в «Справочник» → «Механика (MT)».";
  if (edu_msg_has(t, {"робот", "amt", "dct", "dsg", "robot"}))
    return "\n\n📘 Справка: роботизированная КПП — в «Справочник» → «Робот (AMT / DCT)».";
  if (edu_msg_has(t, {"коробк", "кпп", "передач"}))
    return "\n\n📘 Справка: все типы КПП — откройте «Справочник» → «Коробки передач — обзор».";

  if (edu_msg_has(t, {"awd", "полный привод", "4wd", "4x4"}))
    return "\n\n📘 Справка: полный привод — «Справочник» → «AWD и FWD»; про зиму также «Зима: шины».";
  if (edu_msg_has(t, {"fwd", "передний привод", "привод"}))
    return "\n\n📘 Справка: привод — «Справочник» → «AWD и FWD».";

  if (edu_msg_has(t, {"клиренс", "просвет", "бордюр", "колдобин"}))
    return "\n\n📘 Справка: клиренс — «Справочник» → «Зачем нужен клиренс».";

  if (edu_msg_has(t, {"кроссовер", "внедорож", "suv", "седан", "хэтчбек", "хетчбек", "универсал", "минивэн", "кузов"}))
    return "\n\n📘 Справка: типы кузова — «Справочник» → «Типы кузова».";

  if (edu_msg_has(t, {"расход", "л/100", "литр", "бензин", "дизел", "электро", "ev", "гибрид"}))
    return "\n\n📘 Справка: расход и электро — «Справочник» → «Расход и электромобили».";

  if (edu_msg_has(t, {"л.с", "лс", "лошад", "мощност", "л.с."}))
    return "\n\n📘 Справка: мощность — «Справочник» → «Мощность (л.с.)».";

  if (edu_msg_has(t, {"бюджет", "млн", "дорого", "дешев", "цена", "стоимость влад", "tco"}))
    return "\n\n📘 Справка: бюджет и TCO — «Справочник» → «Бюджет и стоимость владения».";

  if (edu_msg_has(t, {"багажник", "литр", "мест", "семь", "семья", "коляск"}))
    return "\n\n📘 Справка: места и багажник — «Справочник» → «Места и багажник».";

  if (edu_msg_has(t, {"зим", "снег", "шип", "резин", "лед"}))
    return "\n\n📘 Справка: зимняя эксплуатация — «Справочник» → «Зима: шины важнее полного привода».";

  if (edu_msg_has(t, {"не разбира", "новичок", "первый авто", "с чего нач", "как выбра"}))
    return "\n\n📘 Справка: пошаговый старт — «Справочник» → «С чего начать выбор авто».";

  return {};
}

std::string prefs_criteria_markdown(const Preferences& prefs) {
  std::ostringstream md;
  auto line_i = [&](const char* l, const std::optional<int>& v) {
    if (v) md << "- **" << l << ":** " << *v << "\n";
  };
  auto line_d = [&](const char* l, const std::optional<double>& v) {
    if (v) md << "- **" << l << ":** " << *v << "\n";
  };
  auto line_s = [&](const char* l, const std::optional<std::string>& v) {
    if (v && !v->empty()) md << "- **" << l << ":** " << md_escape(*v) << "\n";
  };
  line_i("Мин. цена, ₽", prefs.min_price);
  line_i("Макс. бюджет, ₽", prefs.max_price);
  line_d("Макс. расход, л/100 км", prefs.max_fuel);
  line_i("Мин. мест", prefs.min_seats);
  line_i("Мин. багажник, л", prefs.min_trunk);
  line_i("Мин. мощность, л.с.", prefs.min_power);
  line_i("Макс. мощность, л.с.", prefs.max_power);
  line_s("Кузов", prefs.body_type);
  line_s("КПП", prefs.gearbox);
  line_s("Привод", prefs.drive);
  line_s("Сегмент", prefs.segment);
  line_s("Марка (подстрока)", prefs.brand_substr);
  line_s("Исключить марку", prefs.brand_exclude_substr);
  line_s("Сценарий поездок", prefs.trip_profile);
  if (prefs.only_ev.has_value()) md << "- **Энергия:** " << (*prefs.only_ev ? "только электро" : "без электро") << "\n";
  if (md.str().empty()) md << "- _Критерии не заданы — уточните бюджет и задачи._\n";
  return md.str();
}

std::string build_client_report_markdown(const ClientProfile& client, const Preferences& prefs,
                                         const std::vector<ScoredCar>& top_recs) {
  std::vector<ScoredCar> top3;
  for (size_t i = 0; i < top_recs.size() && i < 3; ++i) top3.push_back(top_recs[i]);

  std::ostringstream md;
  md << "# Отчёт для клиента — AutoSelect\n\n";
  md << "_Сформировано локально по текущей сессии подбора. Оценки ориентировочные._\n\n";

  md << "## Данные клиента\n\n";
  if (!client.name.empty()) md << "- **Имя:** " << md_escape(client.name) << "\n";
  if (!client.phone.empty()) md << "- **Телефон:** " << md_escape(client.phone) << "\n";
  if (!client.email.empty()) md << "- **E-mail:** " << md_escape(client.email) << "\n";
  if (!client.notes.empty()) md << "- **Заметки:** " << md_escape(client.notes) << "\n";
  if (client.name.empty() && client.phone.empty() && client.email.empty() && client.notes.empty())
    md << "- _Поля клиента не заполнены — можно добавить в блоке «Справочник и отчёт»._\n";

  md << "\n## Критерии подбора\n\n" << prefs_criteria_markdown(prefs);

  md << "\n## Топ-3 автомобиля\n\n";
  if (top3.empty()) {
    md << "_Нет вариантов по жёстким фильтрам — ослабьте условия и повторите подбор._\n";
  } else {
    int idx = 1;
    for (const auto& sc : top3) {
      const auto& c = sc.car;
      md << "### " << idx++ << ". " << md_escape(car_display_title(c)) << "\n\n";
      md << "- **Марка:** " << md_escape(c.brand) << "\n";
      md << "- **Цена:** " << c.price << " ₽\n";
      md << "- **Соответствие (мягкий балл):** " << (int)std::round(sc.score * 100.0) << "%\n";
      md << "- **Почему в топе:** " << md_escape(sc.explain_ru.empty() ? explain_match_ru(c, prefs) : sc.explain_ru) << "\n";
      md << "- **Кузов / КПП / привод:** " << c.body_type << " / " << gearbox_label_ru(c.gearbox) << " / "
         << drive_label_ru(c.drive) << "\n";
      md << "- **Мест / багажник / мощность:** " << c.seats << " / " << c.trunk << " л / " << c.power << " л.с.\n";
      const std::string av = avito_search_url(c, prefs);
      md << "- [Поиск на Авито](" << av << ")\n\n";
    }
  }

  md << "## Сравнение топ-3\n\n" << compare_table_markdown(top3);

  md << "\n## Стоимость владения (модель)\n\n";
  if (top3.empty()) {
    md << "_Нет машин для расчёта._\n";
  } else {
    md << "Ориентир: 15 000 км/год. TCO — топливо/энергия, налог, страховка (грубо). «5 лет» — амортизация + эксплуатация.\n\n";
    md << "| # | TCO в год, ₽ | «5 лет», ₽/мес | Комментарий |\n| --- | --- | --- | --- |\n";
    int i = 1;
    for (const auto& sc : top3) {
      const auto& c = sc.car;
      const int tco = estimated_annual_tco_rub(c);
      const auto L = estimate_life_with_car_5y(c, 15000);
      md << "| " << i++ << " | " << tco << " | " << L.monthly_equiv_rub << " | " << md_escape(L.comment_ru) << " |\n";
    }
  }

  md << "\n## Рекомендации менеджера\n\n";
  const std::string notes =
      client.manager_notes.empty() ? default_manager_notes_ru(prefs, top3) : client.manager_notes;
  md << notes << "\n";

  return md.str();
}

static std::string insight_ul_ru(const char* title, const char* cls, const std::vector<std::string>& items) {
  if (items.empty()) return {};
  std::ostringstream o;
  o << "<div class=\"insightList insightList--" << cls << "\"><div class=\"insightList__title\">" << title << "</div>";
  o << "<ul>";
  for (const auto& s : items) o << "<li>" << html::escape(s) << "</li>";
  o << "</ul></div>";
  return o.str();
}

std::string top3_decision_insights_html(const Preferences& prefs, const std::vector<ScoredCar>& recs) {
  if (recs.empty()) return {};
  const size_t n = std::min<size_t>(recs.size(), 3);
  int cheapest_tco = estimated_annual_tco_rub(recs[0].car);
  int cheapest_life = estimate_life_with_car_5y(recs[0].car, 15000).monthly_equiv_rub;
  for (size_t i = 1; i < n; ++i) {
    const int t = estimated_annual_tco_rub(recs[i].car);
    if (t < cheapest_tco) cheapest_tco = t;
    const int m = estimate_life_with_car_5y(recs[i].car, 15000).monthly_equiv_rub;
    if (m < cheapest_life) cheapest_life = m;
  }

  std::ostringstream b;
  b << "<section class=\"card insightPanel\" id=\"top3-insights\">";
  b << "<details class=\"toolPanel__details\" open>";
  b << "<summary class=\"toolPanel__summary\"><span class=\"toolPanel__icon\" aria-hidden=\"true\">📊</span> ";
  b << "<span><strong>Разбор топ‑" << n << "</strong>"
    << "<span class=\"muted toolPanel__hint\"> — стоимость владения и объяснимые рекомендации по выбранным кандидатам</span></span></summary>";
  b << "<div class=\"toolPanel__body\"><p class=\"muted insightLead\">Появляется после подбора. Оценки ориентировочные, для сравнения вариантов между собой — "
    << "не меняют ранжирование в таблице.</p><div class=\"insightGrid\">";

  for (size_t i = 0; i < n; ++i) {
    const auto& sc = recs[i];
    const auto& c = sc.car;
    const auto ex = explainable_match(c, prefs);
    const auto tco = tco_breakdown(c, 15000);
    const auto life = estimate_life_with_car_5y(c, 15000);
    const int score_pct = static_cast<int>(std::round(sc.score * 100.0));
    const bool tco_best = estimated_annual_tco_rub(c) == cheapest_tco;
    const bool life_best = life.monthly_equiv_rub == cheapest_life;

    b << "<article class=\"insightCard\">";
    b << "<div class=\"insightCard__head\"><span class=\"insightRank\">#" << (i + 1) << "</span>";
    b << "<h3 class=\"insightCard__title\">" << html::escape(car_display_title(c)) << "</h3>";
    b << "<span class=\"insightScore\" title=\"Мягкий балл соответствия критериям\">" << score_pct << "%</span></div>";

    b << "<div class=\"insightCols\"><div class=\"insightCol\">";
    b << "<h4 class=\"insightCol__hd\">Объяснимая рекомендация</h4>";
    if (ex.strengths.empty() && ex.preferences.empty() && ex.cautions.empty()) {
      b << "<p class=\"muted\">Проходит жёсткие фильтры; численное соответствие — " << score_pct << "%.</p>";
    } else {
      b << insight_ul_ru("Сильные стороны по вашим критериям", "ok", ex.strengths);
      b << insight_ul_ru("Учтённые пожелания", "soft", ex.preferences);
      b << insight_ul_ru("На что обратить внимание", "warn", ex.cautions);
    }
    b << "</div><div class=\"insightCol\">";
    b << "<h4 class=\"insightCol__hd\">Анализ стоимости владения</h4>";
    b << "<p class=\"insightKpi\">TCO ≈ <strong>" << tco.total_year_rub << " ₽/год</strong>";
    if (tco_best && n > 1) b << " <span class=\"insightTag\">ниже среди топ‑" << n << "</span>";
    b << "</p><ul class=\"insightTcoList\">";
  b << "<li>" << (c.is_electric ? "Энергия (электро)" : "Топливо") << ": ~" << tco.fuel_energy_year_rub
      << " ₽/год при " << tco.km_per_year << " км</li>";
    b << "<li>Налог + грубый аналог ОСАГО: ~" << tco.tax_insurance_year_rub << " ₽/год</li>";
    b << "<li><strong>«5 лет» усреднённо:</strong> ≈ " << life.monthly_equiv_rub << " ₽/мес";
    if (life_best && n > 1) b << " <span class=\"insightTag\">лучше в топе</span>";
    b << "</li>";
    b << "<li>Амортизация за 5 лет (модель): ~" << life.depreciation_5y_rub << " ₽</li>";
    b << "<li>ТО + резина: ~" << (life.service_year_rub + life.tires_year_rub) << " ₽/год</li>";
    b << "</ul><p class=\"muted insightFoot\">" << html::escape(life.comment_ru) << "</p>";
    b << "</div></div></article>";
  }

  b << "</div></div></details></section>";
  return b.str();
}

std::string client_tools_panel_html(const ClientProfile& client, const std::string& save_action) {
  std::ostringstream b;
  b << "<section class=\"card toolPanel\" id=\"client-tools\">";
  b << "<details class=\"toolPanel__details\">";
  b << "<summary class=\"toolPanel__summary\"><span class=\"toolPanel__icon\" aria-hidden=\"true\">📋</span> "
    << "<span><strong>Справочник и отчёт для клиента</strong>"
    << "<span class=\"muted toolPanel__hint\"> — не меняет подбор; можно свернуть</span></span></summary>";
  b << "<div class=\"toolPanel__body\">";

  b << "<div class=\"toolGrid\">";
  b << "<div class=\"toolCol\">";
  b << "<h3 class=\"toolCol__title\">Режим обучения</h3>";
  b << "<p class=\"muted toolCol__sub\">Справочник для клиента без опыта: все типы КПП, кузова, расход, мощность, бюджет, зима. "
    << "В чате при включённой галочке — короткие подсказки по ключевым словам.</p>";
  b << "<div class=\"eduCards\">";
  for (const auto& t : education_topics()) {
    b << "<details class=\"eduCard\"><summary>" << html::escape(t.title) << "</summary>";
    b << "<div class=\"eduCard__body\">" << t.body_html << "</div></details>";
  }
  b << "</div></div>";

  b << "<div class=\"toolCol\">";
  b << "<h3 class=\"toolCol__title\">Данные для отчёта</h3>";
  b << "<form class=\"toolForm\" method=\"POST\" action=\"" << html::escape(save_action) << "\">";
  b << "<div class=\"grid2\">";
  b << "<label class=\"fieldFloat\"><span class=\"fieldFloat__lbl\">Имя клиента</span>"
    << "<input class=\"fieldFloat__input\" name=\"client_name\" value=\"" << html::escape(client.name)
    << "\" placeholder=\"Иван И.\"/></label>";
  b << "<label class=\"fieldFloat\"><span class=\"fieldFloat__lbl\">Телефон</span>"
    << "<input class=\"fieldFloat__input\" name=\"client_phone\" value=\"" << html::escape(client.phone)
    << "\" placeholder=\"+7 …\"/></label>";
  b << "<label class=\"fieldFloat\"><span class=\"fieldFloat__lbl\">E-mail</span>"
    << "<input class=\"fieldFloat__input\" name=\"client_email\" value=\"" << html::escape(client.email)
    << "\" placeholder=\"email\"/></label>";
  b << "<label class=\"fieldCheck toolCheck\"><input type=\"checkbox\" name=\"education_mode\" value=\"1\""
    << (client.education_mode ? " checked" : "") << "/> Режим обучения в чате</label>";
  b << "</div>";
  b << "<label class=\"fieldFloat\"><span class=\"fieldFloat__lbl\">Заметки о клиенте</span>"
    << "<textarea class=\"fieldFloat__input toolArea\" name=\"client_notes\" rows=\"2\" placeholder=\"Семья, срок покупки…\">"
    << html::escape(client.notes) << "</textarea></label>";
  b << "<label class=\"fieldFloat\"><span class=\"fieldFloat__lbl\">Рекомендации менеджера (в отчёт)</span>"
    << "<textarea class=\"fieldFloat__input toolArea\" name=\"manager_notes\" rows=\"4\" placeholder=\"Пусто — подставим шаблон по топу\">"
    << html::escape(client.manager_notes) << "</textarea></label>";
  b << "<div class=\"actions\">"
    << "<button class=\"btn btn--solid btn--sm\" type=\"submit\">Сохранить</button> "
    << "<a class=\"btn btn--outline btn--sm\" href=\"/export/client-report.md\" download=\"client-report.md\">Скачать отчёт (.md)</a> "
    << "<a class=\"btn btn--ghost btn--sm\" href=\"/client-tools\" target=\"_blank\" rel=\"noopener\">Открыть на странице</a>"
    << "</div></form></div></div>";

  b << "</div></details></section>";
  return b.str();
}

std::string client_tools_page(const ClientProfile& client, const Preferences& prefs, const std::vector<ScoredCar>& top3) {
  std::ostringstream b;
  b << "<h1>Справочник и отчёт</h1>";
  b << "<p class=\"muted\">Для клиента, который плохо ориентируется в технике, и для передачи результата подбора. "
    << "Критерии в мастере и чате <strong>не меняются</strong>.</p>";
  b << client_tools_panel_html(client, "/client-tools/save");
  b << "<section class=\"card\"><h2>Превью отчёта</h2>";
  b << "<pre class=\"reportPreview\">" << html::escape(build_client_report_markdown(client, prefs, top3)) << "</pre>";
  b << "<p class=\"actions\"><a class=\"btn btn--solid\" href=\"/export/client-report.md\" download=\"client-report.md\">Скачать .md</a> "
    << "<a class=\"btn btn--outline\" href=\"/wizard\">Мастер</a> <a class=\"btn btn--outline\" href=\"/chat\">Чат</a></p></section>";
  return html::layout("Справочник и отчёт", b.str(), "tools");
}

void apply_client_profile_form(ClientProfile& client, const std::map<std::string, std::string>& form) {
  auto get = [&](const char* k) -> std::string {
    auto it = form.find(k);
    return it == form.end() ? "" : it->second;
  };
  client.name = get("client_name");
  client.phone = get("client_phone");
  client.email = get("client_email");
  client.notes = get("client_notes");
  client.manager_notes = get("manager_notes");
  client.education_mode = form.find("education_mode") != form.end();
}