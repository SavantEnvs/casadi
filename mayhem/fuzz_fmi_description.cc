// DaeBuilder::load_fmi_description: FMI modelDescription.xml (FMI 1/2/3) incl. symbolic extensions.
#include <casadi/casadi.hpp>
#include "scratch.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string path = scratch_write(data, size);
  try {
    casadi::DaeBuilder dae("model");
    dae.load_fmi_description(path);
    dae.nx();
    dae.nu();
    for (const std::string& n : dae.all()) dae.start(n);
  } catch (const casadi::CasadiException&) {
    return -1;
  }
  return 0;
}
