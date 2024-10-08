/**
 * @file planner.cpp
 * @brief Implementation of the main solving procedure and all classes related to the planning.
 */
#include "../include/planner.hpp"

// Define the low level node (aka constraint)
LNode::LNode(LNode* parent, uint i, std::shared_ptr<Vertex> v) : 
    who(), 
    where(), 
    depth(parent == nullptr ? 0 : parent->depth + 1)
{
    if (parent != nullptr) {
            who = parent->who;
            who.push_back(i);
            where = parent->where;
            where.push_back(v);
    }
}

uint HNode::HNODE_CNT = 0;

// Define the high-level
HNode::HNode(const Config& _C, DistTable& D, HNode* _parent, const uint _g, const uint _h, const std::vector<float>& priority, const std::vector<int>& enabled) : 
        C(_C),
        parent(_parent),
        neighbor(),
        g(_g),
        h(_h),
        f(g + h),
        priorities(C.size()),
        order(C.size(), 0),
        search_tree(std::queue<LNode*>()),
        depth(_parent == nullptr ? 0 : _parent->depth + 1)  // Initialize depth
{
    ++HNODE_CNT;

    search_tree.push(new LNode());
    const auto N = C.size();

    // update neighbor
    if (parent != nullptr) parent->neighbor.insert(this);

    

    if (parent == nullptr) 
    {
        
        // Use normal ID
        if (priority.empty()) {
            PROFILE_BLOCK("Setting up priorities without parent");
            for (uint i = 0; i < N; ++i)
                priorities[i] = static_cast<float>(D.get(i, C[i])) / N;
            END_BLOCK();
        } 
        else {
            PROFILE_BLOCK("Setting up priorities without parent but using priority array");
            // Initialize priorities based on the provided priority array
            for (uint i = 0; i < N; ++i)
                priorities[i] = priority[i] / N;
            END_BLOCK();
        }
        
    } 
    else {
        
        if (priority.empty()) {
            // Dynamic priorities based on parent, similar to PIBT
            PROFILE_BLOCK("Setting up priorities from parent standard");
            for (size_t i = 0; i < N; ++i) {
                if (D.get(i, C[i]) != 0)
                    priorities[i] = parent->priorities[i] + 1;
                else
                    priorities[i] = parent->priorities[i] - static_cast<int>(parent->priorities[i]);
            }
            END_BLOCK();
        } else {
            // Dynamic priorities with an enabled flag, similar to PIBT
            PROFILE_BLOCK("Setting up priorities from parent but with true ID");
            for (size_t i = 0; i < N; ++i) {
                if (D.get(i, C[i], enabled[i]) != 0)
                    priorities[i] = parent->priorities[i] + 1;
                else
                    priorities[i] = parent->priorities[i] - static_cast<int>(parent->priorities[i]);
            }
            END_BLOCK();
        }
        
    }

    

    // set order
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(),
                [&](uint i, uint j) { return priorities[i] > priorities[j]; });
}

HNode::~HNode()
{
    while (!search_tree.empty()) {
        delete search_tree.front();
        search_tree.pop();
    }
}

// Planner constructor
Planner::Planner(const Instance& _ins, const Deadline* _deadline,
                 std::mt19937* _MT, const int _verbose,
                 const Objective _objective, const float _restart_rate,
                 const Solution& _global_solution)
        : ins(_ins),
        deadline(_deadline),
        MT(_MT),
        verbose(_verbose),
        objective(_objective),
        RESTART_RATE(_restart_rate),
        N(ins.N),
        V_size(ins.G.size()),
        D(DistTable::getInstance()),
        loop_cnt(0),
        C_next(N),
        tie_breakers(V_size, 0),
        A(N, nullptr),
        occupied_now(V_size, nullptr),
        occupied_next(V_size, nullptr),
        global_solution(_global_solution)                  // initialize with nothing
{
}


