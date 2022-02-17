#include "components.h"
#include <cassert>
#include <cilk/cilk.h>
#include <emu_c_utils/emu_c_utils.h>
#include <queue>
#include <unordered_map>
#include <emu_cxx_utils/fill.h>

using namespace emu;
using namespace emu::parallel;
#define EDGE_GRAIN 10


components::components(graph & g)
: g_(&g)
, worklist_(g.num_vertices())
, component_(g.num_vertices())
, component_size_(g.num_vertices())
, num_components_(g.num_vertices())
, changed_(false)
{
    clear();
}

components::components(const components& other, emu::shallow_copy shallow)
: g_(other.g_)
, worklist_(other.worklist_, shallow)
, component_(other.component_, shallow)
, component_size_(other.component_size_, shallow)
, num_components_(other.num_components_)
, changed_(other.changed_)
{}

void
components::clear()
{
}

void
components::init_components() {
	
	for_each(fixed, g_->vertices_begin(), g_->vertices_end(), [this] (long v){
		// Put each vertex in its own component
		component_[v] = v;
		// Set size of each component to zero
		component_size_[v] = 0;
       
	});
}

void
components::connect_components_remotemin() {

    for_each(fixed, g_->vertices_begin(), g_->vertices_end(), [this] (long src) {
		// long mytid = (long) ATOMIC_ADDMS(&vtid, 1);
		// vstart[mytid] = CLOCK();
		//ENABLE_WEAK_ORDERING();
		long nedges = g_->out_degree(src);		
		if (nedges > 0 && nedges < EDGE_GRAIN) {
			for (auto e = g_->out_edges_begin(src); e < g_-> out_edges_end(src); e++) {
				remote_min(&component_[e->dst], component_[src]);
			}
		} else if (nedges > 0 && nedges >= EDGE_GRAIN) {
			// simulator says this is better, but no diff on HW
			g_->for_each_out_edge(src, [&](long dst) {
					
				remote_min(&component_[dst], component_[src]);
			});
		}
		//DISABLE_WEAK_ORDERING();
		//vend[mytid] = CLOCK();
		});
}

void
components::tree_climb() {

	g_->for_each_vertex(fixed, [this](long v) {
	    	
		// Merge connected components
        while (component_[v] != component_[component_[v]]) {
            component_[v] = component_[component_[v]];
		}  
    });
	
}

void
components::connect_components_migrate() {
#if 0
    // skk dyn policy no improvement
	for_each(fixed, g_->vertices_begin(), g_->vertices_end(), [this] (long src) {
			if (g_->out_degree(src)) {
				// simulator says pulling out comp_src is better, but no diff on HW
				g_->for_each_out_edge(src, [&](long dst) {
						long &comp_src = component_[src];
						long &comp_dst = component_[dst];
						if (comp_dst < comp_src) {
							comp_src = comp_dst;
							changed_ = true;
						} else if (comp_src < comp_dst) {
							comp_dst = comp_src;
							changed_ = true;
						}
					});
			}
       	});
#endif
#if 1
	//lu_profile_perfcntr(PFC_CLEAR, (char*)"CLEARING before dyn loop");
    //lu_profile_perfcntr(PFC_START, (char*)"STARTING before dyn loop");

	//skk error c++2a extension
	// for_each(fixed, g_->vertices_begin(), g_->vertices_end(), [=, *this] (long src) {
	for_each(fixed, g_->vertices_begin(), g_->vertices_end(), [this] (long src) {
	    long nedges = g_->out_degree(src);
		if (nedges > 0 && nedges < EDGE_GRAIN) {
			for (auto e = g_->out_edges_begin(src); e < g_-> out_edges_end(src); e++) {
			   
				long &comp_src = component_[src];
				long &comp_dst = component_[e->dst];
				if (comp_dst < comp_src) {
					comp_src = comp_dst;
					changed_ = true;
				} else if (comp_src < comp_dst) {
					comp_dst = comp_src;
					changed_ = true;
				}	
			}
		
		} else if (nedges > 0 && nedges >= EDGE_GRAIN) {
			
		// simulator says this is better, but no diff on HW
			g_->for_each_out_edge(src, [&](long dst) {
				long &comp_src = component_[src];
				long &comp_dst = component_[dst];
				if (comp_dst < comp_src) {
					comp_src = comp_dst;
					changed_ = true;
				} else if (comp_src < comp_dst) {
					comp_dst = comp_src;
					changed_ = true;
				}
			 });
	    }
    });

	//lu_profile_perfcntr(PFC_STOP, (char*)"STOPPING after dyn loop");

#endif
}


