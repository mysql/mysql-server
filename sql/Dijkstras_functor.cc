#include "sql/Dijkstras_functor.h"

std::vector<const Edge*> Dijkstra::operator()(const int& start_point_id, const int& end_point_id, double& total_cost) {
    empty_path_maps();
    int point = start_point_id;
    set_point(point, 0, m_heu(point), nullptr);
    m_popped_map[point] = true;
    // A*
    while (point != end_point_id) {
      const double point_cost = m_cost_map[point];
      const std::pair<edge_iterator, edge_iterator> edge_range_it = m_edges_lookup_from.equal_range(point);
      // checks all edges from point (i.e. current point)
      for (edge_iterator edge_it = edge_range_it.first; edge_it != edge_range_it.second; edge_it++) {
        const Edge *edge = edge_it->second;
        // ignore edges to popped(visited) nodes
        if (contains(m_popped_map, edge->to)) continue;
        // ignore edge if new path cost > prev path cost to same node
        double old_cost, new_cost = point_cost + edge->cost;
        if (extract(m_cost_map, edge->to, old_cost) && new_cost > old_cost) continue;
        
        set_point(edge->to, new_cost, new_cost + m_heu(edge->to), edge);
        point_heap.push_back(edge->to);
        std::push_heap(point_heap.begin(), point_heap.end(), heap_cmp);
      }
      if (point_heap.empty()) return {};
      point = point_heap.front();
      point_heap.pop_front();
      m_popped_map[point] = true;
    }
    total_cost = m_cost_map[point];
    return retrace(point, start_point_id);
}

template<typename Key, typename T>
inline bool Dijkstra::contains(const std::unordered_map<Key, T>& map, const Key& key) {
	return map.find(key) != map.end();
}
template<typename Key, typename T>
inline bool Dijkstra::extract(const std::unordered_map<Key, T>& map, const Key& key, T& val) {
	auto pair = map.find(key);
	bool found = pair != map.end();
  if (found) val = pair->second;
  return found;
}
void Dijkstra::set_point(const int& id, const double& cost, const double& heu_cost, const Edge *const path) {
  	m_cost_map[id] = cost; m_heu_cost_map[id] = heu_cost; m_path_map[id] = path;
}
std::vector<const Edge*> Dijkstra::retrace(int from_point, int to_point) {
  std::vector<const Edge*> path;
  while (from_point != to_point) {
    const Edge* path_ = m_path_map[from_point];
    path.push_back(path_);
    from_point = path_->from;
  }
  std::reverse(path.begin(), path.end());
  return path;
}
void Dijkstra::empty_path_maps(){
  m_popped_map.clear(); m_cost_map.clear(); m_heu_cost_map.clear(); m_path_map.clear();
  point_heap.clear();
}
