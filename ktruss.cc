#include "ktruss.h"
#include <algorithm>
#include <emu_cxx_utils/intrinsics.h>
#include <cassert>
#include <cilk/cilk.h>
#include <emu_c_utils/emu_c_utils.h>

using namespace emu;
using namespace emu::parallel;

ktruss::ktruss(ktruss_graph & g)
: g_(&g)
, vertex_max_k_(g.num_vertices())
, worklist_(g.num_vertices())
{
}

ktruss::ktruss(const ktruss& other, emu::shallow_copy shallow)
: g_(other.g_)
, vertex_max_k_(other.vertex_max_k_, shallow)
, worklist_(other.worklist_, shallow)
{
}

void
ktruss::clear()
{
    // Init all edge property values to zero
    g_->for_each_vertex(dyn, [this](long src) {
        vertex_max_k_[src] = 0;
        g_->for_each_out_edge(src, [](ktruss_edge_slot &dst) {
            dst.TC = 0;
            dst.qrC = 0;
            dst.KTE = 0;
        });
    });
}

void
ktruss::build_worklist()
{
    worklist_.clear_all();
    g_->for_each_vertex(fixed, [this](long p) {
        // Use binary search to find neighbors of p that are less than p
        auto q_begin = g_->out_edges_begin(p);
        auto q_end = std::lower_bound(q_begin, g_->out_edges_end(p), p);
        // Add these edges to the work list
        if (q_begin != q_end) {
            worklist_.append(p, q_begin, q_end);
        }
    });
}

void
ktruss::count_initial_triangles()
{
    build_worklist();
    // Process all p->q edges with a thread pool
    worklist_.process_all_edges(dyn, [this](long p, ktruss_edge_slot& pq) {
        long q = pq.dst;
        // Range of q's neighbors that are less than q
        auto qr_begin = g_->out_edges_begin(q);
        auto qr_end = std::lower_bound(qr_begin, g_->out_edges_end(q), q);
        // Iterator over edges of p
        auto pr = g_->out_edges_begin(p);

        for (auto qr = qr_begin; qr != qr_end; ++qr){
            while (*pr < *qr) { pr++; }
            if (*qr == *pr) {
                // Found the triangle p->q->r
                emu::remote_add(&qr->TC, 1);
                emu::remote_add(&pq.TC, 1);
                emu::remote_add(&pr->TC, 1);
                emu::remote_add(&qr->qrC, 1);
                emu::remote_add(&g_->find_out_edge(q, p)->pRefC, 1);
            }
        }
    });
}

void
ktruss::unroll_wedges(long k)
{
    build_worklist();
    // Process all p->q edges with a thread pool
    worklist_.process_all_edges(dyn, [this, k](long p, ktruss_edge_slot& pq)
    {
        long q = pq.dst;
        // Iterator over edges of p
        auto pr = g_->out_edges_begin(p);

        if (pq.TC < k - 2 || pr->TC < k - 2) {
            // Range of q's neighbors that are less than q
            auto qr_begin = g_->out_edges_begin(q);
            auto qr_end = std::lower_bound(qr_begin, g_->out_edges_end(q), q);

            for (auto qr = qr_begin; qr != qr_end; ++qr){
                while (*pr < *qr) { pr++; }
                if (*qr == *pr) {
                    // Unroll triangle
                    emu::remote_add(&qr->TC, -1);
                    emu::remote_add(&pq.TC, -1);
                    emu::remote_add(&pr->TC, -1);
                    emu::remote_add(&qr->qrC, -1);
                    emu::remote_add(&g_->find_out_edge(q, p)->pRefC, -1);
                }
            }
        }
    });
}

void
ktruss::unroll_supported_triangles(long k)
{
    build_worklist();
    // Process all q->r edges with a thread pool
    worklist_.process_all_edges(dyn, [this, k](long q, ktruss_edge_slot& qr)
    {
        if (qr.TC < k - 2 && qr.qrC > 0) {
            // Range of q's neighbors that are GREATER than q
            // i.e. we are looking at edges in the other direction
            auto qp_end = g_->out_edges_end(q);
            auto qp_begin = std::upper_bound(g_->out_edges_begin(q), qp_end, q);

            for (auto qp = qp_begin; qp != qp_end; ++qp){
                if (qp->pRefC > 0) {
                    // Unroll supported triangle
                    long r = qr.dst;
                    long p = qp->dst;
                    emu::remote_add(&qr.TC, -1);
                    emu::remote_add(&g_->find_out_edge(p, q)->TC, -1);
                    emu::remote_add(&g_->find_out_edge(p, r)->TC, -1);
                    emu::remote_add(&qr.qrC, -1);
                }
            }
        }
    });
}

long
ktruss::remove_edges(long k)
{
    // TODO use reduction variable here?
    long num_removed = 0;
    g_->for_each_vertex(emu::dyn, [&](long v){
        // Get the edge list for this vertex
        auto begin = g_->out_edges_begin(v);
        auto end = g_->out_edges_end(v);
        // Move all edges with TC == 0 to the end of the list
        auto remove_begin = std::stable_partition(begin, end,
            [](ktruss_edge_slot& e) {
                return e.TC != 0;
            }
        );
        if (remove_begin != end) {
            // Set K for the edges we are about to remove
            for_each(remove_begin, end, [k](ktruss_edge_slot &e) {
                e.KTE = k - 1;
            });
            // Resize the edge list to drop the edges off the end
            g_->set_out_degree(v, remove_begin - begin);
            emu::remote_add(&num_removed, end - remove_begin);
        }
    });
    return num_removed;
}

ktruss::stats
ktruss::compute_truss_sizes(long max_k)
{
    stats s(max_k);
    build_worklist();
    // Process all p->q edges with a thread pool
    worklist_.process_all_edges(dyn, [&](long src, ktruss_edge_slot& dst) {
        assert(dst.KTE >= 2);
        assert(dst.KTE <= max_k);
        // This vertex is a member of all trusses up through k
        emu::remote_max(&vertex_max_k_[src], dst.KTE);
        // This edge is a member of all trusses up through k
        for (long k = 2; k <= dst.KTE; ++k) {
            emu::remote_add(&s.edges_per_truss[k], 1);
        }
    });

    g_->for_each_vertex(fixed, [&](long src) {
        // This edge is a member of all trusses up through k
        for (long k = 2; k <= vertex_max_k_[src]; ++k) {
            emu::remote_add(&s.vertices_per_truss[k], 1);
        }
    });
    return s;
}

ktruss::stats
ktruss::run()
{
    long num_edges = g_->num_edges() / 2;
    long num_removed = 0;
    count_initial_triangles();
    long k = 3;
    do {
        do {
            unroll_wedges(k);
            unroll_supported_triangles(k);
            num_removed = remove_edges(k);
            num_edges -= num_removed;
        } while (num_removed > 0);
        ++k; LOG("Found the %li-truss...\n", k);
    } while (num_edges > 0);

    return compute_truss_sizes(k);
}

// Do serial ktruss computation
bool
ktruss::check()
{
    // FIXME
    return false;
}
