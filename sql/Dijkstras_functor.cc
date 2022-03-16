#include "sql/Dijkstras_functor.h"

std::vector<const Edge*> Dijkstra::operator()(const int& start_point_id, const int& end_point_id, double& total_cost) {
    empty_path_maps();
    int point = start_point_id;
    set_point(point, 0, heu(point), nullptr);
    popped_map[point] = true;
    // A*
    while (point != end_point_id) {
      const double point_cost = cost_map[point];
      const std::pair<edge_iterator, edge_iterator> edge_range_it = edges_lookup_from.equal_range(point);
      for (edge_iterator edge_it = edge_range_it.first; edge_it != edge_range_it.second; edge_it++) {
        const Edge* edge = edge_it->second;
        if (contains(popped_map, edge->to)) continue;
        double old_cost, new_cost = point_cost + edge->cost;
        const bool found = extract(cost_map, edge->to, old_cost);
        if (found && new_cost > old_cost) continue;
        set_point(edge->to, new_cost, new_cost + heu(edge->to), edge);
        point_heap.push_back(edge->to);
        std::push_heap(point_heap.begin(), point_heap.end(), heap_cmp);
      }
      if (point_heap.empty()) return {};
      point = point_heap.front();
      point_heap.pop_front();
      popped_map[point] = true;
    }
    total_cost = cost_map[point];
    return retrace(point, start_point_id);
}
void Dijkstra::set_point(const int& id, const double& cost, const double& heu_cost, const Edge *const path) {
  	cost_map[id] = cost; heu_cost_map[id] = heu_cost; path_map[id] = path;
}
std::vector<const Edge*> Dijkstra::retrace(int from_point, int to_point) {
  std::vector<const Edge*> path;
  while (from_point != to_point) {
    const Edge* path_ = path_map[from_point];
    path.push_back(path_);
    from_point = path_->from;
  }
  std::reverse(path.begin(), path.end());
  return path;
}
void Dijkstra::empty_path_maps(){
  popped_map = {}; cost_map = {}; heu_cost_map = {}; path_map = {};
  point_heap = {};
}