components::stats
components::run()
{
    volatile  uint64_t start, ticks;

	
	//-- Initialize components
    start = CLOCK();
    this->init_components();
    ticks = CLOCK() - start;
    //skk LOG("Init components: %lu clock ticks\n", ticks);
    
    long num_iters = 1;


#if 0
	// -- Try initial pass with remote_min, then move to migrating pass
	start = CLOCK();
	this->connect_components_remotemin();
	volatile uint64_t end = CLOCK();
	ticks = end - start;
	LOG("\nConnect components (iter %ld remote_min): %lu clock ticks (%ld %ld)\n",
		num_iters, ticks, start, end);

	start = CLOCK();
	this->tree_climb();
	ticks = CLOCK() - start;
    //skk LOG("Tree climb: %lu clock ticks\n", ticks);
	num_iters++;
	
#endif
	
	// Migrating components passes
    for (; ; ++num_iters) {
	
		changed_ = false;

		// For all edges that connect vertices in different components...
		// SKK If we only have one directional edges, need to assign both ways
			//starttiming();
		start = CLOCK();
		this->connect_components_migrate();
		ticks = CLOCK() - start;
		LOG("\nConnect components (iter %lu): %lu clock ticks\n", num_iters, ticks);

        // No changes? We're done!
		start = CLOCK();
        if (!repl_reduce(changed_, std::logical_or<>())) break;
		ticks = CLOCK() - start;
		//skk LOG("  Reduce changed: %lu clock ticks\n", ticks);
	
		start = CLOCK();
		this->tree_climb();
		ticks = CLOCK() - start;
		//skk LOG("  Update components: %lu clock ticks\n", ticks);

    }

    
    // Count up the size of each component
    start = CLOCK();
    for_each(fixed, component_.begin(), component_.end(),
        [this](long c) { emu::remote_add(&component_size_[c], 1); }
    );
    ticks = CLOCK() - start;
    //skk LOG("Count component size: %lu clock ticks\n", ticks);

    stats s;
    s.num_iters = num_iters;

    // TODO should use parallel count_if here (can use transform_reduce)
    // s.num_components = parallel::count_if(
    //     component_size_.begin(), component_size_.end(),
    //     [](long size) { return size > 0; }
    // );

    // Count number of components
    start = CLOCK();
    num_components_ = 0;
    for_each(component_size_.begin(), component_size_.end(),
        [&](long size) {
            if (size > 0) { emu::remote_add(&num_components_, 1); }
        }
    );
    ticks = CLOCK() - start;
    //skk LOG("Count number of components: %lu clock ticks\n", ticks);
    start = CLOCK();
    s.num_components = emu::repl_reduce(num_components_, std::plus<>());
    ticks = CLOCK() - start;
    //skk  LOG("Reduce number of components: %lu clock ticks\n", ticks);

    return s;
}

void
components::dump()
{
    long max_component = *std::max_element(component_.begin(), component_.end());

    auto print_range = [](bool first_entry, long c, long first, long last) {
        if (first_entry) {
            printf("Component %li: ", c);
        } else {
            printf(", ");
        }
        if (first == last) {
            printf("%li", first);
        } else {
            printf("%li-%li", first, last);
        }
    };

    // For each component...
    for (long c = 0; c <= max_component; ++c) {
        long range_start = -1;
        bool first_entry = true;
        for (long v = 0; v < g_->num_vertices(); ++v) {
            // Is the vertex in the component?
            if (c == component_[v]) {
                // Record the start of a range of vertices
                if (range_start < 0) { range_start = v; }
            } else {
                // End of the range
                if (range_start >= 0) {
                    // Print start of line only once, omit for empty components
                    // Print vertices as a range if possible
                    print_range(first_entry, c, range_start, v - 1);
                    // Reset range
                    range_start = -1;
                    first_entry = false;
                }
            }
        }
        // Print last range
        if (range_start >= 0) {
            print_range(first_entry, c, range_start, g_->num_vertices() - 1);
        }
        if (!first_entry) { printf("\n"); }
        fflush(stdout);
    }
}

// Do serial BFS on each component to check labels
bool
components::check()
{
    // Make sure we reach each vertex at least once
    std::vector<long> visited(g_->num_vertices(), 0);

    // Build a map from component labels to a vertex in that component
    std::unordered_map<long, long> label_to_source;
    for (long v = 0; v < g_->num_vertices(); ++v) {
        label_to_source[component_[v]] = v;
    }

    for (auto p : label_to_source) {
        long my_component = p.first;
        long source = p.second;
        visited[source] = 1;

        // Do a serial BFS
        // Push source into the queue
        std::queue<long> q;
        q.push(source);
        // For each vertex in the queue...
        while (!q.empty()) {
            long u = q.front(); q.pop();
            // For each out-neighbor of this vertex...
            auto edges_begin = g_->out_neighbors(u);
            auto edges_end = edges_begin + g_->out_degree(u);
            for (auto e = edges_begin; e < edges_end; ++e) {
                long v = *e;
                // Check that all neighbors have the same component ID
                if (component_[v] != my_component) {
                    LOG("Connected vertices in different components: \n");
                    LOG("%li (component %li) -> %li (component %li)\n",
                        source, my_component, v, component_[v]);
                    return false;
                }
                // Add unexplored neighbors to the queue
                if (!visited[v]) {
                    visited[v] = 1;
                    q.push(v);
                }
            }
        }
    }

    // Make sure we visited all vertices
    for (long v = 0; v < g_->num_vertices(); ++v) {
        if (visited[v] == 0) {
            LOG("Failed to visit %li\n", v);
            return false;
        }
    }

    return true;
}
