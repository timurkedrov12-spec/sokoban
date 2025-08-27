#include "../inc/field.h"

#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include <array>
#include <cctype>
#include <iostream>  // std::cout
#include <iostream>
#include <random>
#include <sstream>  // std::stringstream
#include <string>
#include <unordered_set>

#include "../inc/prog.h"

// Load field from file
Field ::Field(std::string fileName) {
    width = 0;
    height = 0;
    arr = nullptr;
    std::ifstream file(fileName);
    if (!file.is_open()) {
        std::cout << "file is not open" << std::endl;
        return;
    }
    file >> width;
    file >> height;
    std::cout << height << " " << width << std::endl;
    arr = new char[height * (width + 1) + 1];  // allocate 2D array with newlines
    std::string line;
    std::getline(file, line);  // skip line
    // dest это адрес каждой строки, мы читаем строку с текстового файла. Более подробно мы копируем содержимое по адресу строки используя фу
    // c_str() которая возвращает адрес c_style адрес строки (массив чаров) и мы копиркем это в dest. Далее добавляем сами newline в конец.
    // в самом конце добавляем null terminator чтобы мы могли обращатся через cout
    for (int i = 0; i < height; i++) {
        char* dest = &arr[i * (width + 1)];
        std::getline(file, line);  // read line from file
        std::cout << line << std::endl;
        memcpy(dest, line.c_str(), width);  // copy line
        dest[width] = '\n';                 // add newline
    }
    arr[height * (width + 1)] = 0;  // null terminator
}

void SaveFieldToFile(Field& f, const string filename) {
    int width = f.GetWidth();
    int height = f.GetHeight();
    std::ofstream out(filename);
    out << width << " " << height << endl;
    for (int y = 0; y < f.GetHeight(); ++y) {
        for (int x = 0; x < f.GetWidth(); ++x) {
            out << f.GetCell({x, y});
        }
        out << std::endl;
    }
}

Field::Field(std::stringstream& ss) {
    std::string s = ss.str();
    width = s.find('\n');
    height = std::count(s.begin(), s.end(), '\n');
    arr = new char[height * (width + 1) + 1];

    istringstream in(s);
    string line;
    for (int i = 0; i < height; i++) {
        getline(in, line);
        char* dest = &arr[i * (width + 1)];
        memcpy(dest, line.c_str(), width);
        dest[width] = '\n';
    }
    arr[height * (width + 1)] = 0;
}

// copy constructor
Field::Field(const Field& other) {
    width = other.width;
    height = other.height;
    arr = new char[height * (width + 1) + 1];
    memcpy(arr, other.arr, height * (width + 1) + 1);
    game = other.game;
}

// move constructor

Field::Field(Field&& other) noexcept
    : width(other.width),
      height(other.height),
      arr(other.arr),
      game(std::move(other.game)) {
    other.arr = nullptr;
}

Field& Field::operator=(const Field& other) {
    if (this != &other) {
        delete[] arr;
        width = other.width;
        height = other.height;
        arr = new char[height * (width + 1) + 1];
        memcpy(arr, other.arr, height * (width + 1) + 1);
        game = other.game;
    }
    return *this;
}

Field& Field::operator=(Field&& other) noexcept {
    if (this != &other) {
        delete[] arr;
        width = other.width;
        height = other.height;
        arr = other.arr;
        game = std::move(other.game);
        other.arr = nullptr;  // обнуляем источник
    }
    return *this;
}

// Destructor
Field ::~Field() {
    if (arr != nullptr) {
        delete[] arr;
    }
    arr = nullptr;
}

void ReloadMap(Field& f, const std::string& fileName) {
    Field newField(fileName);  // загружаем новую карту
    f = std::move(newField);   // перемещаем её в f
}

void GenerateNewMap(Field& f, std::stringstream& ss) {
    Field newField(ss);
    f = std::move(newField);
}

// Print field to console
void Field::Print() {
    std::cout << std::endl
              << std::endl
              << std::endl
              << arr;
}