// Planner constructor
Planner::Planner(std::shared_ptr<Instance> _ins, const Deadline* _deadline,
                 std::mt19937* _MT, const int _verbose,
                 const Objective _objective, const float _restart_rate,
                 const Solution& _global_solution)
        : ins(*_ins.get()),     // get value stored at memory loc
        deadline(_deadline),
        MT(_MT),
        verbose(_verbose),
        objective(_objective),
        RESTART_RATE(_restart_rate),
        N(ins.N),
        V_size(ins.G.size()),
        D(DistTable::getInstance()),
        loop_cnt(0),
        C_next(N),
        tie_breakers(V_size, 0),
        A(N, nullptr),
        occupied_now(V_size, nullptr),
        occupied_next(V_size, nullptr),
        global_solution(_global_solution)                  // initialize with nothing
{
}

Planner::~Planner() {}

/**
 * @brief Standard solver of LaCAM2.
 * @return The solution to the MAPF problem.
 */
Solution Planner::solve(std::string& additional_info, Infos* infos_ptr)
{
    PROFILE_FUNC(profiler::colors::Orange500);
    PROFILE_BLOCK("Initialization");
    solver_info(1, "start search");

    // setup agents
    for (uint i = 0; i < N; ++i) A[i] = new Agent(i);

    // setup search
    auto OPEN = std::stack<HNode*>();
    auto EXPLORED = std::unordered_map<Config, HNode*, ConfigHasher>();
    // insert initial node, 'H': high-level node
    auto H_init = new HNode(ins.starts, D, nullptr, 0, get_h_value(ins.starts));
    OPEN.push(H_init);
    EXPLORED[H_init->C] = H_init;

    std::vector<Config> solution;
    auto C_new = Config(N, nullptr);  // for new configuration
    HNode* H_goal = nullptr;          // to store goal node

    END_BLOCK();
    // DFS
    while (!OPEN.empty() && !is_expired(deadline)) {
        loop_cnt += 1;
        info(1, verbose, "Loop count: ", loop_cnt);

        // do not pop here!
        auto H = OPEN.top();  // high-level node

        // DEBUG PRINT
        info(2, verbose,"\n-------------------------------------------\n");
        info(2, verbose, "- Open a new node (top configuration of OPEN), loop_cnt = ", loop_cnt);
        if(verbose>2) {
            std::cout<<"\n- Printing current configuration : ";
            print_vertices(H->C, ins.G.width);
            std::cout<<"\n";
        }


        // DEBUG print
        // std::cout<<"\nPriorities : ";
        // for (auto i : H->order)
        // std::cout<<H->priorities[i]<<", ";

        // low-level search end
        if (H->search_tree.empty()) {
            OPEN.pop();
            continue;
        }

        // check lower bounds
        if (H_goal != nullptr && H->f >= H_goal->f) {
            OPEN.pop();
            continue;
        }

        // check goal condition
        if (H_goal == nullptr && is_same_config(H->C, ins.goals)) {
            H_goal = H;
            solver_info(1, "found solution, cost: ", H->g);
            if (objective == OBJ_NONE) break;
                continue;
        }

        // create successors at the low-level search
        auto L = H->search_tree.front();
        H->search_tree.pop();
        expand_lowlevel_tree(H, L);

        // create successors at the high-level search
        const auto res = get_new_config(H, L);
        delete L;  // free
        if (!res) continue;

        // create new configuration
        for (auto a : A) C_new[a->id] = a->v_next;

        // check explored list
        const auto iter = EXPLORED.find(C_new);
        if (iter != EXPLORED.end()) {
            // case found
            rewrite(H, iter->second, H_goal, OPEN);
            
            // re-insert or random-restart. Needed to remove for deterministic behavior
            // auto H_insert = (MT != nullptr && get_random_float(MT) >= RESTART_RATE)
            //                     ? iter->second
            //                     : H;
            auto H_insert = iter->second; // Always re-insert the found node

            if (H_goal == nullptr || H_insert->f < H_goal->f) OPEN.push(H_insert);
        } else {
            // insert new search node
            const auto H_new = new HNode(C_new, D, H, H->g + get_edge_cost(H->C, C_new), get_h_value(C_new));
            EXPLORED[H_new->C] = H_new;
            if (H_goal == nullptr || H_new->f < H_goal->f) OPEN.push(H_new);
        }
    }

    // backtrack
    if (H_goal != nullptr) {
        auto H = H_goal;
        while (H != nullptr) {
            solution.push_back(H->C);
            H = H->parent;
        }
        std::reverse(solution.begin(), solution.end());
    }

    // print result
    if (H_goal != nullptr && OPEN.empty()) {
        solver_info(1, "solved optimally, objective: ", objective);
    } else if (H_goal != nullptr) {
        solver_info(1, "solved sub-optimally, objective: ", objective);
    } else if (OPEN.empty()) {
        solver_info(0, "no solution");
    } else {
        solver_info(0, "timeout");
    }

    // logging
    additional_info +="optimal=" + std::to_string(H_goal != nullptr && OPEN.empty()) + "\n";
    additional_info += "objective=" + std::to_string(objective) + "\n";
    additional_info += "loop_cnt=" + std::to_string(loop_cnt) + "\n";
    additional_info += "num_node_gen=" + std::to_string(EXPLORED.size()) + "\n";

    // memory management
    for (auto a : A) delete a;
    for (auto itr : EXPLORED) delete itr.second;

    return solution;
}



