import json
from os.path import join, dirname as up
from collections import defaultdict
from typing import Iterable, List, Tuple, Dict
from src.utils import run_command_in_ubuntu, parse_file

class Instance:
    """
    Represents an instance of a task with start positions, goal positions, enabled agents, and start time.

    Attributes:
        starts (list)   : A list of tuples representing the starting positions of agents.
        goals (list)    : A list of tuples representing the goal positions of agents.
        enabled (list)  : A list of integers representing the indices of agents that are enabled for the task.
        time_start (int): The start time for the instance.
    """
    def __init__(self, starts: List[Tuple[int, int]], goals: List[Tuple[int, int]], enabled: List[int], time_start: int):
        self.starts = starts
        self.goals = goals
        self.enabled = enabled
        self.time_start = time_start
        

def get_partitions(iterable: Iterable) -> List[List[List[int]]]:
    """
    Generate all possible partitions of an iterable into non-empty subsets.

    Args:
        iterable (Iterable): An iterable (e.g., list or set) to be partitioned.

    Returns:
        list: A list of partitions, where each partition is represented as a list of subsets.
    
    Note : The partitions are sorted in decreasing order of the number of subsets.
    """
    def partitions(s):
        if not s:
            return [[]]
        first, *rest = s
        rest_partitions = partitions(rest)
        result = []
        for partition in rest_partitions:
            # Adding the first element as a new subset
            result.append([[first]] + partition)
            # Adding the first element to each existing subset
            for i in range(len(partition)):
                new_partition = partition[:i] + [[first] + partition[i]] + partition[i+1:]
                result.append(new_partition)
        return result
    
    s = list(iterable)
    all_partitions = partitions(s)

    # Return the partitions in decreasing order of cardinality
    return sorted(all_partitions, key=len, reverse=True)

def create_temp_scenario(
    enabled: List[int], 
    starts: List[Tuple[int, int]], 
    goals: List[Tuple[int, int]], 
    map_name: str):
    """
    Create a temporary scenario file based on provided agent information and map name.

    Args:
        enabled (list)  : A list indicating which agents are enabled.
        starts (list)   : A list of tuples representing the starting positions of the agents.
        goals (list)    : A list of tuples representing the goal positions of the agents.
        map_name (str)  : The name of the map for which the scenario file is created.

    Returns: None

    Raises:
        ValueError:
            If the provided map name is not supported.

    Note : The file is written to the 'temp' directory within the 'assets' directory of the project.
    """
    assets_path = up(up(__file__))                                      # LaCAM2_fact/assets
    temp_filepath = join(assets_path, 'temp', 'temp_scenario.scen')     # LaCAM2_fact/assets/temp/temp_scenario.scen
    
    with open(temp_filepath, 'w') as new_file:
        for agent in range(len(enabled)):
            start = starts[agent]
            goal = goals[agent]

            start_s = f"{start[0]}\t{start[1]}"
            goal_s = f"{goal[0]}\t{goal[1]}"


            if map_name == 'warehouse_small' :
                new_file.write('1\twarehouse_small.map\t33\t57\t'+ start_s + '\t' + goal_s + '\t1\n')

            elif map_name == 'warehouse_large' :
                new_file.write('1\twarehouse_large.map\t140\t500\t'+ start_s + '\t' + goal_s + '\t1\n')

            elif map_name == 'warehouse-20-40-10-2-2' :
                new_file.write('1\twarehouse-20-40-10-2-2.map\t164\t340\t'+ start_s + '\t' + goal_s + '\t1\n')

            elif map_name in ["random-32-32-10", "random-32-32-20"] :
                new_file.write('1\t' + map_name + '.map\t32\t32\t'+ start_s + '\t' + goal_s + '\t1\n')

            elif map_name == 'test-5-5' :
                new_file.write('1\test-5-5.map\t5\t5\t'+ start_s + '\t' + goal_s + '\t1\n')

            else :
                raise ValueError("Mapname is not supported")


def write_sol(
    local_solution: List[List[int]], 
    enabled: List[int], 
    global_solution: List[List[int]], 
    N: int):
    """
    Write the local solution to the global solution.

    Args:
        local_solution (list)   : A list of paths, where each sublist contains vertex indices corresponding to agent positions of the freshly solved sub-problem.
        enabled (list)          : A list of indices indicating which lines in the global solution to update.
        global_solution (list)  : A list of paths, where each sublist contains vertex indices corresponding to agent positions of the ORIGINAL problem.
        N (int)                 : The number of agents enabled in the sub-problem.

    Returns:
        None

    Notes:
        - For each index in the range N, the function appends vertices from the solution to the corresponding line in `global_solution`.
        - The `enabled` list determines which lines in `global_solution` are updated with the vertices from `local_solution`.
    """
    for id in range(N):
        sol_bit = local_solution[id]              # Access the solution at index id
        line = global_solution[enabled[id]]

        for v in sol_bit:
            line.append(v)  # Append each vertex to the line