// Check win condition: all boxes are placed correctly
bool Field::HaveWon(const GameDescriptor& g) {
    for (size_t i = 0; i < g.boxPlaces.size(); i++) {
        if (GetCell(g.boxPlaces[i].pos) != std::toupper(g.boxPlaces[i].name))
            return false;
    }
    return true;
}

// Can loader move in given direction?
bool Field::CanMove(Directions d) {
    Point lpos = GetLoaderPosition();
    Point dv = GetDirectionVector(d);
    return IsEmptySpace(lpos + dv) || (IsBox(lpos + dv) && IsEmptySpace(lpos + dv + dv));
}

bool Field::Move(Directions d, StepAnim& anim) {
    if (CanMove(d)) {
        Point lpos = GetLoaderPosition();
        Point dv = GetDirectionVector(d);

        // Позиции в пикселях для анимации
        sf::Vector2f fromPx = cellToPx(lpos.x, lpos.y);
        sf::Vector2f toPx = cellToPx(lpos.x + dv.x, lpos.y + dv.y);

        std::optional<sf::Vector2f> boxFrom, boxTo;
        std::optional<char> boxChar;  // <—

        if (IsBox(lpos + dv)) {
            Point boxPos = lpos + dv;
            Point boxDest = boxPos + dv;
            boxChar = GetCell(boxPos);
            boxFrom = cellToPx(boxPos.x, boxPos.y);
            boxTo = cellToPx(boxDest.x, boxDest.y);

            GetCell(boxDest) = GetCell(boxPos);
        }

        // перемещаем игрока в данных
        GetCell(lpos + dv) = 'x';
        GetCell(lpos) = game->emptyField.GetCell(lpos);

        // старт анимации
        anim.start(fromPx, toPx, boxFrom, boxTo, boxChar, kStepDuration);
        return true;
    }
    return false;
}

// Check if cell is empty space or box goal
bool Field::IsEmptySpace(Point p) {
    char cell = GetCell(p);
    return cell == ' ' || (cell >= 'a' && cell < 'o');
}

// Check if cell contains a box
bool Field::IsBox(Point p) {
    char cell = GetCell(p);
    return cell >= 'A' && cell < 'O';
}

// Check if cell contains a placed box
bool Field::IsBoxPlaced(Point p) {
    char cell = GetCell(p);
    return cell >= 'a' && cell < 'o';
}

// Проверка, что у блока (END или MIDDLE) нет запрещённо близко других стен
bool Field::IsInRestrictedRadius(WallBlock& block, unordered_map<Point, BlockType>& placedBlocks, Point& random_dir, WallCluster& wall) {
    // cout << "Checking block at " << block.pos.x << "," << block.pos.y
    //      << " type: " << (block.type == BlockType::END ? "END" : "MIDDLE") << endl;
    if (block.type == BlockType::END) {
        array<Point, 5> neighbors = GetNeighborPointsArray(block, random_dir);
        for (const auto& neighbor : neighbors) {
            auto it = placedBlocks.find(neighbor);
            if (it != placedBlocks.end() && IsWallType(it->second)) {
                return false;
            }
        }
    } else if (block.type == BlockType::MIDDLE) {
        // определяем перпендикуляры
        return IsMiddleBlockAcceptable(block, placedBlocks, random_dir, wall);
    }
    return true;
}

bool Field::IsMiddleBlockAcceptable(WallBlock& block, unordered_map<Point, BlockType>& placedBlocks, Point& random_dir, WallCluster& wall) {
    Point perp1, perp2;
    if (random_dir.x != 0) {
        perp1 = {0, 1};
        perp2 = {0, -1};
    } else {
        perp1 = {1, 0};
        perp2 = {-1, 0};
    }

    for (int i = 1; i <= 2; ++i) {
        Point check1 = block.pos + Point{perp1.x * i, perp1.y * i};
        Point check2 = block.pos + Point{perp2.x * i, perp2.y * i};

        // cout << "MIDDLE check: pos=" << block.pos.x << "," << block.pos.y
        //      << " -> check1=(" << check1.x << "," << check1.y << ")"
        //      << " check2=(" << check2.x << "," << check2.y << ")" << endl;

        for (const auto& check : {check1, check2}) {
            bool isSelf = std::any_of(wall.blocks.begin(), wall.blocks.end(),
                                      [&](const WallBlock& b) { return b.pos == check; });
            if (isSelf)
                continue;

            auto it = placedBlocks.find(check);
            if (it != placedBlocks.end() && IsWallType(it->second)) {
                // cout << " ❌ Conflict with block at: " << check.x << "," << check.y << endl;
                return false;
            }
        }
    }
    return true;
}