/**
 * @brief Factorized version of LaCAM2. The solving is the same as in standard LaCAM but checks for factorizability among agents.
 * @return A bundle containing the sub-instances in case of factorization and the local solution (from start to split).
 */
Bundle Planner::solve_fact(std::string& additional_info, Infos* infos_ptr, FactAlgo& factalgo, PartitionsMap& partitions_per_timestep, bool save_partitions)
{
    PROFILE_FUNC(profiler::colors::Green);
    PROFILE_BLOCK("Initialization");

    // setup agents
    for (uint i = 0; i < N; ++i) A[i] = new Agent(i);

    // usleep(100000);

    // setup search
    auto OPEN = std::stack<HNode*>();
    auto EXPLORED = std::unordered_map<Config, HNode*, ConfigHasher>();
    
    // insert initial node, 'H': high-level node
    auto H = new HNode(ins.starts, D, nullptr, 0, get_h_value(ins.starts, ins.enabled), ins.priority, ins.enabled);
    OPEN.push(H);
    EXPLORED[H->C] = H;

    Solution solution;
    auto C_new = Config(N, nullptr);      // for new configuration
    HNode* H_goal = nullptr;              // to store goal node

    // Config C_goal_overwrite = ins.goals;  // to overwrite goal condition in case of factorization
    std::list<std::shared_ptr<Instance>> sub_instances;

    uint start_time = global_solution[ins.enabled[0]].size();

    // Restore the inheried priorities of agents
    if (ins.priority.size() > 1)
    {
        for (int i=0; i<int(N); i++)
            H->priorities[i] = ins.priority[i];

        // set order in decreasing priority 
        std::iota(H->order.begin(), H->order.end(), 0);
        std::sort(H->order.begin(), H->order.end(),
                [&](int i, int j) { return H->priorities[i] > H->priorities[j]; });
    }

    END_BLOCK();
    // DFS
    while (!OPEN.empty() && !is_expired(deadline)) {
        loop_cnt += 1;

        // do not pop here!
        auto H = OPEN.top();  // high-level node

        

        // DEBUG print
        // std::cout<<"\nPriorities : ";
        // for (auto i : H->order)
        // std::cout<<H->priorities[i]<<", ";  

        // low-level search end
        if (H->search_tree.empty()) {
            OPEN.pop();
            continue;
        }

        // check lower bounds
        if (H_goal != nullptr && H->f >= H_goal->f) {
            OPEN.pop();
            continue;
        }

        // check goal condition
        if (H_goal == nullptr && is_same_config(H->C, ins.goals)) {
            H_goal = H;
            solver_info(1, "found solution, cost: ", H->g);
            // if (objective == OBJ_NONE) break;
            // continue;
            break;
        }

        // create successors at the low-level search
        auto L = H->search_tree.front();
        H->search_tree.pop();
        expand_lowlevel_tree(H, L);

        // DEBUG PRINT
        info(3, verbose,"\n-------------------------------------------\n");
        info(3, verbose, "- Open a new node (top configuration of OPEN), loop_cnt = ", loop_cnt);
        if(verbose>2) {
        std::cout<<"\n- Printing current configuration : ";
        print_vertices(H->C, ins.G.width);
        std::cout<<"\n";
        }

        // create successors at the high-level search
        const auto res = get_new_config(H, L, ins.enabled);
        delete L;  // free
        if (!res) continue;

        // create new configuration
        for (auto a : A) C_new[a->id] = a->v_next;

        std::vector<float> new_priorities;

        // check explored list
        const auto iter = EXPLORED.find(C_new);
        if (iter != EXPLORED.end()) {
            // case found
            rewrite(H, iter->second, H_goal, OPEN);

            // re-insert or random-restart. Needed to remove for deterministic behavior
            // auto H_insert = (MT != nullptr && get_random_float(MT) >= RESTART_RATE)
            //                     ? iter->second
            //                     : H;
            auto H_insert = iter->second; // Always re-insert the found node

            if (H_goal == nullptr || H_insert->f < H_goal->f) {
                new_priorities = H_insert->priorities;
                OPEN.push(H_insert);
            }
        } else {
            // insert new search node
            const auto H_new = new HNode(C_new, D, H, H->g + get_edge_cost(H->C, C_new), get_h_value(C_new, ins.enabled), H->priorities, ins.enabled);
            EXPLORED[H_new->C] = H_new;
            if (H_goal == nullptr || H_new->f < H_goal->f)
            {
                new_priorities = H_new->priorities;
                OPEN.push(H_new);
            }
        }

        // Prepare the distances for A_star planner if needed
        std::vector<int> distances(N);
        if (factalgo.need_astar)
            for(uint i=0; i<N; i++) distances[i] = D.get(i, C_new[i], ins.enabled[i]); // copy the A* path lengths

        uint timestep = start_time + H->depth+1;

        // DUMP TABLE TO SEE
        // std::ostringstream oss;
        // oss << "table_" << timestep << "_" << ins.enabled.size() << ".csv";
        // std::string filename = oss.str();
        // D.dumpTableToFile(filename);

        // Check for factorizability
        if (N>1 && H_goal == nullptr)
        { 
            if (factalgo.use_def)
                sub_instances = factalgo.is_factorizable_def(C_new, ins.goals, verbose, ins.enabled, new_priorities, timestep);
            else 
                sub_instances = factalgo.is_factorizable(C_new, ins.goals, verbose, ins.enabled, distances, new_priorities);

            if (sub_instances.size() > 0)
            {
                H_goal = H;

                // logging
                if (save_partitions) 
                {
                    for (auto ins : sub_instances) {
                        auto active = ins->enabled;
                        partitions_per_timestep[timestep].push_back(active);
                    }
                }  
                break;
            }
        }
    }

    PROFILE_BLOCK("Backtrack and random stuff");

    // backtrack
    if (H_goal != nullptr) {
        auto H = H_goal;
        while (H != nullptr) {
            solution.push_back(H->C);
            H = H->parent;
        }
        std::reverse(solution.begin(), solution.end());
    }

    // print result
    if (H_goal != nullptr && OPEN.empty()) {
        solver_info(1, "solved optimally, objective: ", objective);
    } else if (H_goal != nullptr) {
        solver_info(1, "solved sub-optimally, objective: ", objective);
    } else if (OPEN.empty()) {
        solver_info(0, "no solution");
    } else if (is_expired(deadline)) {
        solver_info(0, "timeout");
    }

    // logging
    //additional_info += "optimal=" + std::to_string(H_goal != nullptr && OPEN.empty()) + "\n";
    //additional_info += "objective=" + std::to_string(objective) + "\n";
    //additional_info += "loop_cnt=" + std::to_string(loop_cnt) + "\n";
    //additional_info += "num_node_gen=" + std::to_string(EXPLORED.size()) + "\n";

    // memory management
    for (auto a : A) delete a;
    for (auto itr : EXPLORED) delete itr.second;


    //infos_ptr->loop_count += loop_cnt;
    //infos_ptr->PIBT_calls_active += N;   // add N computations because the last step is 'amputated'
    //infos_ptr->actions_count_active += N;   // add N computations because the last step is 'amputated'

    return Bundle(transpose(solution), sub_instances);
}


