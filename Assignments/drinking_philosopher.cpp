#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <vector>
#include <chrono>
#include <random>
#include <memory>
#include <atomic>
#include <iomanip>
#include <sstream>
#include <map>
#include <numeric>
#include <algorithm>
#include <cassert>

// run the simulation for 15 secs
#define SIMULATION 15

// Enumeration to represent the state of a philosopher
enum State { TRANQUIL, THIRSTY, DRINKING };

// Utility function to convert a State enum to its corresponding string
std::string stateToString(State state) {
    switch (state) {
        case TRANQUIL: return "TRANQUIL";
        case THIRSTY: return "THIRSTY";
        case DRINKING: return "DRINKING";
        default: return "UNKNOWN";
    }
}

/**
 * Graph class to represent the topology of philosophers and bottles
 * Implements an adjacency list representation where:
 * - Vertices represent philosophers
 * - Edges represent bottles shared between philosophers
 * - Each edge has a bottleId
 */
class Graph {
public:
    Graph(int number_fo_vertices) : adjacency_list(number_fo_vertices) {
    }

    // Connect
    void addEdge(int vectex_1, int vertex_2, int bottle_id) {
        adjacency_list[vectex_1][vertex_2] = bottle_id;
        adjacency_list[vertex_2][vectex_1] = bottle_id;
    }

    // Get all bottles adjacent to a philosopher
    std::vector<int> getAdjacentBottles(int philosopher_id) {
        std::vector<int> bottles;
        for (const auto &[neighbor, bottle_id]: adjacency_list[philosopher_id]) {
            bottles.push_back(bottle_id);
        }
        return bottles;
    }

private:
    std::vector<std::map<int, int> > adjacency_list; // [philosopher][neighbor] = bottle_id
};

// Class to manage bottle resources shared among philosophers
class Bottles {
public:
    // Constructor initializes the bottles, -1 indicates the bottle is free
    Bottles(const int number_of_bottles) : bottles(number_of_bottles, -1) {
    }

    /**
      * Attempts to acquire the specified bottles for a philosopher.
      * @param philosopher_id The ID of the philosopher attempting to acquire bottles.
      * @param required_bottles A vector of bottle indices the philosopher requires.
      * @return True if all bottles are successfully acquired, False otherwise.
      */
    bool acquireBottles(int philosopher_id, const std::vector<int> &required_bottles) {
        std::unique_lock lock(mtx);
        // Check if all needed bottles are available
        for (int bottle: required_bottles) {
            if (bottles[bottle] != -1 && bottles[bottle] != philosopher_id) {
                return false;
            }
        }
        // Acquire all needed bottles
        for (int bottle: required_bottles) {
            bottles[bottle] = philosopher_id;
        }
        logBottleState();
        return true;
    }

    /**
      * Releases all bottles held by the specified philosopher.
      * @param philosopher_id The ID of the philosopher releasing bottles.
      */
    void releaseBottles(int philosopher_id) {
        std::unique_lock lock(mtx);
        for (int i = 0; i < bottles.size(); ++i) {
            if (bottles[i] == philosopher_id) {
                bottles[i] = -1;
            }
        }
        logBottleState();
    }

    // Log the current state of all bottles
    void logBottleState() {
        std::stringstream ss;
        ss << "Bottles: [";
        for (int i = 0; i < bottles.size(); ++i) {
            if (i > 0) ss << ", ";
            if (bottles[i] == -1) {
                ss << "Free";
            } else {
                ss << "P" << bottles[i];
            }
        }
        ss << "]";
        std::cout << ss.str() << std::endl;
        fflush(stdout);
    }

private:
    std::vector<int> bottles;
    std::mutex mtx;
};

// Class responsible for logging the state transitions of philosophers
class StateLogger {
public:
    /**
      * Logs the current state and action of a philosopher.
      * @param philosopher_id The ID of the philosopher.
      * @param state The current state of the philosopher.
      * @param action A description of the action being performed.
      */
    static void log(int philosopher_id, State state, const std::string &action) {
        std::unique_lock lock(mtx);
        const auto now = std::chrono::system_clock::now();
        const auto now_time = std::chrono::system_clock::to_time_t(now);
        std::cout << std::put_time(std::localtime(&now_time), "%H:%M:%S")
                << " [P" << philosopher_id << "] "
                << std::setw(8) << stateToString(state) << " | "
                << action << std::endl;
    }

private:
    // Mutex to protect logging
    static std::mutex mtx;
};

