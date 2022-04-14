#ifndef DIJKSTRAS_FUNCTOR_INCLUDED
#define DIJKSTRAS_FUNCTOR_INCLUDED

#include <queue>            // std::deque
#include <unordered_map>    // std::unordered_map & std::unordered_multimap
#include <functional>       // std::function
#include <algorithm>        // std::push_heap & std::reverse
#include <cmath>            // INFINITY

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
 * @brief allocator for external allocation e.g. by callback func.
 *  * doesn't deallocate unless default i.e. CallbackAllocator()
 * 
 * @tparam T allocated type
 */
template<class T>
class CallbackAllocator {
 public:
  std::function<void*(const size_t)> m_callback;
  typedef T value_type;
  typedef size_t size_type;
  template<class O>
  CallbackAllocator(const CallbackAllocator<O>& other) noexcept :
    m_callback(other.m_callback) {}
 /**
  * @brief Construct a new Callback Allocator object
  * 
  * @param callback must allocate n bytes
  */
  CallbackAllocator(const std::function<void*(const size_t)>& callback = {}) :
    m_callback(callback) {}
  T* allocate(const size_t n) {
    if (n == 0)
      return nullptr;
    if (m_callback)
      return (T*) m_callback(n * sizeof(T));
    return (T*) malloc(n * sizeof(T));
  }
  void deallocate(T* const ptr, const size_t) {
    if (m_callback)
      return;
    free(ptr);
  }
};

/**
 * @brief functor for A* (finds shortest path)
 * 
 */
template<class EdgeAllocator>
class Dijkstra {
  typedef std::unordered_multimap<int, const Edge*, std::hash<int>, std::equal_to<int>, EdgeAllocator> EdgeMapType;
  typedef typename EdgeMapType::const_iterator edge_iterator;
  // key = Edge.from
  const EdgeMapType* m_edges;
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
  std::unordered_map<int, Point, std::hash<int>, std::equal_to<int>, CallbackAllocator<std::pair<const int, Point>>> m_point_map;
  std::deque<int, CallbackAllocator<int>> point_heap;

  // comparator used for point_heap sorting to make min heap based on m_point_map.cost_heu
  struct greater_point_heuristic_comparator {
    bool operator()(const int& a, const int& b) { return point_map->at(a).cost_heu > point_map->at(b).cost_heu; }
    const decltype(m_point_map)* point_map = nullptr;
  } heap_cmp { &m_point_map };

 public:
  /**
   * @brief Construct a new Dijkstra object
   * 
   * @param edges key must equal edge start node id (i.e. Edge.from)
   * @param heu_func A* heuristic. If not supplied normal dijkstra will be used
   * @param allocate custom allocator e.g. for measuring memory usage
   */
  Dijkstra(const EdgeMapType* edges,
           const std::function<double(const int& point_id)>& heu_func = [](const int&) -> double { return 0.0; },
           const std::function<void*(const size_t n)>& allocate = {})
    : m_edges(edges), m_heu(heu_func),
      m_point_map(CallbackAllocator<std::pair<const int, Point>>(allocate)), point_heap(CallbackAllocator<int>(allocate)) {}
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
                                      const std::function<bool()>& stop = []() -> bool { return false; }){
    m_point_map.clear();
    point_heap.clear();
    int point = start_point_id; // node id
    Point& node = m_point_map[point] = Point{ 0, /*m_heu(point)*/ 0, nullptr };
    // A*
    while (point != end_point_id) {
      const std::pair<edge_iterator, edge_iterator> edge_range_it = m_edges->equal_range(point);
      // checks all edges from point (i.e. current point)
      for (edge_iterator edge_it = edge_range_it.first; edge_it != edge_range_it.second; edge_it++) {
        const Edge *edge = edge_it->second;
        // grabs existing node or creates new with cost of INFINITY inside map
        Point& node_to = m_point_map[edge->to];
        
        // ignore longer paths
        double new_cost = node.cost + edge->cost;
        if (new_cost >= node_to.cost)
            continue;
        
        node_to.cost = new_cost;
        node_to.cost_heu = new_cost + m_heu(edge->to);
        node_to.path = edge;

        point_heap.push_back(edge->to);
        std::push_heap(point_heap.begin(), point_heap.end(), heap_cmp);
      }
      if (point_heap.empty() || stop())
        return {};
      point = point_heap.front();
      point_heap.pop_front();
      node = m_point_map[point];
    }
    total_cost = node.cost;
    return retrace(point, start_point_id);
  }
 private:
  /**
   * @brief finds path by accumulating Point.path from m_point_map and reverting their order
   * ! NB: will deref invalid ptr if path doesn't exist
   * @param from_point node id of path start
   * @param to_point node id of path end
   * @return std::vector<const Edge*> path found in m_point_map.path
   */
  std::vector<const Edge*> retrace(int from_point, const int& to_point){
    std::vector<const Edge*> path;
    while (from_point != to_point) {
      const Edge* path_ = m_point_map[from_point].path;
      path.push_back(path_);
      from_point = path_->from;
    }
    std::reverse(path.begin(), path.end());
    return path;
  }

};

#endif /* DIJKSTRAS_FUNCTOR_INCLUDED */
