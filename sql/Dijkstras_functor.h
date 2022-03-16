#ifndef DIJKSTRAS_FUNCTOR_INCLUDED
#define DIJKSTRAS_FUNCTOR_INCLUDED

#include <queue>
#include <unordered_map>
#include <functional>
#include <algorithm>

struct Edge {
  int id;
  int from, to;
  double cost;
};

class Dijkstra {
  typedef std::unordered_multimap<int, Edge*>::iterator edge_iterator;

  std::unordered_multimap<int, Edge*> edges_lookup_from;
  std::function<double(const int& point_id)> heu = [](const int&) -> double { return 0.0; };
  std::unordered_map<int, bool> popped_map;
  std::unordered_map<int, double> cost_map, heu_cost_map; // heu_cost = real_cost + heuristic
  std::unordered_map<int, const Edge*> path_map;

  struct greater_point_heuristic_comparator {
    bool operator()(const int& a, const int& b) { return val.at(a) > val.at(b); }
    const std::unordered_map<int, double>& val;
  } heap_cmp{ heu_cost_map };
  std::deque<int> point_heap;

 public:
  Dijkstra(std::unordered_multimap<int, Edge*> edges_lookup_from,
            std::function<double(const int& point_id)> heu_func = [](const int&) -> double { return 0.0; })
    : edges_lookup_from(edges_lookup_from), heu(heu_func) {}

  std::vector<const Edge*> operator()(const int& start_point_id, const int& end_point_id, double& total_cost);
 private:
  template<typename Key, typename T>
  inline static bool contains(std::unordered_map<Key, T> map, Key key) {
  	return map.find(key) != map.end();
  }
  template<typename Key, typename T>
  bool extract(std::unordered_map<Key, T> map, Key key, T& val) {
  	auto pair = map.find(key);
  	bool found = pair != map.end();
    if (found) val = pair->second;
    return found;
  }
  void set_point(const int& id, const double& cost, const double& heu_cost, const Edge *const path);
  std::vector<const Edge*> retrace(int from_point, int to_point);
  void empty_path_maps();
};

#endif
