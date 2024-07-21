/*
 * lacam-star
 */

#pragma once


#include "dist_table.hpp"
#include "graph.hpp"
#include "instance.hpp"
#include "factorizer.hpp"
#include "utils.hpp"

// objective function
enum Objective { OBJ_NONE, OBJ_MAKESPAN, OBJ_SUM_OF_LOSS };
std::ostream& operator<<(std::ostream& os, const Objective objective);

// PIBT agent
struct Agent {
  const uint id;
  std::shared_ptr<Vertex> v_now;   // current location
  std::shared_ptr<Vertex> v_next;  // next location
  Agent(uint _id) : id(_id), v_now(nullptr), v_next(nullptr) {}
};
using Agents = std::vector<Agent*>;

// low-level node
struct LNode {
  std::vector<uint> who;
  Vertices where;
  const uint depth;
  LNode(LNode* parent = nullptr, uint i = 0,
        std::shared_ptr<Vertex> v = nullptr);  // who and where
};

// high-level node
struct HNode {
  static uint HNODE_CNT;  // count #(high-level node)
  const Config C;

  // tree
  HNode* parent;
  std::set<HNode*> neighbor;

  // costs
  uint g;        // g-value (might be updated)
  const uint h;  // h-value
  uint f;        // g + h (might be updated)

  // for low-level search
  std::vector<float> priorities;
  std::vector<uint> order;
  std::queue<LNode*> search_tree;

  HNode(const Config& _C, DistTable& D, HNode* _parent, const uint _g,
        const uint _h);
  ~HNode();
};
using HNodes = std::vector<HNode*>;

// return structure for planner::sove_fact
struct Bundle{
  std::list<std::shared_ptr<Instance>> instances;
  Solution solution;

  // constructor
  Bundle();
  // ~Bundle();
};

struct Planner {
  const Instance& ins;
  const Deadline* deadline;
  std::mt19937* MT;
  const int verbose;

  // hyper parameters
  const Objective objective;
  const float RESTART_RATE;  // random restart

  // solver utils
  const uint N;       // number of agents
  const uint V_size;  // number o vertices
  DistTable D;
  uint loop_cnt;      // auxiliary

  // used in PIBT
  std::vector<std::array<std::shared_ptr<Vertex>, 5> > C_next;  // next locations, used in PIBT
  std::vector<float> tie_breakers;              // random values, used in PIBT
  Agents A;
  Agents occupied_now;                          // for quick collision checking
  Agents occupied_next;                         // for quick collision checking

  // used for factorization
  std::shared_ptr<Sol> empty_solution;    // pointer to the empty solution

  // constructor
  Planner(const Instance& _ins, const Deadline* _deadline, std::mt19937* _MT,
          const int _verbose = 0,
          // other parameters
          const Objective _objective = OBJ_NONE,
          const float _restart_rate = 0.001,
          std::shared_ptr<Sol> _empty_solution = {});

  // cosntructor for MT
  Planner(std::shared_ptr<Instance> _ins, const Deadline* _deadline, std::mt19937* _MT,
          const int _verbose = 0,
          // other parameters
          const Objective _objective = OBJ_NONE,
          const float _restart_rate = 0.001,
          std::shared_ptr<Sol> _empty_solution = {});


  ~Planner();

  // standard solving
  Solution solve(std::string& additional_info, Infos* infos_ptr);
  // factorized solving
  Bundle solve_fact(std::string& additional_info, Infos* infos_ptr, FactAlgo& factalgo);
  
  void expand_lowlevel_tree(HNode* H, LNode* L);
  void rewrite(HNode* H_from, HNode* T, HNode* H_goal,
               std::stack<HNode*>& OPEN);
  uint get_edge_cost(const Config& C1, const Config& C2);
  uint get_edge_cost(HNode* H_from, HNode* H_to);
  uint get_h_value(const Config& C);
  bool get_new_config(HNode* H, LNode* L);
  bool funcPIBT(Agent* ai);

  // swap operation
  Agent* swap_possible_and_required(Agent* ai);
  bool is_swap_required(const uint pusher, const uint puller,
                        std::shared_ptr<Vertex> v_pusher_origin, std::shared_ptr<Vertex> v_puller_origin);
  bool is_swap_possible(std::shared_ptr<Vertex> v_pusher_origin, std::shared_ptr<Vertex> v_puller_origin);

  // utilities
  template <typename... Body>
  void solver_info(const int level, Body&&... body)
  {
    if (verbose < level) return;
    std::cout << "elapsed:" << std::setw(6) << elapsed_ms(deadline) << "ms"
              << "  loop_cnt:" << std::setw(8) << loop_cnt
              << "  node_cnt:" << std::setw(8) << HNode::HNODE_CNT << "\t";
    info(level, verbose, (body)...);
  }
};

// helper functions
Solution transpose(const Solution& matrix);
void padSolution(std::shared_ptr<Sol>& sol);