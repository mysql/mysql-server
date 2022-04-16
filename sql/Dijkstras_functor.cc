#include "sql/Dijkstras_functor.h"
#ifdef MYSQL_SERVER
#include "sql/malloc_allocator.h"
#endif

template<class EdgeAllocator>
Dijkstra<EdgeAllocator>::Dijkstra(const EdgeMapType* edges,
           const std::function<double(const int& point_id)>& heu_func,
           const std::function<void*(const size_t n)>& allocate)
    : m_edges(edges), m_heu(heu_func),
      m_point_map(CallbackAllocator<std::pair<const int, Point>>(allocate)), m_point_heap(CallbackAllocator<int>(allocate)) {}

template<class EdgeAllocator>
std::vector<const Edge*> Dijkstra<EdgeAllocator>::operator()(const int& start_point_id, const int& end_point_id, double& total_cost,
                                                             int *popped_points, const std::function<bool()>& stop){
  m_point_map.clear();
  m_point_heap.clear();
  int popped_points_ = 0;
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

      m_point_heap.push_back(edge->to);
      std::push_heap(m_point_heap.begin(), m_point_heap.end(), heap_cmp);
    }
    if (m_point_heap.empty() || stop())
      return {};
    point = m_point_heap.front();
    m_point_heap.pop_front();
    node = m_point_map[point];
    popped_points_++;
  }
  if (popped_points)
    *popped_points = popped_points_;
  total_cost = node.cost;
  return retrace(point, start_point_id);
}

template<class EdgeAllocator>
std::vector<const Edge*> Dijkstra<EdgeAllocator>::retrace(int from_point, const int& to_point){
  std::vector<const Edge*> path;
  while (from_point != to_point) {
    const Edge* path_ = m_point_map[from_point].path;
    path.push_back(path_);
    from_point = path_->from;
  }
  std::reverse(path.begin(), path.end());
  return path;
}


template class Dijkstra<std::allocator<std::pair<const int, const Edge*>>>;
#ifdef MYSQL_SERVER
template class Dijkstra<Malloc_allocator<std::pair<const int, const Edge*>>>;
#endif
