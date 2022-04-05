#include "sql/Dijkstras_functor.h"

Dijkstra::Dijkstra(const malloc_unordered_multimap<int, const Edge*>* edges,
  const std::function<double(const int& point_id)>& heu_func,
  PSI_memory_key psi_key)
  : m_edges(edges), m_heu(heu_func),
    m_point_map(psi_key), heap_cmp{ &m_point_map },
    point_heap(Malloc_allocator<int>(psi_key)) {}

std::vector<const Edge*> Dijkstra::operator()(const int& start_point_id, const int& end_point_id, double& total_cost) {
    m_point_map.clear();
    point_heap.clear();
    int point = start_point_id; // node id
    Point& node = m_point_map[point] = Point{ 0, m_heu(point), nullptr };
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
      if (point_heap.empty()) return {};
      point = point_heap.front();
      point_heap.pop_front();
      node = m_point_map[point];
    }
    total_cost = node.cost;
    return retrace(point, start_point_id);
}
std::vector<const Edge*> Dijkstra::retrace(int from_point, const int& to_point) {
  std::vector<const Edge*> path;
  while (from_point != to_point) {
    const Edge* path_ = m_point_map[from_point].path;
    path.push_back(path_);
    from_point = path_->from;
  }
  std::reverse(path.begin(), path.end());
  return path;
}
