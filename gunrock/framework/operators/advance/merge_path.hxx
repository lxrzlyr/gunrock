/**
 * @file merge_path.hxx
 * @author Muhammad Osama (mosama@ucdavis.edu)
 * @brief
 * @version 0.1
 * @date 2020-10-20
 *
 * @copyright Copyright (c) 2020
 *
 */

#pragma once

#include <gunrock/util/math.hxx>
#include <gunrock/cuda/context.hxx>

#include <gunrock/framework/operators/configs.hxx>

// XXX: Replace these later
#include <moderngpu/transform.hxx>
#include <moderngpu/kernel_scan.hxx>
#include <moderngpu/kernel_load_balance.hxx>

namespace gunrock {
namespace operators {
namespace advance {
namespace merge_path {
template <advance_type_t type,
          typename graph_t,
          typename enactor_type,
          typename operator_type>
void forward(graph_t& G,
             enactor_type* E,
             operator_type op,
             cuda::standard_context_t& __ignore) {
  // XXX: should use existing context (__ignore)
  mgpu::standard_context_t context(false, __ignore.stream());

  // Used as an input buffer (frontier)
  auto active_buffer = E->get_active_frontier_buffer();
  // Used as an output buffer (frontier)
  auto inactive_buffer = E->get_inactive_frontier_buffer();

  // Get input data of the active buffer.
  auto input_data = active_buffer->data();

  // Scan over the work domain to find the output frontier's size.
  auto scanned_work_domain = E->scanned_work_domain.data().get();
  thrust::device_vector<int> count(1, 0);

  auto segment_sizes = [=] __device__(std::size_t idx) {
    int count = 0;
    int v = input_data[idx];

    // if item is invalid, skip processing.
    if (!gunrock::util::limits::is_valid(v))
      return 0;

    count = G.get_number_of_neighbors(v);
    return count;
  };

  mgpu::transform_scan<int>(segment_sizes, (int)active_buffer->size(),
                            scanned_work_domain, mgpu::plus_t<int>(),
                            count.data(), context);

  // If output frontier is empty, resize and return.
  thrust::host_vector<int> front = count;
  if (!front[0]) {
    inactive_buffer->resize(front[0]);
    return;
  }

  // Resize the output (inactive) buffer to the new size.
  inactive_buffer->resize(front[0]);
  auto output_data = inactive_buffer->data();

  // Expand incoming neighbors, and using a load-balanced transformation
  // (merge-path based load-balancing) run the user defined advance operator on
  // the load-balanced work items.
  auto neighbors_expand = [=] __device__(std::size_t idx, std::size_t seg,
                                         std::size_t rank) {
    auto v = input_data[seg];

    // if item is invalid, skip processing.
    if (!gunrock::util::limits::is_valid(v))
      return;

    auto start_edge = G.get_starting_edge(v);
    auto e = start_edge + rank;
    auto n = G.get_destination_vertex(e);
    auto w = G.get_edge_weight(e);
    bool cond = op(v, n, e, w);
    output_data[idx] =
        cond ? n : gunrock::numeric_limits<decltype(v)>::invalid();
  };

  mgpu::transform_lbs(neighbors_expand, front[0], scanned_work_domain,
                      (int)active_buffer->size(), context);

  // Swap frontier buffers, output buffer now becomes the input buffer and
  // vice-versa.
  E->swap_frontier_buffers();
}

template <advance_type_t type,
          advance_direction_t direction,
          typename graph_t,
          typename enactor_type,
          typename operator_type>
void execute(graph_t& G,
             enactor_type* E,
             operator_type op,
             cuda::standard_context_t& __ignore) {
  if (direction == advance_direction_t::forward) {
    forward<type>(G, E, op, __ignore);
  } else if (direction == advance_direction_t::backward) {
    // backward<type>(G, E, op, __ignore);
  } else {  // both (forward + backward)
    using find_csr_t = typename graph_t::graph_csr_view_t;
    using find_csc_t = typename graph_t::graph_csc_view_t;

    // std::cout << "\tContains CSR Representation? " << std::boolalpha
    //           << G.contains_representation<find_csr_t>() << std::endl;

    // static_assert(
    //     (G.contains_representation<find_csr_t>() &&
    //      G.contains_representation<find_csc_t>()),
    //     "Direction optimized advance is only supported when the graph exists
    //     " "in both CSR and CSC sparse-matrix representations.");
  }
}
}  // namespace merge_path
}  // namespace advance
}  // namespace operators
}  // namespace gunrock