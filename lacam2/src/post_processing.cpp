#include "../include/post_processing.hpp"

#include "../include/dist_table.hpp"

bool is_feasible_solution(const Instance& ins, const Solution& solution,
                          const int verbose)
{
  if (solution.empty()) return true;

  // check start locations
  if (!is_same_config(solution.front(), ins.starts)) {
    info(0, verbose, "invalid starts");
    return false;
  }

  // check goal locations
  if (!is_same_config(solution.back(), ins.goals)) {
    info(0, verbose, "invalid goals");
    return false;
  }

  for (size_t t = 1; t < solution.size(); ++t) {
    for (size_t i = 0; i < ins.N; ++i) {
      auto v_i_from = solution[t - 1][i];   // force to get the raw pointers. not anymore
      auto v_i_to = solution[t][i];         // force to get the raw pointers
      // check connectivity
      //if (v_i_from != v_i_to && std::find(v_i_to->neighbor.begin(), v_i_to->neighbor.end(), v_i_from) == v_i_to->neighbor.end()) {
      if(!is_neighbor(v_i_from, v_i_to, ins.G.width) && v_i_from.get()->index != v_i_to.get()->index)
      {
        info(0, verbose, "invalid move");
        std::cout<<"\nFrom : ";
        print_vertex(v_i_from, ins.G.width);
        std::cout<<"\nTo : ";
        print_vertex(v_i_to, ins.G.width);
        std::cout<<"\n";
        return false;
      }

      // check conflicts
      for (size_t j = i + 1; j < ins.N; ++j) {
        auto v_j_from = solution[t - 1][j];  // force to get the raw pointers
        auto v_j_to = solution[t][j];        // force to get the raw pointers
        // vertex conflicts
        if (v_j_to.get()->index == v_i_to.get()->index) {
          info(0, verbose, "vertex conflict between ", i, " and ", j, " at timestep ", t);
          std::cout<<"\nAt : ";
          print_vertex(v_j_to, ins.G.width);
          //std::cout<<"\nTo : ";
          //print_vertex(v_i_to, ins.G.width);
          std::cout<<"\n";
          return false;
        }
        // swap conflicts
        if (v_j_to.get()->index == v_i_from.get()->index && v_j_from.get()->index == v_i_to.get()->index) {
          info(0, verbose, "edge conflict between ", i, " and ", j, " at timestep ", t);
          std::cout<<"\nFrom : ";
          print_vertex(v_i_from, ins.G.width);
          std::cout<<"\nTo : ";
          print_vertex(v_i_to, ins.G.width);
          std::cout<<"\n";
          return false;
        }
      }
    }
  }

  return true;
}


bool is_neighbor(std::shared_ptr<Vertex> v1, std::shared_ptr<Vertex> v2, int width)
{
  int t1 = v1->index;
  int y1 = (int) t1/width;
  int x1 = t1%width;  

  int t2 = v2->index;
  int y2 = (int) t2/width;
  int x2 = t2%width; 

  // if v1 and v2 are neighbors
  if(((abs(x1-x2)==1) && (abs(y1-y2)==0)) != ((abs(y1-y2)==1) && (abs(x1-x2)==0)))
    return true;

  else
    return false;
}

int get_makespan(const Solution& solution)
{
  if (solution.empty()) return 0;
  return solution.size() - 1;
}

int get_path_cost(const Solution& solution, uint i)
{
  const auto makespan = solution.size();
  const auto g = solution.back()[i];
  auto c = makespan;
  while (c > 0 && solution[c - 1][i] == g) --c;
  return c;
}

int get_sum_of_costs(const Solution& solution)
{
  if (solution.empty()) return 0;
  int c = 0;
  const auto N = solution.front().size();
  for (size_t i = 0; i < N; ++i) c += get_path_cost(solution, i);
  return c;
}

int get_sum_of_loss(const Solution& solution)
{
  if (solution.empty()) return 0;
  int c = 0;
  const auto N = solution.front().size();
  const auto T = solution.size();
  for (size_t i = 0; i < N; ++i) {
    auto g = solution.back()[i];
    for (size_t t = 1; t < T; ++t) {
      if (solution[t - 1][i] != g || solution[t][i] != g) ++c;
    }
  }
  return c;
}

int get_makespan_lower_bound(const Instance& ins, DistTable& dist_table)
{
  uint c = 0;
  for (size_t i = 0; i < ins.N; ++i) {
    c = std::max(c, dist_table.get(i, ins.starts[i]));
  }
  return c;
}

int get_sum_of_costs_lower_bound(const Instance& ins, DistTable& dist_table)
{
  int c = 0;
  for (size_t i = 0; i < ins.N; ++i) {
    c += dist_table.get(i, ins.starts[i]);
  }
  return c;
}

