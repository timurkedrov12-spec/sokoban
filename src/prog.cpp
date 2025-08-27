// field
//  2d char array
#include "../inc/prog.h"

#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include <array>
#include <cctype>
#include <chrono>
#include <iostream>  // std::cout
#include <iostream>
#include <random>
#include <sstream>  // std::stringstream
#include <stdexcept>
#include <string>
#include <unordered_set>

#include "../inc/field.h"

bool IsWallType(BlockType t) {
    return t == BlockType::MIDDLE || t == BlockType::END || t == BlockType::ANCHOR;
}

bool CanMoveMap(Point direction, StateForGenerator& state) {
    Point next = state.reverseMap.player + direction;

    if (IsEmptySpaceMap(state.map, next)) {
        return true;
    }

    auto it = state.map.find(next);
    if (it != state.map.end() && it->second == BlockType::BOX) {
        Point nextnext = next + direction;
        if (IsEmptySpaceMap(state.map, nextnext)) {
            return true;
        }
    }

    return false;
}

bool IsEmptySpaceMap(unordered_map<Point, BlockType>& PlacedBlocks, const Point& pos) {
    auto it = PlacedBlocks.find(pos);
    return it == PlacedBlocks.end() || it->second == BlockType::TARGET;
}

bool IsBoxMap(unordered_map<Point, BlockType>& PlacedBlocks, Point& pos) {
    return PlacedBlocks[pos] == BlockType::BOX;
}

Directions PointToDirection(Point p) {
    if (p.x == -1 && p.y == 0)
        return Directions::LEFT;
    if (p.x == 1 && p.y == 0)
        return Directions::RIGHT;
    if (p.x == 0 && p.y == -1)
        return Directions::UP;
    if (p.x == 0 && p.y == 1)
        return Directions::DOWN;

    std::cerr << "PointToDirection: Invalid direction: (" << p.x << ", " << p.y << ")\n";
    throw std::runtime_error("Invalid direction point");
}

static inline bool isBlocked(const unordered_map<Point, BlockType>& m, Point p) {
    auto it = m.find(p);
    return it != m.end() && (IsWallType(it->second) || it->second == BlockType::BOX);
}

// Возвращаем true, если ход применён (игрок сдвинулся / толкнул ящик)
bool MoveMap(Point dir, StateForGenerator& s) {
    Point cur = s.reverseMap.player;
    Point next = cur + dir;

    auto it = s.map.find(next);

    // Пусто (в т.ч. цель — целей в map нет) → просто идём
    if (it == s.map.end()) {
        s.reverseMap.player = next;
        return true;
    }

    // Стена — стоп
    if (IsWallType(it->second))
        return false;

    // Ящик — пробуем толкнуть
    if (it->second == BlockType::BOX) {
        Point nn = next + dir;
        if (isBlocked(s.map, nn))
            return false;  // за ящиком стена/ящик → нельзя
        // сдвигаем ящик next → nn
        s.map.erase(next);
        s.map[nn] = BlockType::BOX;

        // обновляем список ящиков в reverseMap
        for (auto& b : s.reverseMap.boxes)
            if (b == next) {
                b = nn;
                break;
            }

        // игрок становится на место ящика
        s.reverseMap.player = next;
        return true;
    }

    return false;  // на всякий случай
}

ReverseMap BuildReverseMap(const std::unordered_map<Point, BlockType>& map) {
    ReverseMap rev;
    for (const auto& [pt, type] : map) {
        switch (type) {
            case BlockType::PLAYER:
                rev.player = pt;
                break;
            case BlockType::TARGET:
                rev.targets.push_back(pt);
                break;
            case BlockType::BOX:
                rev.boxes.push_back(pt);
                break;
            default:
                break;
        }
    }
    return rev;
}

// Constructor: store initial box places
GameDescriptor::GameDescriptor(Field& field) : emptyField(field) {
    emptyField.Clear();  // clear field of boxes and loader
    Point p;
    for (p.x = 0; p.x < emptyField.GetWidth(); p.x++) {
        for (p.y = 0; p.y < emptyField.GetHeight(); p.y++) {
            if (emptyField.IsBoxPlaced(p)) {       // находим места где должны стоять коробки
                BoxPlace box;                      // создаем обьект box
                box.name = emptyField.GetCell(p);  // даем чар из массива чаров это там где нашли по координате p
                box.pos = p;                       // координата p
                boxPlaces.push_back(box);          // save goal location добавляем ее в вектор
            }
        }
    }
}