// Define static mutex for StateLogger
std::mutex StateLogger::mtx;

// Class representing a philosopher in the simulation
class Philosopher {
public:
    /**
      * Constructor to initialize a philosopher.
      * @param id The unique ID of the philosopher.
      * @param bottle A shared pointer to Bottles.
      * @param graph A shared pointer to Graph
      */
    Philosopher(int id, std::shared_ptr<Bottles> bottle, std::shared_ptr<Graph> graph)
        : id(id), state(TRANQUIL), bottles(std::move(bottle)), graph(std::move(graph)) {
    }

    /**
      * The main loop for the philosopher's behavior.
      * The philosopher alternates between thinking, becoming thirsty, acquiring bottles, drinking, and releasing bottles.
      */
    void run() {
        while (true) {
            think();
            becomeThirsty();
            while (!requestBottles()) {
                // Wait for a random short duration before retrying to acquire bottles
                std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100));
            }
            drink();
            releaseBottles();
        }
    }

    State getState() {
        return state;
    }

    void publicBecomeThirsty() {
        becomeThirsty();
    }

private:
    int id; // Unique id for the philosopher
    State state; // Current state of the philosopher
    std::vector<int> required_bottles;
    std::shared_ptr<Bottles> bottles;
    std::shared_ptr<Graph> graph;
    std::mutex mtx; // Mutex to protect philosopher's state

    // Simulate thinking
    void think() {
        state = TRANQUIL;
        StateLogger::log(id, state, "Started thinking");
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1000 + 500));
        StateLogger::log(id, state, "Finished thinking");
    }

    /**
     * Transitions the philosopher to the THIRSTY state and determines the bottles needed.
     */
    void becomeThirsty() {
        std::unique_lock lock(mtx);
        state = THIRSTY;

        std::random_device rd;
        std::mt19937 generate(rd());

        // Get available bottles from the graph
        std::vector<int> adjacent_bottles = graph->getAdjacentBottles(id);

        // Randomly select 1 or 2 bottles to simulate `philosopher may need different subsets of bottles
        int limit = (rand() % 2) + 1;
        required_bottles.clear();

        // Random selection of bottles
        std::vector<int> indices(adjacent_bottles.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::ranges::shuffle(indices, generate);

        for (int i = 0; i < limit && i < adjacent_bottles.size(); ++i) {
            required_bottles.push_back(adjacent_bottles[indices[i]]);
        }

        std::stringstream ss;
        ss << "Needs bottles: ";
        for (size_t i = 0; i < required_bottles.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << required_bottles[i];
        }
        StateLogger::log(id, state, ss.str());
    }

    /**
     * Attempts to acquire the necessary bottles from the Bottles manager.
     * @return True if bottles are successfully acquired, False otherwise.
     */
    bool requestBottles() {
        if (bottles->acquireBottles(id, required_bottles)) {
            state = DRINKING;
            StateLogger::log(id, state, "Acquired bottles");
            return true;
        }
        return false;
    }

    // Simulate drinking
    void drink() {
        StateLogger::log(id, state, "Started drinking");
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1000 + 500));
        StateLogger::log(id, state, "Finished drinking");
    }

    void releaseBottles() {
        bottles->releaseBottles(id);
        state = TRANQUIL;
        StateLogger::log(id, state, "Released bottles");
    }
};