// Проверка валидности стены: нет пересечений и нет запрещённой близости
bool Field::WallCorrect(WallCluster& wall, unordered_map<Point, BlockType>& placedBlocks, Point& random_dir) {
    for (WallBlock& block : wall.blocks) {
        // Проверка на пересечение с уже размещёнными блоками

        if (placedBlocks.count(block.pos)) {
            return false;  // столкновение
        }

        // Проверка по радиусу (свободное место вокруг)
        if (!IsInRestrictedRadius(block, placedBlocks, random_dir, wall)) {
            return false;
        }
    }
    return true;  // вся стена допустима
}

// Clear field from boxes and loader
void Field::Clear() {
    char* ptr = arr;
    while (*ptr != 0) {
        if ((*ptr >= 'A' && *ptr < 'O') || *ptr == 'x') {
            *ptr = ' ';
        }
        ptr++;
    }
}

// Get reference to cell at point
char& Field::GetCell(Point p) {
    return arr[p.y * (width + 1) + p.x];
}

// Return direction vector (dx, dy) from enum
Point Field::GetDirectionVector(Directions d) {
    static Point p[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    return p[(int)d];
}

// Find loader's current position
Point Field::GetLoaderPosition() {
    char* result = strchr(arr, 'x');  // search for loader char
    if (result == NULL) {
        std::exit(1);  // error if not found
    }
    long int pos = result - arr;  // index from start
    Point p;
    p.x = pos % (width + 1);  // остаток это x
    p.y = pos / (width + 1);  // целая часть без дроби это y
    return p;
}

array<Point, 5> Field::GetNeighborPointsArray(WallBlock& block, Point& random_dir) {
    Point perp1, perp2;

    if (random_dir.x != 0) {
        perp1 = {0, 1};
        perp2 = {0, -1};
    } else {
        perp1 = {1, 0};
        perp2 = {-1, 0};
    }

    // Сначала вычисляем все точки
    Point check1 = block.pos + perp1;
    Point check2 = block.pos + perp2;
    Point checkdiag1 = {check1.x + random_dir.x, check1.y + random_dir.y};
    Point checkdiag2 = {check2.x + random_dir.x, check2.y + random_dir.y};
    Point checkfront = {block.pos.x + random_dir.x, block.pos.y + random_dir.y};

    // Заполняем std::array и возвращаем
    return {check1, check2, checkdiag1, checkdiag2, checkfront};
}

array<Point, 8> Field::GetEightDirections() {
    return {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, -1}, {-1, 1}, {1, -1}}};
}

WallBlock Field::GetRandomAnchor(int SelectedAnchor, unordered_map<Point, BlockType>& placedBlocks) {
    int current = 0;
    for (const auto& [pt, type] : placedBlocks) {
        if (type != BlockType::ANCHOR)
            continue;
        if (current == SelectedAnchor) {
            return {pt, type};
        }
        current++;
    }

    // Если якорей нет вообще — это логическая ошибка генерации
    throw std::runtime_error("GetRandomAnchor: no anchors in placedBlocks");
}

void Field::SetGame(std::shared_ptr<GameDescriptor> g) {
    game = g;
}

char Field::GetBackgroundAt(Point p) const {
    return game->emptyField.GetCell(p);
}

string Field::ToString() const {
    return string(arr);
}

void Field::LoadFromGenerator(std::stringstream& ss) {
    string s = ss.str();
    if (arr != nullptr) {
        delete[] arr;
    }
    width = s.find('\n');
    height = std::count(s.begin(), s.end(), '\n');
    arr = new char[s.size() + 1];
    strcpy(arr, s.c_str());
}