// Check if a key was hit (non-blocking)
int kbhit(void) {
    struct termios oldt, newt;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);  // get terminal settings
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);                 // disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);          // apply new settings
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);           // get file flags
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);  // set non-blocking mode

    int ch = getchar();  // read character

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);  // restore terminal
    fcntl(STDIN_FILENO, F_SETFL, oldf);       // restore flags

    if (ch != EOF) {
        ungetc(ch, stdin);  // push character back
        return 1;
    }

    return 0;
}

// Print point as (x, y)
std::ostream& operator<<(std::ostream& os, Point p) {
    return os << "(" << p.x << ", " << p.y << ")";
}

unordered_map<Point, BlockType> generator(int H, int W, int targets, int numClusters, int movesQuantity) {
    int attempt = 0;
    const int maxAttempts = 1000;
    std::mt19937 rng(std::random_device{}());
    Point directions[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

    while (attempt++ < maxAttempts) {
        int anchorCounter = 0;
        auto placedBlocks = Field::InitMap(H, W, anchorCounter);
        ReverseMap rev;

        // стены
        for (int c = 0; c < numClusters; ++c) {
            std::uniform_int_distribution<> anchor_dis(0, anchorCounter - 1);
            WallBlock a = Field::GetRandomAnchor(anchor_dis(rng), placedBlocks);
            std::uniform_int_distribution<> dir_dis(0, 3);
            Point dir = directions[dir_dis(rng)];
            std::uniform_int_distribution<> cluster_dis(2, 6);
            int cnt = cluster_dis(rng);

            WallCluster wall;
            for (int i = 1; i < cnt; ++i) {
                Point b{a.pos.x + i * dir.x, a.pos.y + i * dir.y};
                if (b.x <= 0 || b.x >= W - 1 || b.y <= 0 || b.y >= H - 1)
                    continue;
                BlockType t = (i == cnt - 1) ? BlockType::END : BlockType::MIDDLE;
                wall.blocks.push_back({b, t});
            }
            if (Field::WallCorrect(wall, placedBlocks, dir)) {
                Field::AddToPlacedBlocks(wall, placedBlocks);
            }
        }

        // объекты
        rev.boxes = Field::AddBoxes(placedBlocks, H, W);
        rev.player = Field::AddPlayer(placedBlocks, H, W);
        rev.targets = Field::AddTarget(placedBlocks, H, W, targets);

        // Сохраняем карту ДЛЯ ИГРЫ (со всеми объектами)
        auto gameMap = placedBlocks;

        // А вот ДЛЯ BFS чистим игрока и цели
        auto solidMap = placedBlocks;
        solidMap.erase(rev.player);
        for (const auto& t : rev.targets) solidMap.erase(t);

        StateForGenerator start{solidMap, rev, {}};

        StateForGenerator solved = BFSGenerated(start, H, W);
        if (!HaveWonMap(solved)) {
            std::cout << "No solution found on attempt " << attempt << "\n";
            continue;
        }

        // фильтр по минимальному числу толчков (мы пишем их в movesHistory в GenerateNeighbors)
        if (solved.movesHistory.size() >= static_cast<size_t>(movesQuantity)) {
            // ВАЖНО: вернуть ИГРОВУЮ карту (с игроком и целями), иначе игра сразу «выиграна»
            return gameMap;
        }
    }

    throw std::runtime_error("❌ Could not generate solvable map after max attempts");
}

//___________________________________________________________________________________________________________________________________________
//___________________________________________________________________________________________________________________________________________

stringstream MapToStringStream(unordered_map<Point, BlockType>& placedBlocks, int& mapHeight, int& mapWidth) {
    stringstream ss;
    int targetCounter = 0;
    int boxCounter = 0;
    for (int y = 0; y < mapHeight; y++) {
        for (int x = 0; x < mapWidth; x++) {
            char ch = ' ';
            auto it = placedBlocks.find({x, y});
            if (it != placedBlocks.end()) {
                switch (it->second) {
                    case BlockType::ANCHOR:
                        ch = 'o';
                        break;
                    case BlockType::MIDDLE:
                        ch = 'o';
                        break;
                    case BlockType::END:
                        ch = 'o';
                        break;
                    case BlockType::BOX:
                        ch = 'A' + boxCounter;
                        boxCounter++;
                        break;  // ящик
                    case BlockType::PLAYER:
                        ch = 'x';
                        break;  // игрок
                    case BlockType::TARGET:
                        ch = 'a' + targetCounter;
                        targetCounter++;
                        break;  // игрок
                    case BlockType::EMPTY:
                        ch = ' ';  // или что-то по умолчанию
                        break;
                }
            }

            ss << ch;
        }
        ss << '\n';
    }

    return ss;
}

bool HaveWonMap(const StateForGenerator& s) {
    std::unordered_set<Point> boxSet(s.reverseMap.boxes.begin(), s.reverseMap.boxes.end());
    if (boxSet.size() != s.reverseMap.targets.size())
        return false;
    for (const auto& t : s.reverseMap.targets) {
        if (!boxSet.count(t))
            return false;
    }
    return true;
}

AI::AI(std::shared_ptr<GameDescriptor> g, Field& f)
    : game(g), initialState{f, {}} {
    initialState.field.SetGame(g);
}

vector<State> AI::GenerateNeighbors(State& current, StepAnim& anim) {
    // making a vector neghbors for next vertexes
    vector<State> neighbors;
    for (Directions d : {Directions::UP, Directions::DOWN, Directions::LEFT, Directions::RIGHT}) {
        // if in current field in this direction able to move then we add this direction to next field and save it to moves history
        if (current.field.CanMove(d)) {
            State next = current;
            next.field.Move(d, anim);
            next.movesHistory.push_back(d);
            neighbors.push_back(next);
        }
    }
    return neighbors;
}

void AI::BFS(Field& f, StepAnim& anim) {
    queue<State> q;  // queue using for FIFO method which means that the object which was inserted earlier will be used earlier as well
    // here we using it for detecting all states and choosing in the end only those who leads us to win
    set<std::string> visited;  // set using for keeping only unique fields
    q.push(initialState);      //
    vector<State> neighbors;
    while (!q.empty()) {
        State current = q.front();
        q.pop();
        if (current.field.HaveWon(*game)) {
            f = current.field;
            cout << "SOLUTION TO WIN" << endl;
            cout << endl;
            for (auto b : current.movesHistory) {
                if (b == Directions::LEFT) {
                    cout << "left" << endl;
                }
                if (b == Directions::RIGHT) {
                    cout << "right" << endl;
                }
                if (b == Directions::UP) {
                    cout << "up" << endl;
                }
                if (b == Directions::DOWN) {
                    cout << "down" << endl;
                }
            }
            return;
        }
        for (State next : GenerateNeighbors(current, anim)) {
            if (!visited.count(next.field.ToString())) {
                visited.insert(next.field.ToString());
                q.push(next);
            }
        }
    }
    cout << "No solution found." << endl;
}

std::string SerializeMapForHash(const std::unordered_map<Point, BlockType>& map, int H, int W) {
    std::string result;
    result.reserve(H * W + H);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            auto it = map.find({x, y});
            char c = '0';  // пусто
            if (it != map.end()) {
                if (it->second == BlockType::BOX)
                    c = 'B';
                else if (IsWallType(it->second))
                    c = 'W';
                else
                    c = '0';  // игнорируем PLAYER/TARGET и т.п.
            }
            result.push_back(c);
        }
        result.push_back('\n');
    }
    return result;
}