/**
 * @brief Update the relation between 2 configurations by updating their costs and rewriting the net of configurations to converge to optimality.
 */
void Planner::rewrite(HNode* H_from, HNode* H_to, HNode* H_goal, std::stack<HNode*>& OPEN)
{
    // update neighbors. Means H_to is reachable from H_from
    H_from->neighbor.insert(H_to);

    // Dijkstra update of the whole net of High Level nodes in our instance. This is the part of the code that makes LaCAM converge to optimality
    std::queue<HNode*> Q({H_from});  // queue is sufficient
    while (!Q.empty()) {
        auto n_from = Q.front();
        Q.pop();
        for (auto n_to : n_from->neighbor) {
        auto g_val = n_from->g + get_edge_cost(n_from->C, n_to->C);
        if (g_val < n_to->g) {
            if (n_to == H_goal)
            solver_info(1, "cost update: ", n_to->g, " -> ", g_val);
            n_to->g = g_val;
            n_to->f = n_to->g + n_to->h;
            n_to->parent = n_from;
            Q.push(n_to);
            if (H_goal != nullptr && n_to->f < H_goal->f) OPEN.push(n_to);
        }
        }
    }
}


/**
 * @brief Compute the 'edge cost' aka the difference in # of agents at their goal position. Can pass it either configs or 2 Hnodes.
 */
