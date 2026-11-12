// Function::deserialize on a CasADi serialization stream (the .casadi file format).
#include <casadi/casadi.hpp>
#include <sstream>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  try {
    std::istringstream in(std::string(reinterpret_cast<const char*>(data), size));
    casadi::Function f = casadi::Function::deserialize(in);
    if (f.is_null()) return 0;
    // A successfully deserialized function must be usable: print it and evaluate it at zero.
    std::ostringstream os;
    f.disp(os, true);
    std::vector<casadi::DM> arg;
    for (casadi_int i = 0; i < f.n_in(); ++i) arg.push_back(casadi::DM::zeros(f.sparsity_in(i)));
    std::vector<casadi::DM> res;
    f.call(arg, res);
  } catch (const casadi::CasadiException&) {
    return -1;
  }
  return 0;
}
