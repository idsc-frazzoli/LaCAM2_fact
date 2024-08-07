/*
 * post processing, e.g., calculating solution quality
 */
#pragma once
#include "instance.hpp"
#include "factorizer.hpp"

bool is_feasible_solution(const Instance& ins, const Solution& solution,
                          const int verbose = 0);
                          bool is_neighbor(std::shared_ptr<Vertex> v1, std::shared_ptr<Vertex> v2, int width);

int get_makespan(const Solution& solution);

int get_path_cost(const Solution& solution, uint i);  // single-agent path cost

int get_sum_of_costs(const Solution& solution);

int get_sum_of_loss(const Solution& solution);

int get_makespan_lower_bound(const Instance& ins, DistTable& D);

int get_sum_of_costs_lower_bound(const Instance& ins, DistTable& D);

void print_stats(const int verbose, const Instance& ins,
                 const Solution& solution, const double comp_time_ms);

void make_log(const Instance& ins, const Solution& solution,
              const std::string& output_name, const double comp_time_ms,
              const std::string& map_name, const int seed,
              const std::string& additional_info,
              PartitionsMap& partitions_per_timestep,
              const bool log_short = false);  // true -> paths not appear

void make_stats(const std::string file_name, const std::string factorize, const int N, 
                const int comp_time_ms, const Infos infos, const Solution solution, const std::string mapname, int success, const std::string multi_threading);


void write_partitions(const PartitionsMap& partitions_per_timestep);