#include <emu_cxx_utils/replicated.h>
#include "graph.h"
#include "worklist.h"

class components
{
private:
    emu::repl<graph*> g_;
    worklist<graph::edge_type> worklist_;
	void init_components();
	void connect_components_remotemin();
	void connect_components_migrate();
	void check_changed();
	void update_prev_comp();
	void tree_climb();
public:
    // Component that this vertex belongs to
    emu::striped_array<long> component_;
	// Previous component to track changes
    emu::striped_array<long> prev_component_;
    // Size of each component
    emu::striped_array<long> component_size_;
    // Number of components
    emu::repl<long> num_components_;
    // Was there any change in components in the current iteration?
    emu::repl<long> changed_;

    explicit components(graph& g);
    components(const components& other, emu::shallow_copy);

    struct stats {
        // Number of components found in the graph
        long num_components;
        // Number of iterations required for convergence
        long num_iters;
    };
    stats run();
    void dump();
    void clear();
    bool check();
};
