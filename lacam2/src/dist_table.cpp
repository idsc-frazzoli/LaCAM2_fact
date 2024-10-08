/**
 * @file dist_table.cpp
 * @brief Implementation of the DistTable class (initialization and singleton management) and especially the Lazy BFS evaluation.
 */

#include "../include/dist_table.hpp"

// Static pointer to DistTable.
DistTable* DistTable::instance = nullptr;

// Singleton pointer manager.
DistTable& DistTable::getInstance() {
    if (instance == nullptr) {
        throw std::runtime_error("DistTable instance not initialized. Call initialize() first.");
    }
    return *instance;
}

// Initialize the Instance
void DistTable::initialize(const Instance& ins) {
    if (instance == nullptr) {
        instance = new DistTable(ins);
    } else {
        throw std::runtime_error("DistTable instance already initialized.");
    }
}

// Cleanup the Instance
void DistTable::cleanup() {
    delete instance;
    instance = nullptr;
}


// Default constructor
DistTable::DistTable(const Instance& ins)
    : V_size(ins.G.V.size()), table(ins.N, std::vector<uint>(V_size, V_size))
{
    PROFILE_BLOCK("setup dist_table");
    for (size_t i = 0; i < ins.N; ++i) {
        OPEN.push_back(std::queue<Vertex*>());
        auto n = ins.goals[i].get();
        OPEN[i].push(n);
        table[i][n->id] = 0;
    }
    END_BLOCK();
}


/**
 * @brief Returns the estimated distance to goal for an agent using A*
 * @param i agent id
 * @param v_id id of current vertex
 * @param true_id true id of agent in case of factorization
 */
uint DistTable::get(uint i, uint v_id, int true_id)
{
  // Override the id by the true_id if it is known
  if (true_id > 0) i = true_id;

  // Return value if already known
  if (table[i][v_id] < V_size) return table[i][v_id];

  /*
   * BFS with lazy evaluation
   * c.f., Reverse Resumable A*
   * https://www.aaai.org/Papers/AIIDE/2005/AIIDE05-020.pdf
   *
   * sidenote:
   * tested RRA* but lazy BFS was much better in performance
   */
  while (!OPEN[i].empty()) {
    auto n = OPEN[i].front();
    OPEN[i].pop();
    const int d_n = table[i][n->id];
    for (auto& m : n->neighbor) {
        const int d_m = table[i][m->id];
        if (d_n + 1 >= d_m) continue;
        table[i][m->id] = d_n + 1;
        OPEN[i].push(m.get());
    }
    if (n->id == int(v_id)) return d_n;
  }
  return V_size;
}


uint DistTable::get(uint i, std::shared_ptr<Vertex> v, int true_id) { return get(i, v.get()->id, true_id); }


/// Helper function to save the content of the DistTable, useful for debug.
void DistTable::dumpTableToFile(const std::string& filename) const {
    std::ofstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Unable to open file for writing");
    }

    // Dump the distance table
    file << "Distance Table:" << std::endl;
    for (size_t i = 0; i < table.size(); ++i) {
        file << "Agent " << i << ": ";
        for (size_t j = 0; j < table[i].size(); ++j) {
            file << std::setw(4) << table[i][j] << " ";
        }
        file << std::endl;
    }

    // Optionally dump the OPEN queues (if needed)
    file << std::endl << "OPEN Queues:" << std::endl;
    for (size_t i = 0; i < OPEN.size(); ++i) {
        file << "Agent " << i << ": ";
        std::queue<Vertex*> queue_copy = OPEN[i]; // Copy to avoid modifying original
        while (!queue_copy.empty()) {
            file << queue_copy.front()->id << " ";
            queue_copy.pop();
        }
        file << std::endl;
    }

    file.close();
}