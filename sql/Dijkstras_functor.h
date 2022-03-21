#ifndef DIJKSTRAS_FUNCTOR_INCLUDED
#define DIJKSTRAS_FUNCTOR_INCLUDED

#include <queue>
#include <unordered_map>
#include <functional>
#include <algorithm>

/**
 * @brief Edge data for Dijkstra functor
 * 
 */
struct Edge {
  int id;
  int from, to;
  double cost;
};

/**
 * @brief functor for A* (finds shortest path)
 * 
 */
class Dijkstra {
  typedef std::unordered_multimap<int, const Edge*>::const_iterator edge_iterator;

  // key = Edge.from
  const std::unordered_multimap<int, const Edge*>* m_edges_lookup_from;
  const std::function<double(const int& point_id)> m_heu = [](const int&) -> double { return 0.0; };

  std::unordered_map<int, bool> m_popped_map;
  std::unordered_map<int, double> m_cost_map, m_heu_cost_map; // heu_cost = real_cost + heuristic
  std::unordered_map<int, const Edge*> m_path_map;

  // comparator used for point_heap sorting to make min heap based on m_heu_cost_map
  struct greater_point_heuristic_comparator {
    bool operator()(const int& a, const int& b) { return val.at(a) > val.at(b); }
    const std::unordered_map<int, double>& val;
  } heap_cmp{ m_heu_cost_map };
  std::deque<int> point_heap;

 public:
  /**
   * @brief Construct a new Dijkstra object
   * 
   * @param edges_lookup_from key must equal edge start node id (i.e. Edge.from)
   * @param heu_func A* heuristic. If not supplied normal dijkstra will be used
   */
  Dijkstra(const std::unordered_multimap<int, const Edge*>* edges_lookup_from,
           const std::function<double(const int& point_id)>& heu_func = [](const int&) -> double { return 0.0; })
    : m_edges_lookup_from(edges_lookup_from), m_heu(heu_func) {}
  /**
   * @brief runs A* to find shortest path through m_edges_lookup_from
   * 
   * @param start_point_id node id of path start
   * @param end_point_id node if of path end
   * @param total_cost l-val-ref returns total cost of found path (if path exists)
   * @return std::vector<const Edge*> vector of pointers pointing to edges in
   *  edges_lookup_from representing found path
   */
  std::vector<const Edge*> operator()(const int& start_point_id, const int& end_point_id, double& total_cost);
 private:
  /**
   * @brief checks if map contains key
   * 
   * @tparam Key type
   * @tparam T value type
   * @param map 
   * @param key 
   * @return true if map contains key
   * @return false if map doesn't contain key
   */
  template<typename Key, typename T>
  static inline bool contains(const std::unordered_map<Key, T>& map, const Key& key);
  /**
   * @brief extracts value from map with given key
   * 
   * @tparam Key type
   * @tparam T value type
   * @param map 
   * @param key 
   * @param val 
   * @return true if map contains key
   * @return false if map doesn't contain key
   */
  template<typename Key, typename T>
  static inline bool extract(const std::unordered_map<Key, T>& map, const Key& key, T& val);
  /**
   * @brief stores point info in node maps (i.e. m_xxx_map)
   * 
   * @param id node id
   * @param cost dijkstra path cost
   * @param heu_cost cost + heuristic_cost (e.g. euclidean dist)
   * @param path ptr to edge leading to node
   */
  inline void set_point(const int& id, const double& cost, const double& heu_cost, const Edge *const path);
  /**
   * @brief finds path by accumulating edge_ptrs from m_path_map and reverting their order
   *   NB: will deref invalid ptr if path doesn't exist
   * @param from_point node id of path start
   * @param to_point node id of path end
   * @return std::vector<const Edge*> path found in m_path_map
   */
  std::vector<const Edge*> retrace(int from_point, const int& to_point);

  /**
   * @brief empties all node maps (i.e. m_xxx_map) to remove prev path data
   * 
   */
  inline void empty_path_maps();
};

#endif /* DIJKSTRAS_FUNCTOR_INCLUDED */