uint Planner::get_edge_cost(const Config& C1, const Config& C2)
{
    if (objective == OBJ_SUM_OF_LOSS) {
        uint cost = 0;
        for (uint i = 0; i < N; ++i) {                              // loop through every agent
            if (C1[i] != ins.goals[i] || C2[i] != ins.goals[i]) {   // for either config, each agent not at its goal position increases cost by one.
                cost += 1;
            }
        }
        return cost;
    }

    // default: makespan
    return 1;
}
uint Planner::get_edge_cost(HNode* H_from, HNode* H_to)
{
    return get_edge_cost(H_from->C, H_to->C);
}

/**
 * @brief Computes the heuristic.
 */ 
uint Planner::get_h_value(const Config& C, const std::vector<int>& enabled)
{
    uint cost = 0;
    if (!enabled.empty()) {
        if (objective == OBJ_MAKESPAN) {
            for (uint i = 0; i < N; ++i) cost = std::max(cost, D.get(i, C[i], enabled[i]));
        } else if (objective == OBJ_SUM_OF_LOSS) {
            for (uint i = 0; i < N; ++i) cost += D.get(i, C[i], enabled[i]);
        }
        return cost;
    }
    else {
        if (objective == OBJ_MAKESPAN) {
            for (uint i = 0; i < N; ++i) cost = std::max(cost, D.get(i, C[i]));
        } else if (objective == OBJ_SUM_OF_LOSS) {
            for (uint i = 0; i < N; ++i) cost += D.get(i, C[i]);
        }
        return cost;
    }
}

void Planner::expand_lowlevel_tree(HNode* H, LNode* L)
{
    if (L->depth >= N) return;
    const int i = H->order[L->depth];
    auto C = H->C[i]->neighbor;
    C.push_back(H->C[i]);
    // randomize
    // if (MT != nullptr) std::shuffle(C.begin(), C.end(), *MT);   // not ramdomize
    // insert
    for (auto v : C) H->search_tree.push(new LNode(L, i, v));
}

/**
 * @brief Creates a new configuration given some constraints for the next step. Basically the same as in LaCAM.
 */