def update_local_solution(
    temp_solution: List[List[int]], 
    local_solution: dict, 
    enabled_agents: List[int], 
    enabled_ins: List[int]):
    """
    Updates the local solution with the temporary solution of enabled agents.
    
    Args:
        temp_solution (list)    : List of tuples representing the solution steps for enabled agents.
        local_solution (dict)   : Dictionary representing the global solution steps.
        enabled_agents (list)   : List of enabled agent global IDs.
        enabled_ins (list)      : List of enabled agent local IDs.
    """
    N = len(enabled_ins)

    for step, positions in temp_solution:

        if step not in local_solution:
            local_solution[step] = [(-1, -1)]*N

        for i, id_glob in enumerate(enabled_agents) :
            id_loc = enabled_ins.index(id_glob)
            local_solution[step][id_loc] = positions[i]


def pad_local_solution(
    local_solution: dict, 
    n: int, 
    goals: List[Tuple[int, int]]):
    """
    Pads the local solution for agents that have reached their goals.
    
    Args:
        local_solution (dict)   : Dictionary representing the global solution steps.
        n (int)                 : Number of enabled agent.
        goals (list)            : List of goal positions for all agents.
    """
    for step, positions in local_solution.items():
        if step != 0 :
            for agent_id in range(n):
                if positions[agent_id] == (-1,-1):
                    # If the agent is not in the local solution for this step, add its goal position
                    local_solution[step][agent_id] = goals[agent_id]



def is_neighbor(pos1: Tuple[int, int], pos2: Tuple[int, int]):
    """
    Check if pos1 and pos2 are neighbors in a grid of given width.
    
    Args:
        pos1 (tuple):   Position (x, y) of the first point.
        pos2 (tuple):   Position (x, y) of the second point.
    
    Returns:
        bool: True if the positions are neighbors, False otherwise.
    """
    x1, y1 = pos1
    x2, y2 = pos2
    if abs(x1 - x2) + abs(y1 - y2) == 1:  # Manhattan distance should be 1
        return True
    return False


def is_valid_solution(local_solution: dict):
    """
    Validates the solution by checking for vertex collisions, edge collisions,
    correct start and goals, and connectivity.
    
    Args:
        local_solution (dict):  Dictionary representing the global solution steps.
    
    Returns:
        bool: True if the solution is valid, False otherwise.
    """

    # Check for vertex and edge collisions, and connectivity
    last_positions = local_solution[0]
    final_step = max(local_solution.keys())
    for step in range(1, final_step + 1):
        current_positions = {}
        
        for agent_id, pos in enumerate(local_solution[step]):
            # Check for vertex collisions
            if pos in current_positions.values():
                # print(f"Vertex conflict at step {step} between agents")
                return False
            current_positions[agent_id] = pos
            
            # Check connectivity
            last_pos = last_positions[agent_id]
            if pos != last_pos and not is_neighbor(last_pos, pos):
                # print(f"Invalid move for agent {agent_id} from {last_pos} to {pos} at step {step}")
                return False
        
        # Check for edge collisions (swap conflicts)
        for i in range(len(local_solution[step])):
            for j in range(i + 1, len(local_solution[step])):
                if (local_solution[step][i] == last_positions[j] and 
                    local_solution[step][j] == last_positions[i]):
                    # print(f"Edge conflict between agents {i} and {j} at step {step}")
                    return False
        
        last_positions = current_positions

    return True


def clean_partition_dict(partition_dict: dict):
    """
    Cleans the partition dictionary by removing entries where the partitions do not change
    from one timestep to the next.

    Args:
        partition_dict (dict): Dictionary with timesteps as keys and partition lists as values.

    Returns:
        dict: Cleaned dictionary with only the entries where partitions change.
    """
    cleaned_dict = {}
    prev_partition = None

    for timestep, partitions in partition_dict.items():
        if partitions != prev_partition:
            cleaned_dict[timestep] = partitions
            prev_partition = partitions

    return cleaned_dict