void alphaTests() {
    std::cout << "\n================ ALPHA TESTS ================\n";

    // Test 1: Graph Construction and Edge Addition
    {
        Graph g(5);
        g.addEdge(0, 1, 0);
        g.addEdge(1, 2, 1);

        auto bottles = g.getAdjacentBottles(1);
        assert(bottles.size() == 2);
        assert(std::find(bottles.begin(), bottles.end(), 0) != bottles.end());
        assert(std::find(bottles.begin(), bottles.end(), 1) != bottles.end());
        std::cout << "Graph construction test passed\n";
    }

    // Test 2: Bottle Management
    {
        Bottles bottles(3);
        std::vector<int> req_bottles = {0, 1};

        // Test acquisition
        assert(bottles.acquireBottles(0, req_bottles) == true);

        // Test concurrent acquisition
        std::vector<int> other_bottles = {1, 2};
        assert(bottles.acquireBottles(1, other_bottles) == false);

        // Test release
        bottles.releaseBottles(0);
        assert(bottles.acquireBottles(1, other_bottles) == true);
        std::cout << "Bottle management test passed\n";

    }

    // Test 3: State Transitions
    {
        auto bottles = std::make_shared<Bottles>(3);
        auto graph = std::make_shared<Graph>(3);
        Philosopher philosopher(0, bottles, graph);

        // Test initial state
        assert(philosopher.getState() == TRANQUIL);

        philosopher.publicBecomeThirsty();
        assert(philosopher.getState() == THIRSTY);
        std::cout << "Philosopher state transition test passed\n";
    }

}

int main() {
    srand(time(0));
    constexpr int number_of_philosophers = 5;
    constexpr int number_of_bottles = 6;

    alphaTests();

    std::cout << "\n================ BETA TESTS ================\n";

    // Create and initialize the graph
    auto graph = std::make_shared<Graph>(number_of_philosophers);

    graph->addEdge(0, 1, 0);
    graph->addEdge(1, 2, 1);
    graph->addEdge(2, 3, 2);
    graph->addEdge(3, 4, 3);
    graph->addEdge(4, 0, 4);
    graph->addEdge(0, 2, 5);

    auto bottles = std::make_shared<Bottles>(number_of_bottles);
    std::vector<std::thread> threads;
    std::vector<std::unique_ptr<Philosopher> > philosophers;

    std::cout << "=== Simulating Drinking Philosophers for " << SIMULATION << " secs ===" << std::endl;
    std::cout << "Time\t[Phil]\tState\t|\tAction" << std::endl;
    std::cout << "-------------------------------------------" << std::endl;

    // Initialize philosophers
    philosophers.reserve(number_of_philosophers);
    for (int idx = 0; idx < number_of_philosophers; ++idx) {
        philosophers.emplace_back(std::make_unique<Philosopher>(idx, bottles, graph));
    }

    // Start philosopher threads
    for (int idx = 0; idx < number_of_philosophers; ++idx) {
        threads.emplace_back(&Philosopher::run, philosophers[idx].get());
    }

    // Run simulation for specified duration
    std::this_thread::sleep_for(std::chrono::seconds(SIMULATION));

    // Kill
    std::quick_exit(0);
    return 0;
}

