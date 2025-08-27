# Компилятор и флаги
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -g

# Пути к SFML
SFML_INC = /opt/homebrew/opt/sfml/include
SFML_LIB = /opt/homebrew/opt/sfml/lib

# Пути к своим заголовкам
INCLUDE_DIRS = -Isrc -Iinclude -I$(SFML_INC)

# Файлы
SRC = src/main.cpp src/field.cpp src/prog.cpp
OBJ = $(SRC:src/%.cpp=obj/%.o)
DEP = $(OBJ:.o=.d)

# Цель
TARGET = bin/program

# Правила
.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -L$(SFML_LIB) -o $@ $^ -lsfml-graphics -lsfml-window -lsfml-system

obj/%.o: src/%.cpp
	@mkdir -p obj
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -MMD -MP -c $< -o $@

-include $(DEP)

clean:
	rm -rf obj bin