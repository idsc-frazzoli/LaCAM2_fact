/*
 * instance definition
 */
#pragma once

// define the uint type even if it should be handled by the compiler
#ifndef uint
typedef unsigned int uint;
#endif

#include <random>

#include "graph.hpp"
#include "utils.hpp"

struct Instance {
  const Graph G;
  Config starts;
  Config goals;
  const std::vector<int> enabled;             // list of enabled agents ("absolute ids of the agents in this instance/partition")
  const std::map<int, int> agent_map;         // instance_id of the agent to absolute_id (#todo not necessary if enabled is sorted)
  const uint N;                               // number of agents
  const std::vector<float> priority = {0.0};  // priority of agents

  // for factorization (more robust)
  Instance(const Graph& _G, Config& _starts, Config& _goals, const std::vector<int>& _enabled, std::map<int, int>& _agent_map, const int _N, const std::vector<float>& _priority);
  
  // for MAPF benchmark
  Instance(const std::string& scen_filename, const std::string& map_filename, const std::vector<int>& _enabled, std::map<int, int>& _agent_map, const int _N = 1);

  // Rule of five
  Instance(const Instance& other);                // Copy constructor
  Instance(Instance&& other) noexcept;            // Move constructor
  Instance& operator=(const Instance& other);     // Copy assignment operator
  Instance& operator=(Instance&& other) noexcept; // Move assignment operator
  ~Instance();                                    // Destructor

  // simple feasibility check of instance
  bool is_valid(const int verbose = 0) const;
};

// solution: a sequence of configurations
using Solution = std::vector<Config>;

struct Sol {
  Solution solution;
  Sol(int N);         // constructor
};


std::ostream& operator<<(std::ostream& os, const Solution& solution);