/*

Output logs


================ ALPHA TESTS ================
Graph construction test passed
Bottles: [P0, P0, Free]
Bottles: [Free, Free, Free]
Bottles: [Free, P1, P1]
Bottle management test passed
20:47:33 [P0]  THIRSTY | Needs bottles:
Philosopher state transition test passed

================ BETA TESTS ================
=== Simulating Drinking Philosophers for 15 secs ===
Time	[Phil]	State	|	Action
-------------------------------------------
20:47:33 [P0] TRANQUIL | Started thinking
20:47:33 [P4] TRANQUIL | Started thinking
20:47:33 [P3] TRANQUIL | Started thinking
20:47:33 [P2] TRANQUIL | Started thinking
20:47:33 [P1] TRANQUIL | Started thinking
20:47:34 [P2] TRANQUIL | Finished thinking
20:47:34 [P2]  THIRSTY | Needs bottles: 2, 1
Bottles: [Free, P2, P2, Free, Free, Free]
20:47:34 [P2] DRINKING | Acquired bottles
20:47:34 [P2] DRINKING | Started drinking
20:47:34 [P4] TRANQUIL | Finished thinking
20:47:34 [P4]  THIRSTY | Needs bottles: 4, 3
Bottles: [Free, P2, P2, P4, P4, Free]
20:47:34 [P4] DRINKING | Acquired bottles
20:47:34 [P4] DRINKING | Started drinking
20:47:34 [P3] TRANQUIL | Finished thinking
20:47:34 [P3]  THIRSTY | Needs bottles: 3
20:47:35 [P0] TRANQUIL | Finished thinking
20:47:35 [P0]  THIRSTY | Needs bottles: 5, 4
20:47:35 [P4] DRINKING | Finished drinking
Bottles: [Free, P2, P2, Free, Free, Free]
20:47:35 [P4] TRANQUIL | Released bottles
20:47:35 [P4] TRANQUIL | Started thinking
Bottles: [Free, P2, P2, P3, Free, Free]
20:47:35 [P3] DRINKING | Acquired bottles
20:47:35 [P3] DRINKING | Started drinking
Bottles: [Free, P2, P2, P3, P0, P0]
20:47:35 [P0] DRINKING | Acquired bottles
20:47:35 [P0] DRINKING | Started drinking
20:47:35 [P1] TRANQUIL | Finished thinking
20:47:35 [P1]  THIRSTY | Needs bottles: 1, 0
20:47:35 [P3] DRINKING | Finished drinking
Bottles: [Free, P2, P2, Free, P0, P0]
20:47:35 [P3] TRANQUIL | Released bottles
20:47:35 [P3] TRANQUIL | Started thinking
20:47:35 [P2] DRINKING | Finished drinking
Bottles: [Free, Free, Free, Free, P0, P0]
20:47:35 [P2] TRANQUIL | Released bottles
20:47:35 [P2] TRANQUIL | Started thinking
Bottles: [P1, P1, Free, Free, P0, P0]
20:47:35 [P1] DRINKING | Acquired bottles
20:47:35 [P1] DRINKING | Started drinking
20:47:36 [P1] DRINKING | Finished drinking
Bottles: [Free, Free, Free, Free, P0, P0]
20:47:36 [P1] TRANQUIL | Released bottles
20:47:36 [P1] TRANQUIL | Started thinking
20:47:36 [P0] DRINKING | Finished drinking
Bottles: [Free, Free, Free, Free, Free, Free]
20:47:36 [P0] TRANQUIL | Released bottles
20:47:36 [P0] TRANQUIL | Started thinking
20:47:36 [P4] TRANQUIL | Finished thinking
20:47:36 [P4]  THIRSTY | Needs bottles: 4
Bottles: [Free, Free, Free, Free, P4, Free]
20:47:36 [P4] DRINKING | Acquired bottles
20:47:36 [P4] DRINKING | Started drinking
20:47:36 [P1] TRANQUIL | Finished thinking
20:47:36 [P1]  THIRSTY | Needs bottles: 0, 1
Bottles: [P1, P1, Free, Free, P4, Free]
20:47:36 [P1] DRINKING | Acquired bottles
20:47:36 [P1] DRINKING | Started drinking
20:47:36 [P3] TRANQUIL | Finished thinking
20:47:36 [P3]  THIRSTY | Needs bottles: 3
Bottles: [P1, P1, Free, P3, P4, Free]
20:47:36 [P3] DRINKING | Acquired bottles
20:47:36 [P3] DRINKING | Started drinking
20:47:36 [P2] TRANQUIL | Finished thinking
20:47:36 [P2]  THIRSTY | Needs bottles: 1
20:47:37 [P4] DRINKING | Finished drinking
Bottles: [P1, P1, Free, P3, Free, Free]
20:47:37 [P4] TRANQUIL | Released bottles
20:47:37 [P4] TRANQUIL | Started thinking
20:47:37 [P3] DRINKING | Finished drinking
Bottles: [P1, P1, Free, Free, Free, Free]
20:47:37 [P3] TRANQUIL | Released bottles
20:47:37 [P3] TRANQUIL | Started thinking
20:47:37 [P0] TRANQUIL | Finished thinking
20:47:37 [P0]  THIRSTY | Needs bottles: 0, 5
20:47:38 [P1] DRINKING | Finished drinking
Bottles: [Free, Free, Free, Free, Free, Free]
20:47:38 [P1] TRANQUIL | Released bottles
20:47:38 [P1] TRANQUIL | Started thinking
Bottles: [Free, P2, Free, Free, Free, Free]
20:47:38 [P2] DRINKING | Acquired bottles
20:47:38 [P2] DRINKING | Started drinking
Bottles: [P0, P2, Free, Free, Free, P0]
20:47:38 [P0] DRINKING | Acquired bottles
20:47:38 [P0] DRINKING | Started drinking
20:47:38 [P2] DRINKING | Finished drinking
Bottles: [P0, Free, Free, Free, Free, P0]
20:47:38 [P2] TRANQUIL | Released bottles
20:47:38 [P2] TRANQUIL | Started thinking
20:47:38 [P4] TRANQUIL | Finished thinking
20:47:38 [P4]  THIRSTY | Needs bottles: 3
Bottles: [P0, Free, Free, P4, Free, P0]
20:47:38 [P4] DRINKING | Acquired bottles
20:47:38 [P4] DRINKING | Started drinking
20:47:39 [P3] TRANQUIL | Finished thinking
20:47:39 [P3]  THIRSTY | Needs bottles: 2
Bottles: [P0, Free, P3, P4, Free, P0]
20:47:39 [P3] DRINKING | Acquired bottles
20:47:39 [P3] DRINKING | Started drinking
20:47:39 [P1] TRANQUIL | Finished thinking
20:47:39 [P1]  THIRSTY | Needs bottles: 1, 0
20:47:39 [P0] DRINKING | Finished drinking
Bottles: [Free, Free, P3, P4, Free, Free]
20:47:39 [P0] TRANQUIL | Released bottles
20:47:39 [P0] TRANQUIL | Started thinking
Bottles: [P1, P1, P3, P4, Free, Free]
20:47:39 [P1] DRINKING | Acquired bottles
20:47:39 [P1] DRINKING | Started drinking
20:47:39 [P4] DRINKING | Finished drinking
Bottles: [P1, P1, P3, Free, Free, Free]
20:47:39 [P4] TRANQUIL | Released bottles
20:47:39 [P4] TRANQUIL | Started thinking
20:47:39 [P2] TRANQUIL | Finished thinking
20:47:39 [P2]  THIRSTY | Needs bottles: 1
20:47:40 [P1] DRINKING | Finished drinking
Bottles: [Free, Free, P3, Free, Free, Free]
20:47:40 [P1] TRANQUIL | Released bottles
20:47:40 [P1] TRANQUIL | Started thinking
Bottles: [Free, P2, P3, Free, Free, Free]
20:47:40 [P2] DRINKING | Acquired bottles
20:47:40 [P2] DRINKING | Started drinking
20:47:40 [P3] DRINKING | Finished drinking
Bottles: [Free, P2, Free, Free, Free, Free]
20:47:40 [P3] TRANQUIL | Released bottles
20:47:40 [P3] TRANQUIL | Started thinking
20:47:40 [P0] TRANQUIL | Finished thinking
20:47:40 [P0]  THIRSTY | Needs bottles: 5, 4
Bottles: [Free, P2, Free, Free, P0, P0]
20:47:40 [P0] DRINKING | Acquired bottles
20:47:40 [P0] DRINKING | Started drinking
20:47:40 [P4] TRANQUIL | Finished thinking
20:47:40 [P4]  THIRSTY | Needs bottles: 3, 4
20:47:41 [P2] DRINKING | Finished drinking
Bottles: [Free, Free, Free, Free, P0, P0]
20:47:41 [P2] TRANQUIL | Released bottles
20:47:41 [P2] TRANQUIL | Started thinking
20:47:41 [P3] TRANQUIL | Finished thinking
20:47:41 [P3]  THIRSTY | Needs bottles: 3, 2
Bottles: [Free, Free, P3, P3, P0, P0]
20:47:41 [P3] DRINKING | Acquired bottles
20:47:41 [P3] DRINKING | Started drinking
20:47:41 [P1] TRANQUIL | Finished thinking
20:47:41 [P1]  THIRSTY | Needs bottles: 0, 1
Bottles: [P1, P1, P3, P3, P0, P0]
20:47:41 [P1] DRINKING | Acquired bottles
20:47:41 [P1] DRINKING | Started drinking
20:47:42 [P2] TRANQUIL | Finished thinking
20:47:42 [P2]  THIRSTY | Needs bottles: 2, 5
20:47:42 [P0] DRINKING | Finished drinking
Bottles: [P1, P1, P3, P3, Free, Free]
20:47:42 [P0] TRANQUIL | Released bottles
20:47:42 [P0] TRANQUIL | Started thinking
20:47:42 [P3] DRINKING | Finished drinking
Bottles: [P1, P1, Free, Free, Free, Free]
20:47:42 [P3] TRANQUIL | Released bottles
20:47:42 [P3] TRANQUIL | Started thinking
Bottles: [P1, P1, Free, P4, P4, Free]
20:47:42 [P4] DRINKING | Acquired bottles
20:47:42 [P4] DRINKING | Started drinking
Bottles: [P1, P1, P2, P4, P4, P2]
20:47:42 [P2] DRINKING | Acquired bottles
20:47:42 [P2] DRINKING | Started drinking
20:47:42 [P1] DRINKING | Finished drinking
Bottles: [Free, Free, P2, P4, P4, P2]
20:47:42 [P1] TRANQUIL | Released bottles
20:47:42 [P1] TRANQUIL | Started thinking
20:47:42 [P2] DRINKING | Finished drinking
Bottles: [Free, Free, Free, P4, P4, Free]
20:47:42 [P2] TRANQUIL | Released bottles
20:47:42 [P2] TRANQUIL | Started thinking
20:47:42 [P3] TRANQUIL | Finished thinking
20:47:42 [P3]  THIRSTY | Needs bottles: 3, 2
20:47:43 [P1] TRANQUIL | Finished thinking
20:47:43 [P1]  THIRSTY | Needs bottles: 1
Bottles: [Free, P1, Free, P4, P4, Free]
20:47:43 [P1] DRINKING | Acquired bottles
20:47:43 [P1] DRINKING | Started drinking
20:47:43 [P4] DRINKING | Finished drinking
Bottles: [Free, P1, Free, Free, Free, Free]
20:47:43 [P4] TRANQUIL | Released bottles
20:47:43 [P4] TRANQUIL | Started thinking
20:47:43 [P0] TRANQUIL | Finished thinking
20:47:43 [P0]  THIRSTY | Needs bottles: 5, 0
Bottles: [P0, P1, Free, Free, Free, P0]
20:47:43 [P0] DRINKING | Acquired bottles
20:47:43 [P0] DRINKING | Started drinking
Bottles: [P0, P1, P3, P3, Free, P0]
20:47:43 [P3] DRINKING | Acquired bottles
20:47:43 [P3] DRINKING | Started drinking
20:47:43 [P0] DRINKING | Finished drinking
Bottles: [Free, P1, P3, P3, Free, Free]
20:47:43 [P0] TRANQUIL | Released bottles
20:47:43 [P0] TRANQUIL | Started thinking
20:47:43 [P4] TRANQUIL | Finished thinking
20:47:43 [P4]  THIRSTY | Needs bottles: 3
20:47:43 [P2] TRANQUIL | Finished thinking
20:47:43 [P2]  THIRSTY | Needs bottles: 1
20:47:44 [P3] DRINKING | Finished drinking
Bottles: [Free, P1, Free, Free, Free, Free]
20:47:44 [P3] TRANQUIL | Released bottles
20:47:44 [P3] TRANQUIL | Started thinking
Bottles: [Free, P1, Free, P4, Free, Free]
20:47:44 [P4] DRINKING | Acquired bottles
20:47:44 [P4] DRINKING | Started drinking
20:47:44 [P1] DRINKING | Finished drinking
Bottles: [Free, Free, Free, P4, Free, Free]
20:47:44 [P1] TRANQUIL | Released bottles
20:47:44 [P1] TRANQUIL | Started thinking
Bottles: [Free, P2, Free, P4, Free, Free]
20:47:44 [P2] DRINKING | Acquired bottles
20:47:44 [P2] DRINKING | Started drinking
20:47:44 [P3] TRANQUIL | Finished thinking
20:47:44 [P3]  THIRSTY | Needs bottles: 3, 2
20:47:45 [P2] DRINKING | Finished drinking
Bottles: [Free, Free, Free, P4, Free, Free]
20:47:45 [P2] TRANQUIL | Released bottles
20:47:45 [P2] TRANQUIL | Started thinking
20:47:45 [P0] TRANQUIL | Finished thinking
20:47:45 [P0]  THIRSTY | Needs bottles: 4, 0
Bottles: [P0, Free, Free, P4, P0, Free]
20:47:45 [P0] DRINKING | Acquired bottles
20:47:45 [P0] DRINKING | Started drinking
20:47:45 [P4] DRINKING | Finished drinking
Bottles: [P0, Free, Free, Free, P0, Free]
20:47:45 [P4] TRANQUIL | Released bottles
20:47:45 [P4] TRANQUIL | Started thinking
Bottles: [P0, Free, P3, P3, P0, Free]
20:47:45 [P3] DRINKING | Acquired bottles
20:47:45 [P3] DRINKING | Started drinking
20:47:45 [P1] TRANQUIL | Finished thinking
20:47:45 [P1]  THIRSTY | Needs bottles: 1, 0
20:47:46 [P2] TRANQUIL | Finished thinking
20:47:46 [P2]  THIRSTY | Needs bottles: 1, 2
20:47:46 [P0] DRINKING | Finished drinking
Bottles: [Free, Free, P3, P3, Free, Free]
20:47:46 [P0] TRANQUIL | Released bottles
20:47:46 [P0] TRANQUIL | Started thinking
Bottles: [P1, P1, P3, P3, Free, Free]
20:47:46 [P1] DRINKING | Acquired bottles
20:47:46 [P1] DRINKING | Started drinking
20:47:46 [P4] TRANQUIL | Finished thinking
20:47:46 [P4]  THIRSTY | Needs bottles: 3
20:47:46 [P3] DRINKING | Finished drinking
Bottles: [P1, P1, Free, Free, Free, Free]
20:47:46 [P3] TRANQUIL | Released bottles
20:47:46 [P3] TRANQUIL | Started thinking
Bottles: [P1, P1, Free, P4, Free, Free]
20:47:46 [P4] DRINKING | Acquired bottles
20:47:46 [P4] DRINKING | Started drinking
20:47:47 [P0] TRANQUIL | Finished thinking
20:47:47 [P0]  THIRSTY | Needs bottles: 0, 4
20:47:47 [P1] DRINKING | Finished drinking
Bottles: [Free, Free, Free, P4, Free, Free]
20:47:47 [P1] TRANQUIL | Released bottles
20:47:47 [P1] TRANQUIL | Started thinking
Bottles: [Free, P2, P2, P4, Free, Free]
20:47:47 [P2] DRINKING | Acquired bottles
20:47:47 [P2] DRINKING | Started drinking
Bottles: [P0, P2, P2, P4, P0, Free]
20:47:47 [P0] DRINKING | Acquired bottles
20:47:47 [P0] DRINKING | Started drinking
20:47:47 [P3] TRANQUIL | Finished thinking
20:47:47 [P3]  THIRSTY | Needs bottles: 3
20:47:47 [P4] DRINKING | Finished drinking
Bottles: [P0, P2, P2, Free, P0, Free]
20:47:47 [P4] TRANQUIL | Released bottles
20:47:47 [P4] TRANQUIL | Started thinking
Bottles: [P0, P2, P2, P3, P0, Free]
20:47:47 [P3] DRINKING | Acquired bottles
20:47:47 [P3] DRINKING | Started drinking
20:47:48 [P2] DRINKING | Finished drinking
Bottles: [P0, Free, Free, P3, P0, Free]
20:47:48 [P2] TRANQUIL | Released bottles
20:47:48 [P2] TRANQUIL | Started thinking
20:47:48 [P4] TRANQUIL | Finished thinking
20:47:48 [P4]  THIRSTY | Needs bottles: 3

Process finished with exit code 0


*/
