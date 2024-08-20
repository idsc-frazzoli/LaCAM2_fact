/*
 * distance table with lazy evaluation, using BFS
 */
#pragma once

#include "graph.hpp"
#include "instance.hpp"
#include "utils.hpp"

// Singleton pattern
struct DistTable {

    // Singleton management
    static DistTable& getInstance();
    static void initialize(const Instance& ins);
    static void cleanup();

    // DistTable constructor
    DistTable(const Instance& ins);
    DistTable(const Instance* ins);
    
    const uint V_size;                              // number of vertices
    std::vector<std::vector<uint>> table;           // distance table, index: agent-id & vertex-id. Used to to keep track of the shortest distances from each goal vertex to all other vertices in the graph.
    std::vector<std::queue<Vertex*>> OPEN;          // search queue

    uint get(uint i, uint v_id, int true_id = -1);
    uint get(uint i, std::shared_ptr<Vertex> v, int true_id = -1);

    // const int get_length(int i, int v_id) const;

    

    void setup(const Instance& ins);  // initialization with const ref
    void setup(const Instance* ins);  // initialization with raw ptr

private:
    // Static pointer to the single instance of the class
    static DistTable* instance;
};
