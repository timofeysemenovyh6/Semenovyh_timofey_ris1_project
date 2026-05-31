CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?= -lsqlite3

SRC = \
  src/main.cpp \
  src/http.cpp \
  src/db.cpp \
  src/reco.cpp \
  src/html.cpp \
  src/client_tools.cpp \
  src/session.cpp

BIN = car_picker

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) -Isrc $(SRC) -o $(BIN) $(LDFLAGS)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)

.PHONY: all run clean
