#ifndef PROG_H
#define PROG_H

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <queue>
#include <set>
#include <vector>

#include "../inc/field.h"

using namespace std;

using Map = unordered_map<Point, BlockType>;

int kbhit(void);

bool IsWallType(BlockType t);

struct State {
    Field field;
    std::vector<Directions> movesHistory;
};

struct ReverseMap {
    Point player;
    vector<Point> targets;
    vector<Point> boxes;
};

struct StateForGenerator {
    unordered_map<Point, BlockType> map;
    ReverseMap reverseMap;
    std::vector<Directions> movesHistory;
};

bool IsWallType(BlockType t);

unordered_map<Point, BlockType> generator(int mapHeight, int mapWidth, int targets, int numClusters, int movesQuantity);

stringstream MapToStringStream(unordered_map<Point, BlockType>& placedBlocks, int& mapHeight, int& mapWidth);

string SerializeMapForHash(const unordered_map<Point, BlockType>& map, int height, int width);

std::string SerializeState(const StateForGenerator& s, int h, int w);

vector<StateForGenerator> GenerateNeighbors(StateForGenerator& current);

StateForGenerator BFSGenerated(const StateForGenerator& start, int H, int W);

bool CanMoveMap(Point direction, StateForGenerator& state);

bool IsEmptySpaceMap(unordered_map<Point, BlockType>& PlacedBlocks, const Point& pos);

bool IsBoxMap(unordered_map<Point, BlockType>& PlacedBlocks, Point& pos);

Directions PointToDirection(Point p);

bool MoveMap(Point direction, StateForGenerator& state);

bool HaveWonMap(const StateForGenerator& s);

ReverseMap BuildReverseMap(const std::unordered_map<Point, BlockType>& map);

class AI {
   private:
    /* data */
    std::shared_ptr<GameDescriptor> game;
    State initialState;

   public:
    AI(std::shared_ptr<GameDescriptor> g, Field& f);
    vector<State> GenerateNeighbors(State& current, StepAnim& anim);
    void BFS(Field& f, StepAnim& anim);
};

inline bool InBounds(Point p, int H, int W) noexcept;

inline bool IsSolid(const Map& m, Point p) noexcept;

vector<uint8_t> ComputeReachableFlat(const Map& m, Point player, int H, int W);
// тогда между крайним блоком может быть расстояние в один блок между другим блоком. Также мы проверяем является ли этот блок кластером его координаты.

#endif