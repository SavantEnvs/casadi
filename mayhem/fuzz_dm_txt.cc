// DM::from_file, "txt" format.
#include <casadi/casadi.hpp>
#include "scratch.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string path = scratch_write(data, size);
  try {
    casadi::DM m = casadi::DM::from_file(path, "txt");
    m.nnz();
  } catch (const casadi::CasadiException&) {
    return -1;
  }
  return 0;
}