// Создание стартовых блоков — по периметру карты. Все они типа ANCHOR
unordered_map<Point, BlockType> Field::InitMap(int& mapHeight, int& mapWidth, int& anchorCounter) {
    unordered_map<Point, BlockType> placedBlocks;
    for (int y = 0; y < mapHeight; y++) {
        for (int x = 0; x < mapWidth; x++) {
            if (x == 0 || x == mapWidth - 1 || y == 0 || y == mapHeight - 1) {
                placedBlocks[{x, y}] = BlockType::ANCHOR;
                anchorCounter++;
            }
        }
    }
    return placedBlocks;
}

// Добавляет готовую стену в вектор всех размещённых блоков
void Field::AddToPlacedBlocks(WallCluster wall, unordered_map<Point, BlockType>& placedBlocks) {
    for (auto& b : wall.blocks) {
        placedBlocks[{b.pos}] = b.type;  // просто добавляем все блоки, включая END и MIDDLE
    }
}

vector<Point> Field::AddBoxes(unordered_map<Point, BlockType>& placedBlocks, int& mapHeight, int& mapWidth) {
    random_device rd;
    mt19937 rng(rd());
    uniform_int_distribution<> box_dis_y(2, mapHeight - 3);
    uniform_int_distribution<> box_dis_x(2, mapWidth - 3);
    vector<Point> boxes;
    int boxesAdded = 0;
    while (boxesAdded < 2) {
        int y = box_dis_y(rng);
        int x = box_dis_x(rng);
        Point candidate = {x, y};

        // Если точка свободна — ставим ящик
        if (placedBlocks.count(candidate) == 0) {
            placedBlocks[candidate] = BlockType::BOX;
            cout << "box #" << boxesAdded << " at (" << candidate.x << "," << candidate.y << ")" << endl;
            boxesAdded++;
            boxes.push_back(candidate);
        }
        // иначе — продолжаем цикл (выбираем новую точку)
    }
    return boxes;
}

Point Field::AddPlayer(unordered_map<Point, BlockType>& placedBlocks, int& mapHeight, int& mapWidth) {
    random_device rd;
    mt19937 rng(rd());
    uniform_int_distribution<> dis_y(1, mapHeight - 2);
    uniform_int_distribution<> dis_x(1, mapWidth - 2);
    while (true) {
        int y = dis_y(rng);
        int x = dis_x(rng);
        Point pos = {x, y};

        if (placedBlocks.count(pos) == 0) {
            placedBlocks[pos] = {BlockType::PLAYER};
            return pos;
        }
    }
}

vector<Point> Field::AddTarget(unordered_map<Point, BlockType>& placedBlocks, int& mapHeight, int& mapWidth, int& targets) {
    random_device rd;
    mt19937 rng(rd());
    uniform_int_distribution<> dis_y(1, mapHeight - 2);
    uniform_int_distribution<> dis_x(1, mapWidth - 2);
    array<Point, 8> neighbors = Field::GetEightDirections();
    vector<Point> targetsvector;
    int targetCounter = 0;

    while (targetCounter < targets) {
        int y = dis_y(rng);
        int x = dis_x(rng);
        Point pos = {x, y};

        // Не должна быть занята ни стеной, ни ящиком
        auto it = placedBlocks.find(pos);

        if (it != placedBlocks.end())
            continue;  // занято чем угодно

        // Вокруг не должно быть других целей
        bool tooClose = false;
        for (const auto& dir : neighbors) {
            Point neighborPos = {pos.x + dir.x, pos.y + dir.y};
            auto neighbor = placedBlocks.find(neighborPos);
            if (neighbor != placedBlocks.end() && neighbor->second == BlockType::TARGET) {
                tooClose = true;
                break;
            }
        }
        if (tooClose)
            continue;

        // Всё ок — добавляем цель
        placedBlocks[pos] = BlockType::TARGET;
        cout << "Target #" << targetCounter << " at (" << pos.x << "," << pos.y << ")\n";
        targetCounter++;
        targetsvector.push_back(pos);
    }
    return targetsvector;
}
