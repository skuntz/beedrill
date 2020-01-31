#include <string.h>
#include <getopt.h>
#include <limits.h>

#include "graph.h"
#include "dist_edge_list.h"
#include "components.h"

const struct option long_options[] = {
    {"graph_filename"   , required_argument},
    {"distributed_load" , no_argument},
    {"num_trials"       , required_argument},
    {"dump_edge_list"   , no_argument},
    {"check_graph"      , no_argument},
    {"dump_graph"       , no_argument},
    {"check_results"    , no_argument},
    {"help"             , no_argument},
    {nullptr}
};

void
print_help(const char* argv0)
{
    LOG( "Usage: %s [OPTIONS]\n", argv0);
    LOG("\t--graph_filename     Path to graph file to load\n");
    LOG("\t--distributed_load   Load the graph from all nodes at once (File must exist on all nodes, use absolute path).\n");
    LOG("\t--num_trials         Run BFS this many times.\n");
    LOG("\t--dump_edge_list     Print the edge list to stdout after loading (slow)\n");
    LOG("\t--check_graph        Validate the constructed graph against the edge list (slow)\n");
    LOG("\t--dump_graph         Print the graph to stdout after construction (slow)\n");
    LOG("\t--check_results      Validate the results (slow)\n");
    LOG("\t--help               Print command line help\n");
}

struct components_args
{
    const char* graph_filename;
    bool distributed_load;
    long num_trials;
    bool dump_edge_list;
    bool check_graph;
    bool dump_graph;
    bool check_results;

    static components_args
    parse(int argc, char *argv[])
    {
        components_args args = {};
        args.graph_filename = NULL;
        args.distributed_load = false;
        args.num_trials = 1;
        args.dump_edge_list = false;
        args.check_graph = false;
        args.dump_graph = false;
        args.check_results = false;

        int option_index;
        while (true) {
            int c = getopt_long(argc, argv, "", long_options, &option_index);
            // Done parsing
            if (c == -1) { break; }
            // Parse error
            if (c == '?') {
                LOG( "Invalid arguments\n");
                print_help(argv[0]);
                exit(1);
            }
            const char* option_name = long_options[option_index].name;

            if (!strcmp(option_name, "graph_filename")) {
                args.graph_filename = optarg;
            } else if (!strcmp(option_name, "distributed_load")) {
                args.distributed_load = true;
            } else if (!strcmp(option_name, "num_trials")) {
                args.num_trials = atol(optarg);
            } else if (!strcmp(option_name, "dump_edge_list")) {
                args.dump_edge_list = true;
            } else if (!strcmp(option_name, "check_graph")) {
                args.check_graph = true;
            } else if (!strcmp(option_name, "dump_graph")) {
                args.dump_graph = true;
            } else if (!strcmp(option_name, "check_results")) {
                args.check_results = true;
            } else if (!strcmp(option_name, "help")) {
                print_help(argv[0]);
                exit(1);
            }
        }
        if (args.graph_filename == NULL) { LOG( "Missing graph filename\n"); exit(1); }
        if (args.num_trials <= 0) { LOG( "num_trials must be > 0\n"); exit(1); }
        return args;
    }
};


int main(int argc, char ** argv)
{
    bool success = true;
    // Set active region for hooks
    const char* active_region = getenv("HOOKS_ACTIVE_REGION");
    if (active_region != NULL) {
        hooks_set_active_region(active_region);
    } else {
        hooks_set_active_region("components");
    }

    // Parse command-line arguments
    components_args args = components_args::parse(argc, argv);

    // Load edge list from file
    auto dist_el = dist_edge_list::load(args.graph_filename);
    if (args.dump_edge_list) {
        LOG("Dumping edge list...\n");
        dist_el->dump();
    }

    // Build the graph
    LOG("Constructing graph...\n");
    auto g = graph::from_edge_list(*dist_el);

    g->print_distribution();
    if (args.check_graph) {
        LOG("Checking graph...");
        if (g->check(*dist_el)) {
            LOG("PASS\n");
        } else {
            LOG("FAIL\n");
            success = false;
        };
    }
    if (args.dump_graph) {
        LOG("Dumping graph...\n");
        g->dump();
    }

    // Initialize the algorithm
    LOG("Initializing data structures...\n");
    auto cc = emu::make_repl_copy<components>(*g);

    // Run multiple trials of the algorithm
    for (long s = 0; s < args.num_trials; ++s) {
        // Clear out the data structures
        cc->clear();

        LOG("Finding connected components...\n");
        hooks_region_begin("components");
        cc->run();
        double time_ms = hooks_region_end();

        LOG("Found components in %3.2f ms \n", time_ms);
    }

    if (args.check_results) {
        LOG("Checking results...\n");
        if (cc->check()) {
            LOG("PASS\n");
        } else {
            LOG("FAIL\n");
            success = false;
        }
    }

    return !success;
}