// Conic QP solvers (qrqp, ipqp) on a small problem decoded from the input bytes:
//   byte 0 solver, 1 nx, 2 ng, then sparsity bits of H (lower triangle, mirrored) and A, then values.
#include <casadi/casadi.hpp>
#include <vector>

namespace {
struct Reader {
  const uint8_t* p;
  size_t n, i = 0;
  uint8_t byte() { return i < n ? p[i++] : 0; }
  bool bit() {
    if (nbits == 0) { cur = byte(); nbits = 8; }
    bool b = cur & 1; cur >>= 1; --nbits; return b;
  }
  double val() { return static_cast<int8_t>(byte()) / 8.0; }
  uint8_t cur = 0;
  int nbits = 0;
};
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 3) return -1;
  Reader r{data, size};
  const char* solver = (r.byte() & 1) ? "ipqp" : "qrqp";
  casadi_int nx = 1 + r.byte() % 8, ng = r.byte() % 9;

  std::vector<casadi_int> hr, hc, ar, ac;
  std::vector<double> hv, av;
  for (casadi_int j = 0; j < nx; ++j)
    for (casadi_int i = j; i < nx; ++i)
      if (r.bit()) {
        double v = r.val();
        hr.push_back(i); hc.push_back(j); hv.push_back(v);
        if (i != j) { hr.push_back(j); hc.push_back(i); hv.push_back(v); }
      }
  for (casadi_int j = 0; j < nx; ++j)
    for (casadi_int i = 0; i < ng; ++i)
      if (r.bit()) { ar.push_back(i); ac.push_back(j); av.push_back(r.val()); }

  casadi::DM H = casadi::DM::triplet(hr, hc, hv, nx, nx);
  casadi::DM A = casadi::DM::triplet(ar, ac, av, ng, nx);
  std::vector<double> g(nx), lbx(nx), ubx(nx), x0(nx), lba(ng), uba(ng);
  for (auto& v : g) v = r.val();
  for (casadi_int i = 0; i < nx; ++i) { lbx[i] = r.val(); ubx[i] = lbx[i] + std::abs(r.val()); x0[i] = r.val(); }
  for (casadi_int i = 0; i < ng; ++i) { lba[i] = r.val(); uba[i] = lba[i] + std::abs(r.val()); }

  try {
    casadi::Dict opts = {{"error_on_fail", false}, {"print_header", false},
                         {"print_iter", false}, {"print_info", false}};
    casadi::Function f = casadi::conic("qp", solver, {{"h", H.sparsity()}, {"a", A.sparsity()}}, opts);
    casadi::DMDict arg = {{"h", H}, {"a", A}, {"g", g}, {"lbx", lbx}, {"ubx", ubx},
                          {"lba", lba}, {"uba", uba}, {"x0", x0}};
    f(arg);
  } catch (const casadi::CasadiException&) {
    return -1;
  }
  return 0;
}