bool Planner::get_new_config(HNode* H, LNode* L, const std::vector<int>& enabled)
{
    PROFILE_FUNC(profiler::colors::Yellow);

    // setup cache
    for (auto a : A) {
            // clear previous cache
            if (a->v_now != nullptr && occupied_now[a->v_now->id] == a) {
                occupied_now[a->v_now->id] = nullptr;
            }
            if (a->v_next != nullptr) {
                occupied_next[a->v_next->id] = nullptr;
                a->v_next = nullptr;
            }

            // set occupied now
            a->v_now = H->C[a->id];
            occupied_now[a->v_now->id] = a;
    }

    // add constraints
    for (uint k = 0; k < L->depth; ++k) {
        const int i = L->who[k];        // agent
        const int l = L->where[k]->id;  // loc

        // check vertex collision
        if (occupied_next[l] != nullptr) return false;
        // check swap collision
        auto l_pre = H->C[i]->id;
        if (occupied_next[l_pre] != nullptr && occupied_now[l] != nullptr &&
            occupied_next[l_pre]->id == occupied_now[l]->id)
        return false;

        // set occupied_next
        A[i]->v_next = L->where[k];
        occupied_next[l] = A[i];
    }

    // perform PIBT
    if (!enabled.empty()) {       // if factorized use
        for (int k : H->order) {
            auto a = A[k];
            if (a->v_next == nullptr && !funcPIBT(a, enabled)) return false;  // planning failure
        }
        return true;
    } 
    else {                        // standard use
        for (int k : H->order) {
            auto a = A[k];
            if (a->v_next == nullptr && !funcPIBT(a)) return false;  // planning failure
        }
        return true;
    }
}


/**
 * @brief PIBT planner for the low level node.
 */
bool Planner::funcPIBT(Agent* ai, const std::vector<int>& enabled)
{
    const auto i = ai->id;
    const size_t K = ai->v_now->neighbor.size();

    // get candidates for next locations
    for (size_t k = 0; k < K; ++k) {
        auto u = ai->v_now->neighbor[k];
        C_next[i][k] = u;
        if (MT != nullptr) tie_breakers[u->id] = get_random_float(MT);  // set tie-breaker
    }
    C_next[i][K] = ai->v_now;

    // sort in ascending descending order of priority
    Agent* swap_agent;
    
    if (enabled.empty()) {     
        std::sort(C_next[i].begin(), C_next[i].begin() + K + 1,
                [&](std::shared_ptr<Vertex> const v, std::shared_ptr<Vertex> const u) {
                    return D.get(i, v) + tie_breakers[v->id] <
                        D.get(i, u) + tie_breakers[u->id];
        });

        swap_agent = swap_possible_and_required(ai);
        if (swap_agent != nullptr)
            std::reverse(C_next[i].begin(), C_next[i].begin() + K + 1);
    }
    else {
        const int true_id = enabled[i];
        std::sort(C_next[i].begin(), C_next[i].begin() + K + 1,
                [&](std::shared_ptr<Vertex> const v, std::shared_ptr<Vertex> const u) {
                    return D.get(i, v, true_id) + tie_breakers[v->id] <
                        D.get(i, u, true_id) + tie_breakers[u->id];
                });
        swap_agent = swap_possible_and_required_fact(ai, enabled);
        if (swap_agent != nullptr)
            std::reverse(C_next[i].begin(), C_next[i].begin() + K + 1);
    }

    //DEBUG PRINT
    if(verbose > 3)
    {
        Config C_next_vector2(C_next[i].begin(), C_next[i].end());
        info(2, verbose, "-- Order of preference for actions for agent ", i, " : ");
        for (size_t k=0; k<=K; k++)
        {
            print_vertex(C_next[i][k], ins.G.width);
            std::cout<<" (d="<<D.get(i, C_next[i][k])<<") // ";
        }
        std::cout<<"\n";
    }

    // main operation
    for (size_t k = 0; k < K + 1; ++k) {
        auto u = C_next[i][k];

        // avoid vertex conflicts
        if (occupied_next[u->id] != nullptr) continue;

        auto& ak = occupied_now[u->id];

        // avoid swap conflicts
        if (ak != nullptr && ak->v_next == ai->v_now) continue;

        // reserve next location
        occupied_next[u->id] = ai;
        ai->v_next = u;

        // priority inheritance
        if (!enabled.empty() && ak != nullptr && ak != ai && ak->v_next == nullptr && !funcPIBT(ak, enabled))
            continue;
        else if (ak != nullptr && ak != ai && ak->v_next == nullptr && !funcPIBT(ak))
            continue;

        // success to plan next one step
        // pull swap_agent when applicable
        if (k == 0 && swap_agent != nullptr && swap_agent->v_next == nullptr && occupied_next[ai->v_now->id] == nullptr) {
            swap_agent->v_next = ai->v_now;
            occupied_next[swap_agent->v_next->id] = swap_agent;
        }
        return true;
    }

    // failed to secure node
    occupied_next[ai->v_now->id] = ai;
    ai->v_next = ai->v_now;
    return false;
}