void print_stats(const int verbose, const Instance& ins,
                 const Solution& solution, const double comp_time_ms)
{
  auto ceil = [](float x) { return std::ceil(x * 100) / 100; };
  auto dist_table = DistTable(ins);
  const auto makespan = get_makespan(solution);
  const auto makespan_lb = get_makespan_lower_bound(ins, dist_table);
  const auto sum_of_costs = get_sum_of_costs(solution);
  const auto sum_of_costs_lb = get_sum_of_costs_lower_bound(ins, dist_table);
  const auto sum_of_loss = get_sum_of_loss(solution);
  info(1, verbose, "solved: ", comp_time_ms, "ms", "\tmakespan: ", makespan,
       " (lb=", makespan_lb, ", ub=", ceil((float)makespan / makespan_lb), ")",
       "\tsum_of_costs: ", sum_of_costs, " (lb=", sum_of_costs_lb,
       ", ub=", ceil((float)sum_of_costs / sum_of_costs_lb), ")",
       "\tsum_of_loss: ", sum_of_loss, " (lb=", sum_of_costs_lb,
       ", ub=", ceil((float)sum_of_loss / sum_of_costs_lb), ")");
}

// for log of map_name
static const std::regex r_map_name = std::regex(R"(.+/(.+))");

void make_log(const Instance& ins, const Solution& solution,
              const std::string& output_name, const double comp_time_ms,
              const std::string& map_name, const int seed,
              const std::string& additional_info, const bool log_short)
{
  // map name
  std::smatch results;
  const auto map_recorded_name =
      (std::regex_match(map_name, results, r_map_name)) ? results[1].str()
                                                        : map_name;

  // for instance-specific values
  auto dist_table = DistTable(ins);

  // log for visualizer
  auto get_x = [&](int k) { return k % ins.G.width; };
  auto get_y = [&](int k) { return k / ins.G.width; };
  std::ofstream log;
  log.open(output_name, std::ios::out);
  log << "agents=" << ins.N << "\n";
  log << "map_file=" << map_recorded_name << "\n";
  log << "solver=planner\n";
  log << "solved=" << !solution.empty() << "\n";
  log << "soc=" << get_sum_of_costs(solution) << "\n";
  log << "soc_lb=" << get_sum_of_costs_lower_bound(ins, dist_table) << "\n";
  log << "makespan=" << get_makespan(solution) << "\n";
  log << "makespan_lb=" << get_makespan_lower_bound(ins, dist_table) << "\n";
  log << "sum_of_loss=" << get_sum_of_loss(solution) << "\n";
  log << "sum_of_loss_lb=" << get_sum_of_costs_lower_bound(ins, dist_table)
      << "\n";
  log << "comp_time=" << comp_time_ms << "\n";
  log << "seed=" << seed << "\n";
  log << additional_info;
  if (log_short) return;
  log << "starts=";
  for (size_t i = 0; i < ins.N; ++i) {
    int k = ins.starts[i]->index;
    log << "(" << get_x(k) << "," << get_y(k) << "),";
  }
  log << "\ngoals=";
  for (size_t i = 0; i < ins.N; ++i) {
    int k = ins.goals[i]->index;
    log << "(" << get_x(k) << "," << get_y(k) << "),";
  }
  log << "\nsolution=\n";
  for (size_t t = 0; t < solution.size(); ++t) {
    log << t << ":";
    auto C = solution[t];
    for (auto v : C) {
      log << "(" << get_x(v->index) << "," << get_y(v->index) << "),";
    }
    log << "\n";
  }
  log.close();
}


// print the stats to a json-like .txt file
void make_stats(const std::string file_name, const std::string factorize, const int N, 
                const int comp_time_ms, const Infos infos, const Solution solution, const std::string mapname, int success, const std::string multi_threading)
{ 
  std::ofstream out;

  out.open(file_name, std::ios::app);
  out<<"\t{\n";
  out<<"\t\t\"Number of agents\" : \""<<N<<"\",\n";
  out<<"\t\t\"Map name\" : \""<<mapname<<"\",\n";
  out<<"\t\t\"Success\" : \""<<success<<"\",\n";
  out<<"\t\t\"Computation time (ms)\" : \""<<comp_time_ms<<"\",\n";
  out<<"\t\t\"Makespan\" : \""<<get_makespan(solution)<<"\",\n";
  out<<"\t\t\"Factorized\" : \""<<factorize<<"\",\n";
  out<<"\t\t\"Multi threading\" : \""<<multi_threading<<"\",\n";
  out<<"\t\t\"Loop count\" : \""<<infos.loop_count<<"\",\n";
  out<<"\t\t\"PIBT calls\" : \""<<infos.PIBT_calls<<"\",\n";
  out<<"\t\t\"Active PIBT calls\" : \""<<infos.PIBT_calls_active<<"\",\n";
  out<<"\t\t\"Action counts\" : \""<<infos.actions_count<<"\",\n";
  out<<"\t\t\"Active action counts\" : \""<<infos.actions_count_active<<"\",\n";
  out<<"\t\t\"Sum of costs\" : \""<<get_sum_of_costs(solution)<<"\",\n";          // length of all the solutions
  out<<"\t\t\"Sum of loss\" : \""<<get_sum_of_costs(solution)<<"\"\n";            // number of vertex transitions. NO COMMA HEREAFTER
  out<<"\t},\n";

  out.close();
}