std::string SerializeState(const StateForGenerator& s, int H, int W) {
    std::string key = SerializeMapForHash(s.map, H, W);
    key.push_back('#');
    key += std::to_string(s.reverseMap.player.x);
    key.push_back(',');
    key += std::to_string(s.reverseMap.player.y);
    return key;
}

inline bool InBounds(Point p, int H, int W) noexcept {
    return p.x >= 0 && p.x < W && p.y >= 0 && p.y < H;
}

inline bool IsSolid(const Map& m, Point p) noexcept {
    auto it = m.find(p);
    if (it == m.end()) {
        return false;  // пусто то есть нет в мапе значит точно не коробка и не стена
    }
    return IsWallType(it->second) || it->second == BlockType::BOX;
}

std::vector<uint8_t>
ComputeReachableFlat(const Map& m, Point player, int H, int W) {
    auto idx = [W](Point p) { return p.y * W + p.x; };

    // 0 — недостижимо, 1 — достижимо (без толчков)
    std::vector<uint8_t> vis(H * W, 0);

    // если старт за пределами или занятая клетка — возврат пустой маски
    if (!InBounds(player, H, W) || IsSolid(m, player))
        return vis;

    static constexpr std::array<Point, 4> Kdirs{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

    std::queue<Point> q;
    q.push(player);
    vis[idx(player)] = 1;

    while (!q.empty()) {  // <-- не забыть '!'
        Point u = q.front();
        q.pop();
        for (const auto& d : Kdirs) {
            Point v{u + d};
            if (!InBounds(v, H, W))
                continue;  // вне поля
            if (IsSolid(m, v))
                continue;  // стена/ящик — не пройти
            if (vis[idx(v)])
                continue;  // уже посещали
            vis[idx(v)] = 1;
            q.push(v);
        }
    }
    return vis;
}

std::vector<StateForGenerator> GenerateNeighbors(const StateForGenerator& current, int H, int W) {
    std::vector<StateForGenerator> neighbors;

    auto idx = [W](Point p) { return p.y * W + p.x; };
    static const std::array<Point, 4> dirs{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

    // заливка — куда игрок может добраться БЕЗ толчков
    auto vis = ComputeReachableFlat(current.map, current.reverseMap.player, H, W);

    // для каждого ящика пробуем толкнуть в 4 стороны
    for (Point b : current.reverseMap.boxes) {
        for (Point d : dirs) {
            Point nn = {b.x + d.x, b.y + d.y};    // куда поедет ящик
            Point back = {b.x - d.x, b.y - d.y};  // откуда толкаем (игрок должен туда уметь встать)

            if (!InBounds(nn, H, W) || IsSolid(current.map, nn))
                continue;  // впереди стена/ящик
            if (!InBounds(back, H, W) || !vis[idx(back)])
                continue;  // игрок не может встать за ящик

            // формируем новое состояние
            StateForGenerator next = current;

            // двигаем ящик в карте
            next.map.erase(b);
            next.map[nn] = BlockType::BOX;

            // обновляем список ящиков
            for (auto& bx : next.reverseMap.boxes) {
                if (bx == b) {
                    bx = nn;
                    break;
                }
            }

            // игрок занимает место, где стоял ящик
            next.reverseMap.player = b;

            next.movesHistory.push_back(PointToDirection(d));

            neighbors.push_back(std::move(next));
        }
    }
    return neighbors;
}

StateForGenerator BFSGenerated(const StateForGenerator& start, int H, int W) {
    std::queue<StateForGenerator> q;
    std::unordered_set<std::string> visited;

    q.push(start);
    visited.insert(SerializeState(start, H, W));

    while (!q.empty()) {
        StateForGenerator cur = std::move(q.front());
        q.pop();

        if (HaveWonMap(cur))
            return cur;

        for (auto& nxt : GenerateNeighbors(cur, H, W)) {
            std::string key = SerializeState(nxt, H, W);
            if (visited.insert(key).second) {
                q.push(std::move(nxt));
            }
        }
    }
    std::cout << "No solution found\n";
    return {};  // нет решения
}

// Я хочу интегрировать BFS в генератор. Я думаю о функции где будет один из параметров количество ходов за которые нужно пройти карту.
//  Я понимаю что в нынешней реализации у меня BFS работает с Field но
//  его нет на момент generator. Зато у меня есть unordered map в котором есть
//   информация про всю карту. Просто я думаю тогда нужно иметь не много другой State
//   где вместо field у нас unordered map в принципе вектор с ходами можно оставить.
//   Нужно изменить canMove и move