/**
 * @brief Define the swap operation.
 */
Agent* Planner::swap_possible_and_required(Agent* ai)
{
    const int i = ai->id;
    // ai wanna stay at v_now -> no need to swap
    if (C_next[i][0] == ai->v_now) return nullptr;

    // usual swap situation, c.f., case-a, b
    auto aj = occupied_now[C_next[i][0]->id];
    if (aj != nullptr && aj->v_next == nullptr &&
        is_swap_required(ai->id, aj->id, ai->v_now, aj->v_now) &&
        is_swap_possible(aj->v_now, ai->v_now)) 
    {
        return aj;
    }

    // for clear operation, c.f., case-c
    for (auto u : ai->v_now->neighbor) {
        auto ak = occupied_now[u->id];
        if (ak == nullptr || C_next[i][0] == ak->v_now) continue;
        if (is_swap_required(ak->id, ai->id, ai->v_now, C_next[i][0]) &&
            is_swap_possible(C_next[i][0], ai->v_now)) {
        return ak;
        }
    }

    return nullptr;
}

/**
 * @brief Simulate whether the swap is required.
 */
bool Planner::is_swap_required(const uint pusher, const uint puller, std::shared_ptr<Vertex> v_pusher_origin, std::shared_ptr<Vertex> v_puller_origin)
{
    auto v_pusher = v_pusher_origin;
    auto v_puller = v_puller_origin;
    std::shared_ptr<Vertex> tmp = nullptr;
    while (D.get(pusher, v_puller) < D.get(pusher, v_pusher)) {
        auto n = v_puller->neighbor.size();
        // remove agents who need not to move
        for (auto u : v_puller->neighbor) {
        auto a = occupied_now[u->id];
        if (u == v_pusher ||
            (u->neighbor.size() == 1 && a != nullptr && ins.goals[a->id] == u)) {
            --n;
        } else {
            tmp = u;
        }
        }
        if (n >= 2) return false;  // able to swap
        if (n <= 0) break;
        v_pusher = v_puller;
        v_puller = tmp;
    }

    // judge based on distance
    return (D.get(puller, v_pusher) < D.get(puller, v_puller)) &&
            (D.get(pusher, v_pusher) == 0 ||
            D.get(pusher, v_puller) < D.get(pusher, v_pusher));
}

/**
 * @brief Simulate whether the swap is possible.
 */
bool Planner::is_swap_possible(std::shared_ptr<Vertex> v_pusher_origin, std::shared_ptr<Vertex> v_puller_origin)
{
    auto v_pusher = v_pusher_origin;
    auto v_puller = v_puller_origin;
    std::shared_ptr<Vertex> tmp = nullptr;
    while (v_puller != v_pusher_origin) {  // avoid loop
        auto n = v_puller->neighbor.size();  // count #(possible locations) to pull
        for (auto u : v_puller->neighbor) {
            auto a = occupied_now[u->id];
            if (u == v_pusher ||
                (u->neighbor.size() == 1 && a != nullptr && ins.goals[a->id] == u)) {
                --n;      // pull-impossible with u
            } else {
                tmp = u;  // pull-possible with u
            }
        }
        if (n >= 2) return true;  // able to swap
        if (n <= 0) return false;
        v_pusher = v_puller;
        v_puller = tmp;
    }
    return false;
}

/**
 * @brief Define the swap operation but with true IDs of the agents.
 */
