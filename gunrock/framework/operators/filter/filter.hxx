#pragma once

#include <gunrock/cuda/context.hxx>
#include <gunrock/framework/operators/configs.hxx>
#include <gunrock/util/type_limits.hxx>
#include <gunrock/util/type_traits.hxx>

#include <gunrock/framework/operators/filter/uniquify.hxx>
#include <gunrock/framework/operators/filter/predicated.hxx>
#include <gunrock/framework/operators/filter/bypass.hxx>

namespace gunrock {
namespace operators {
namespace filter {

template <filter_type_t type = filter_type_t::predicated,
          typename graph_t,
          typename enactor_type,
          typename operator_type>
void execute(graph_t& G,
             enactor_type* E,
             operator_type op,
             cuda::standard_context_t* context) {
  if (type == filter_type_t::uniquify)
    uniquify::execute(G, E, op, *context);
  else if (type == filter_type_t::predicated)
    predicated::execute(G, E, op, *context);
  else if (type == filter_type_t::bypass)
    bypass::execute(G, E, op, *context);
  else
    error::throw_if_exception(cudaErrorUnknown);
}

}  // namespace filter
}  // namespace operators
}  // namespace gunrock