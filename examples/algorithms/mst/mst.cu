#include <gunrock/algorithms/algorithms.hxx>
#include <gunrock/algorithms/mst.hxx>
#include "mst_cpu.hxx"  // Reference implementation
#include <fstream>

using namespace gunrock;
using namespace memory;

void test_mst(int num_arguments, char** argument_array) {
  if (num_arguments != 2) {
    std::cerr << "usage: ./bin/<program-name> filename.mtx" << std::endl;
    exit(1);
  }

  // --
  // Define types
  
  using vertex_t = int;
  using edge_t   = int;
  using weight_t = float;
  using csr_t =
      format::csr_t<memory_space_t::device, vertex_t, edge_t, weight_t>;

  // --
  // IO
  
  csr_t csr;
  std::string filename = argument_array[1];

  std::ifstream infile (filename);
  std::string line;
  getline(infile, line);
  if (line.find("symmetric") == std::string::npos) {
      printf("Error: input matrix must be symmetric\n");
      exit(1);
  }

  if (util::is_market(filename)) {
    io::matrix_market_t<vertex_t, edge_t, weight_t> mm;
    csr.from_coo(mm.load(filename));
  } else if (util::is_binary_csr(filename)) {
    csr.read_binary(filename);
  } else {
    std::cerr << "Unknown file format: " << filename << std::endl;
    exit(1);
  }

  // --
  // Build graph

  auto G = graph::build::from_csr<memory_space_t::device, graph::view_t::csr>(
      csr.number_of_rows,
      csr.number_of_columns,
      csr.number_of_nonzeros,
      csr.row_offsets.data().get(),
      csr.column_indices.data().get(),
      csr.nonzero_values.data().get() 
  );

  if (G.is_directed()) {
      printf("Error: invalid graph (MST is only defined on undirected graphs)\n");
      exit(1);
  }

  // --
  // Params and memory allocation
  
  vertex_t n_vertices = G.get_number_of_vertices();
  thrust::device_vector<weight_t> mst_weight(1);


  // --
  // GPU Run

  float gpu_elapsed = gunrock::mst::run(G, mst_weight.data().get());
  thrust::host_vector<weight_t> h_mst_weight = mst_weight;
  printf("GPU MST Weight: %f\n", h_mst_weight[0]);

  // --
  // CPU Run

  weight_t cpu_mst_weight;
  float cpu_elapsed = mst_cpu::run<csr_t, vertex_t, edge_t, weight_t>(
      csr, &cpu_mst_weight);
  printf("CPU MST Weight: %f\n", cpu_mst_weight);

  // --
  // Log

  std::cout << "GPU Elapsed Time : " << gpu_elapsed << " (ms)" << std::endl;
  std::cout << "CPU Elapsed Time : " << cpu_elapsed << " (ms)" << std::endl;
}

int main(int argc, char** argv) {
  test_mst(argc, argv);
}