Agent* Planner::swap_possible_and_required_fact(Agent* ai, const std::vector<int>& enabled)
{
    const int i = ai->id;
    const int true_id = enabled[i];

    // ai wanna stay at v_now -> no need to swap
    if (C_next[i][0] == ai->v_now) return nullptr;

    // usual swap situation, c.f., case-a, b
    auto aj = occupied_now[C_next[i][0]->id];
    if (aj != nullptr && aj->v_next == nullptr &&
        is_swap_required(true_id, enabled[aj->id], ai->v_now, aj->v_now) &&
        is_swap_possible(aj->v_now, ai->v_now)) {
        return aj;
    }

    // for clear operation, c.f., case-c
    for (auto u : ai->v_now->neighbor) {
        auto ak = occupied_now[u->id];
        if (ak == nullptr || C_next[i][0] == ak->v_now) continue;
        if (is_swap_required(enabled[ak->id], true_id, ai->v_now, C_next[i][0]) &&
            is_swap_possible(C_next[i][0], ai->v_now)) {
            return ak;
        }
    }

    return nullptr;
}

/**
 * @brief Simulate whether the swap is required but with true IDs of the agents.
 */
bool Planner::is_swap_required_fact(const uint true_pusher_id, const uint true_puller_id, std::shared_ptr<Vertex> v_pusher_origin, std::shared_ptr<Vertex> v_puller_origin)
{
    auto v_pusher = v_pusher_origin;
    auto v_puller = v_puller_origin;
    std::shared_ptr<Vertex> tmp = nullptr;
    while (D.get(true_pusher_id, v_puller, true_pusher_id) < D.get(true_pusher_id, v_pusher, true_pusher_id)) {
        auto n = v_puller->neighbor.size();
        // remove agents who need not to move
        for (auto u : v_puller->neighbor) {
        auto a = occupied_now[u->id];
        if (u == v_pusher ||
            (u->neighbor.size() == 1 && a != nullptr && ins.goals[a->id] == u)) {
            --n;
        } else {
            tmp = u;
        }
        }
        if (n >= 2) return false;  // able to swap
        if (n <= 0) break;
        v_pusher = v_puller;
        v_puller = tmp;
    }

    // judge based on distance
    return (D.get(true_puller_id, v_pusher, true_puller_id) < D.get(true_puller_id, v_puller, true_puller_id)) &&
            (D.get(true_pusher_id, v_pusher, true_pusher_id) == 0 ||
            D.get(true_pusher_id, v_puller, true_pusher_id) < D.get(true_pusher_id, v_pusher, true_pusher_id));
}

// Just some printing stuff to visualize objective
std::ostream& operator<<(std::ostream& os, const Objective obj)
{
    if (obj == OBJ_NONE) {
        os << "none";
    } else if (obj == OBJ_MAKESPAN) {
        os << "makespan";
    } else if (obj == OBJ_SUM_OF_LOSS) {
        os << "sum_of_loss";
    }
    return os;
}



/**
 * @brief Transposes a matrix. Used to transpose the solution to have timesteps in y-axis instead of x-axis.
 */
Solution transpose(const Solution& matrix) {

    if (matrix.empty() || matrix[0].empty())
    { 
        std::cout<<"\nempty matrix";
        return {};
    }

    size_t numRows = matrix.size();
    size_t numCols = matrix[0].size();

    // Ensure all rows have the same number of columns
    for (const auto& row : matrix) {
        if (row.size() != numCols) {
            throw std::invalid_argument("All rows in the input matrix must have the same number of columns.");
        }
    }

    // Initialize the transposed matrix with the correct dimensions
    Solution transposed(numCols, Config(numRows, nullptr));

    // Perform the transposition
    for (size_t i = 0; i < numRows; ++i) {
        for (size_t j = 0; j < numCols; ++j) {
            transposed[j][i] = matrix[i][j];
        }
    }

    return transposed;
}

/**
 * @brief Pad the global solution to make sure the matrix is full. For agents that reached their goal sooner than others, it adds the goal position to the end of their solution until the matrix is full.
 */
void padSolution(Solution& sol) {
    // Find the length of the longest row
    size_t maxLength = 0;
    for (const auto& row : sol) {
        if (row.size() > maxLength) {
            maxLength = row.size();
        }
    }

    // Pad each row with its last element until it reaches maxLength
    for (auto& row : sol) {
        if (!row.empty()) {
            auto lastElement = row.back();
            while (row.size() < maxLength) {
                row.push_back(lastElement);
            }
        }
    }
}