def max_fact_partitions(map_name: str, N: int):
    """
    Compute the maximum factorization for a given map and number of agents, and store partitions at each timestep.
    Basically the python version of LaCAM2 that can call the cpp version of LaCAM2.

    Args:
        map_name (str): The name of the map for which the factorization is to be computed.
        N (int):        The number of agents involved in the factorization.

    Returns: None

    Notes:
        - The function first sets up necessary directories and extracts map width.
        - It launches an external process to solve the MAPF problem and parses the result.
        - It creates initial instances and manages an open list of instances to solve.
        - For each instance, it generates all possible partitions and solves the problem for each partition.
        - Valid solutions are added to the global solution, and partitions per timestep are recorded.
        - Sub-instances are created for further processing and added to the open list.
        - The results, including partitions per timestep, are saved to a JSON file.
    """
    # Setup directories 
    base_path = up(up(up(__file__)))     # LaCAM2_fact/
    res_path = join(base_path, 'build', 'result.txt')

    # Launch lacam a first time and parse result
    start_comm = "build/main -i assets/maps/" + map_name + "/other_scenes/" + map_name + "-" + str(N) + ".scen -m assets/maps/" + map_name + "/" + map_name + ".map -N " + str(N) + " -v 0 -s"

    run_command_in_ubuntu(start_comm)
    result = parse_file(res_path)

    OPENins = []

    # Create first instance and push it to open list
    starts_glob = result['starts']
    goals_glob = result['goals']
    enabled_glob = list(range(N))

    start_ins = Instance(starts_glob, goals_glob, enabled_glob, 0)
    OPENins.append(start_ins)

    # Dictionnary for the glabal solution and store first step
    global_solution = {}
    for i, line in enumerate(result['solution']) :
        global_solution[line[0]] = line[1]
    
    # Dictionary to store partitions per timestep
    partitions_per_timestep = defaultdict(list)

    # Helper variable
    last_split = []

    while len(OPENins) > 0 :

        ins = OPENins.pop()
    
        for partition in get_partitions(ins.enabled):
            local_solution = {}
            for enabled in partition :

                # Create a temporary scenario for the current partition
                create_temp_scenario(enabled, [ins.starts[i] for i in enabled], [ins.goals[i] for i in enabled], map_name)
                temp_command = "build/main -i assets/temp/temp_scenario.scen -m assets/maps/" + map_name + "/" + map_name + ".map -N "+ str(len(enabled)) + " -v 0 -s"

                # Solve the MAPF for the current partition
                run_command_in_ubuntu(temp_command)

                temp_result = parse_file(res_path)
                temp_solution = temp_result['solution']

                # Write temp solution to local_solution by taking care of agent id
                update_local_solution(temp_solution, local_solution, enabled, ins.enabled)
            
            # pas solution
            pad_local_solution(local_solution, len(ins.enabled), goals_glob)

            # Check if the local_solution solution is valid
            if is_valid_solution(local_solution):
                # print("Valid solution found for partition")

                ts = ins.time_start

                # Record the partitions used for the current timestep if they differ from the previous split
                if partition != last_split and len(partition[0]) != len(ins.enabled) :
                    if len(partitions_per_timestep[ts]) > 0 :
                        partitions_per_timestep[ts].append(partition)
                    else :
                        partitions_per_timestep[ts] = partition
                    last_split = partition
                    print(f"Instance is factorizable at timestep {ts}")

                # Push sub_instances to OPENins
                for enabled_agents in partition:
                    if len(enabled_agents) > 1 :
                        sub_instance = Instance(
                            global_solution[ts+1],
                            goals_glob,
                            enabled_agents,
                            ts+1
                        )
                        OPENins.append(sub_instance)

                
                break

    # Save partitions_per_timestep to a JSON file
    partitions_file_path = join(base_path, 'assets', 'temp', "FactDef_partitions.json")
    with open(partitions_file_path, 'w') as file:
        json.dump(partitions_per_timestep, file, indent=4)

    print("Partitions stored")

    return


def smallest_partitions(N: int):
    """
    Generate and save the smallest possible partitions for a given number of agents.
    Example: {{0}, {1}, {2}, {3}} for N = 3=4 agents.

    Args:
        N (int): The number of agents for which the partitions are to be generated.

    Returns: None
    """
    assets_path = up(up(__file__))     # LaCAM2_fact/assets/

    partitions_per_timestep = {}
    partitions_per_timestep[1] = []
    
    for i in range(N):
        partitions_per_timestep[1].append([i])

    # Save partitions_per_timestep to a JSON file
    partitions_file_path = join(assets_path, 'temp', "FactDef_partitions.json")
    with open(partitions_file_path, 'w') as file:
        json.dump(partitions_per_timestep, file, indent=4)



def half_smallest_partitions(N: int):
    """
    Generate and save the smallest possible partitions for a given number of agents by grouping agents in pairs.
    Example: {{0, 1}, {2, 3}} for N = 3=4 agents.

    Args:
        N (int): The number of agents for which the partitions are to be generated.

    Returns: None
    """
    assets_path = up(up(__file__))     # LaCAM2_fact/assets/

    partitions_per_timestep = {}
    partitions_per_timestep[1] = []
    
    if N%2 == 0 :
        for i in range(int(N/2)):
            partitions_per_timestep[1].append([i*2,i*2+1])
    else :
        for i in range(int((N-1)/2)):
            partitions_per_timestep[1].append([i*2,i*2+1])
        partitions_per_timestep[1].append([N-1])

    # Save partitions_per_timestep to a JSON file
    partitions_file_path = join(assets_path, 'temp', "FactDef_partitions.json")
    with open(partitions_file_path, 'w') as file:
        json.dump(partitions_per_timestep, file, indent=4)

