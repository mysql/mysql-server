#ifndef DIJKSTRAS_FUNCTOR_INCLUDED
#define DIJKSTRAS_FUNCTOR_INCLUDED

#include <queue>            // std::deque
#include <unordered_map>    // std::unordered_map & std::unordered_multimap
#include <functional>       // std::function
#include <algorithm>        // std::push_heap & std::reverse
#include <cmath>            // INFINITY

#include "include/map_helpers.h" // malloc_unordered_multimap

/**
 * @brief Edge data for Dijkstra functor
 * 
 */
struct Edge {
  int id;
  // node id
  int from, to;
  // weight
  double cost;
};

/**
 * @brief functor for A* (finds shortest path)
 * 
 */
class Dijkstra {
  typedef malloc_unordered_multimap<int, const Edge*>::const_iterator edge_iterator;

  // #iterations/#nodes popped in Dijkstra::operator() before callback param stop is called
  static constexpr unsigned int iters_per_callback = 255;
  PSI_memory_key psi_key;

  // key = Edge.from
  const malloc_unordered_multimap<int, const Edge*>* m_edges;
  const std::function<double(const int& point_id)> m_heu;

  /**
   * @brief Node data (internal use)
   * 
   */
  struct Point {
      // sum of edge.cost in path
      double cost = INFINITY;
      // cost_heu = real_cost + heuristic
      double cost_heu = INFINITY;
      // used in retrace() to return path
      // linked list in Edge would speed up retrace(), but also mutate m_edges
      const Edge* path = nullptr;
  };
  malloc_unordered_map<int, Point> m_point_map;

  // comparator used for point_heap sorting to make min heap based on m_point_map.cost_heu
  struct greater_point_heuristic_comparator {
    bool operator()(const int& a, const int& b) { return point_map->at(a).cost_heu > point_map->at(b).cost_heu; }
    const malloc_unordered_map<int, Point>* point_map = nullptr;
  } heap_cmp;
  std::deque<int, Malloc_allocator<int>> point_heap;

 public:
  /**
   * @brief Construct a new Dijkstra object
   * 
   * @param edges_lookup_from key must equal edge start node id (i.e. Edge.from)
   * @param heu_func A* heuristic. If not supplied normal dijkstra will be used
   */
  Dijkstra(const malloc_unordered_multimap<int, const Edge*>* edges,
           const std::function<double(const int& point_id)>& heu_func = [](const int&) -> double { return 0.0; },
           PSI_memory_key psi_key = PSI_NOT_INSTRUMENTED);
  /**
   * @brief runs A* to find shortest path through m_edges
   * 
   * @param start_point_id node id of path start
   * @param end_point_id node if of path end
   * @param total_cost l-val-ref returns total cost of found path (if path exists)
   * @param stop callback for exiting function. called every ...
   * @return std::vector<const Edge*> vector of pointers, pointing to edges in
   *  m_edges, representing found path, or empty vector if stoped by param stop or no path exists
   */
  std::vector<const Edge*> operator()(const int& start_point_id, const int& end_point_id, double& total_cost,
                                      const std::function<bool()>& stop = []() -> bool { return false; });
 private:
  /**
   * @brief finds path by accumulating Point.path from m_point_map and reverting their order
   * ! NB: will deref invalid ptr if path doesn't exist
   * @param from_point node id of path start
   * @param to_point node id of path end
   * @return std::vector<const Edge*> path found in m_point_map.path
   */
  std::vector<const Edge*> retrace(int from_point, const int& to_point);
};

#endif /* DIJKSTRAS_FUNCTOR_INCLUDED */
