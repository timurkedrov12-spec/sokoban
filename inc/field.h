#ifndef FIELD_H
#define FIELD_H
#include <SFML/Graphics.hpp>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <vector>
using namespace std;

constexpr int CELL_SIZE = 40;  // размер клетки в пикселях

inline constexpr float kStepDuration = 0.12f;  // секунд на шаг

struct Point {
    int x;
    int y;

    Point operator+(const Point other) const {
        return {x + other.x, y + other.y};
    }

    Point operator-(const Point other) const {
        return {x - other.x, y - other.y};
    }

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

static inline Point pxToCell(const sf::Vector2f& p) {
    return {int((p.x - 1.f) / CELL_SIZE), int((p.y - 1.f) / CELL_SIZE)};
}

static inline sf::Vector2f cellToPx(int x, int y) {
    return {x * CELL_SIZE + 1.f, y * CELL_SIZE + 1.f};
}

enum class BlockType : uint8_t {
    EMPTY = 0,
    ANCHOR = 1,
    MIDDLE = 2,
    END = 3,
    BOX = 4,
    PLAYER = 5,
    TARGET = 6
};

struct WallBlock {
    Point pos;
    BlockType type;
};

struct WallCluster {
    vector<WallBlock> blocks;
};

enum class Directions {
    LEFT,
    RIGHT,
    UP,
    DOWN
};

namespace std {
template <>
struct hash<Point> {
    size_t operator()(const Point& p) const {
        return hash<int>()(p.x) ^ (hash<int>()(p.y) << 1);
    }
};
}  // namespace std

struct BoxPlace {
    char name;
    Point pos;
};

class GameDescriptor;

struct StepAnim;

class Field {
   private:
    /* data */
    int width;
    int height;
    char* arr;
    std::shared_ptr<GameDescriptor> game;
    unordered_map<Point, BlockType> placedBlocks;

   public:
    // static Point GetDirectionVector(Directions d);
    Field(std::string fileName);
    Field(std::stringstream& ss);
    Field(const Field& other);
    Field(Field&& Other) noexcept;
    Field& operator=(const Field& other);
    Field& operator=(Field&& other) noexcept;
    ~Field();
    void Generate(int mapHeight, int mapWidth, int targets, int numClusters, int movesQuantity);
    void Print();
    bool HaveWon(const GameDescriptor& g);
    bool CanMove(Directions d);
    bool Move(Directions d, StepAnim& anim);
    bool IsEmptySpace(Point p);
    bool IsBox(Point p);
    bool IsBoxPlaced(Point p);
    static bool IsInRestrictedRadius(WallBlock& block, unordered_map<Point, BlockType>& placedBlocks, Point& random_dir, WallCluster& wall);
    static bool IsMiddleBlockAcceptable(WallBlock& block, unordered_map<Point, BlockType>& placedBlocks, Point& random_dir, WallCluster& wall);
    static bool WallCorrect(WallCluster& wall, unordered_map<Point, BlockType>& placedBlocks, Point& random_dir);

    void Clear();
    // getter & setter
    char& GetCell(Point p);
    Point GetDirectionVector(Directions d);
    Point GetLoaderPosition();
    static array<Point, 5> GetNeighborPointsArray(WallBlock& block, Point& random_dir);
    static array<Point, 8> GetEightDirections();
    static WallBlock GetRandomAnchor(int SelectedAnchor, unordered_map<Point, BlockType>& placedBlocks);
    int GetHeight() { return height; }
    int GetWidth() { return width; }
    char GetBackgroundAt(Point p) const;

    void SetGame(std::shared_ptr<GameDescriptor> g);
    string ToString() const;
    void LoadFromGenerator(std::stringstream& ss);

    static unordered_map<Point, BlockType> InitMap(int& mapHeight, int& mapWidth, int& anchorCounter);
    // adders
    static void AddToPlacedBlocks(WallCluster wall, unordered_map<Point, BlockType>& placedBlocks);
    static vector<Point> AddBoxes(unordered_map<Point, BlockType>& placedBlocks, int& mapHeight, int& mapWidth);
    static Point AddPlayer(unordered_map<Point, BlockType>& placedBlocks, int& mapHeight, int& mapWidth);
    static vector<Point> AddTarget(unordered_map<Point, BlockType>& placedBlocks, int& mapHeight, int& mapWidt, int& targets);
};

void ReloadMap(Field& f, const std::string& fileName);

void GenerateNewMap(Field& f, std::stringstream& ss);

class GameDescriptor {
   public:
    Field emptyField;
    std::vector<BoxPlace> boxPlaces;
    GameDescriptor(Field& field);
};

void SaveFieldToFile(Field& f, const string filename);

struct StepAnim {
    sf::Vector2f PlayerFrom{}, PlayerTo{};
    optional<sf::Vector2f> BoxFrom, BoxTo;
    float duration = kStepDuration;
    sf::Clock clock;
    std::optional<char> boxChar;  // <— добавили
    static sf::Vector2f lerp(sf::Vector2f a, sf::Vector2f b, float t) {
        return a + (b - a) * t;
    }

    void start(sf::Vector2f pf, sf::Vector2f pt,
               std::optional<sf::Vector2f> bf = std::nullopt,
               std::optional<sf::Vector2f> bt = std::nullopt,
               std::optional<char> bc = std::nullopt,
               std::optional<float> durationOverrideSec = std::nullopt) {
        PlayerFrom = pf;
        PlayerTo = pt;
        BoxFrom = bf;
        BoxTo = bt;
        boxChar = bc;
        if (durationOverrideSec && *durationOverrideSec > 0.01f) {
            duration = *durationOverrideSec;
        }  // защита от деления на ноль
        clock.restart();
    }

    float progress() const {
        return std::min(clock.getElapsedTime().asSeconds() / duration, 1.f);
    }

    bool isFinished() {
        return progress() >= 1.f;
    }

    sf::Vector2f playerPos() {
        return lerp(PlayerFrom, PlayerTo, progress());
    }

    std::optional<sf::Vector2f> boxPos() const {
        if (BoxFrom && BoxTo)
            return lerp(*BoxFrom, *BoxTo, progress());
        return std::nullopt;
    }
};

#endif