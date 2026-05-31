## Qt-клиент (qmake)

### Что это
GUI на Qt (Widgets), который общается с сервером `car_picker` по HTTP:
- `POST /api/chat`
- `POST /api/wizard`
- `GET /api/favorites` — список избранного в сессии (те же cookie, что и у сайта)
- `POST /api/favorite/toggle` — добавить/убрать id (`application/x-www-form-urlencoded`, поле `id`)
- `POST /api/favorites/clear` — очистить избранное

В ответах `/api/chat` и `/api/wizard` есть поле `favorite_ids` — чтобы подсветить ♥ в мастере после чата.

У `QNetworkAccessManager` включён `QNetworkCookieJar`, иначе каждый запрос шёл бы с новой сессией и избранное не сохранялось бы между вкладками.

### Сборка
Нужны установленный Qt (с `qmake`) и модули Widgets/Network.

```bash
cd "qt-client"
qmake car_picker_qt.pro
make
```

### Запуск
Один раз соберите сервер в **`car-picker-cpp`** (`make` — появятся `car_picker` и `cars.db`).

**Из Qt Creator:** нажмите **Run** — клиент сам запустит `car_picker`, если порт **8080** свободен и при обходе каталогов вверх от исполняемого файла находится пара **`cars.db` + `car_picker`** (это корень `car-picker-cpp` после `make`). При **закрытии** окна Qt дочерний сервер **останавливается**.

Если порт 8080 уже занят (например, вы вручную запустили `./car_picker` в терминале), второй сервер не стартует — Qt подключится к уже работающему.

Из терминала (после `qmake && make` в `qt-client`):

```bash
./car_picker_qt
```

То же правило автозапуска сервера.

