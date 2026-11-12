// Known-answer program for mayhem/test.sh: prints "<case> <value>" lines; test.sh compares them.
#include <casadi/casadi.hpp>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unistd.h>

using namespace casadi;

static void out(const char* name, double v) {
  if (v < 0 ? -v < 5e-7 : v < 5e-7) v = 0;
  std::printf("%s %.5f\n", name, v);
}

static DMDict solve_qp(const std::string& solver) {
  Sparsity H = Sparsity::dense(2, 2), A = Sparsity::dense(1, 2);
  Function s = conic("qp", solver, {{"h", H}, {"a", A}},
                     Dict{{"print_iter", false}, {"print_header", false}, {"print_info", false}});
  return s(DMDict{{"h", DM::eye(2)}, {"g", DM({-1.0, -1.0})}, {"a", DM::ones(1, 2)},
                  {"lba", -DM::inf()}, {"uba", 1}});
}

int main(int argc, char** argv) {
  const char* xml = argc > 1 ? argv[1] : nullptr;
  {  // symbolic differentiation: d/dx (x^2 y) at (3,2), and function value
    SX x = SX::sym("x"), y = SX::sym("y");
    Function f("f", {x, y}, {x * x * y, gradient(x * x * y, x)});
    DMVector r = f(DMVector{3, 2});
    out("sx_value", double(r[0]));
    out("sx_dfdx", double(r[1]));
  }
  {  // MX matrix product
    MX a = MX::sym("a", 2, 2), b = MX::sym("b", 2);
    Function f("f", {a, b}, {mtimes(a, b)});
    DM am = DM::zeros(2, 2), bv = DM::zeros(2);
    am(0, 0) = 1; am(0, 1) = 2; am(1, 0) = 3; am(1, 1) = 4; bv(0) = 5; bv(1) = 6;
    DMVector r = f(DMVector{am, bv});
    out("mx_mtimes0", double(r[0](0)));
    out("mx_mtimes1", double(r[0](1)));
  }
  {  // serialization round trip, then evaluate the deserialized function
    SX x = SX::sym("x", 2);
    Function f("f", {x}, {sin(x(0)) + x(1) * x(1)});
    Function g = Function::deserialize(f.serialize());
    out("serialize_roundtrip", double(g(DMVector{DM({0.0, 3.0})})[0]));
  }
  for (const char* solver : {"qrqp", "ipqp"}) {
    DMDict r = solve_qp(solver);
    out((std::string(solver) + "_x0").c_str(), double(r["x"](0)));
    out((std::string(solver) + "_x1").c_str(), double(r["x"](1)));
    out((std::string(solver) + "_cost").c_str(), double(r["cost"]));
  }
  {  // Newton rootfinder: z^2 = 2
    SX z = SX::sym("z");
    Function rf = rootfinder("rf", "newton", Function("r", {z}, {z * z - 2}),
                             Dict{{"abstol", 1e-12}});
    out("rootfinder_sqrt2", double(rf(DMVector{1.0})[0]));
  }
  {  // CVODES: xdot = -p x over [0,1]
    SX x = SX::sym("x"), p = SX::sym("p");
    Function I = integrator("I", "cvodes", SXDict{{"x", x}, {"p", p}, {"ode", -p * x}}, 0, 1,
                            Dict{{"abstol", 1e-12}, {"reltol", 1e-10}});
    out("cvodes_exp", double(I(DMDict{{"x0", 1}, {"p", 1}}).at("xf")));
  }
  {  // linear interpolant through (0,0),(1,1),(2,4),(3,9) at 2.5
    Function li = interpolant("li", "linear", std::vector<std::vector<double>>{{0, 1, 2, 3}},
                              std::vector<double>{0, 1, 4, 9});
    out("interpolant_linear", double(li(DMVector{2.5})[0]));
  }
  {  // DM / Sparsity file readers
    char path[] = "/tmp/casadi-kat-XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) return 2;
    const char* mtx = "%%MatrixMarket matrix coordinate real general\n3 3 3\n1 1 1.5\n3 2 3.25\n2 3 -2\n";
    if (write(fd, mtx, std::char_traits<char>::length(mtx)) < 0) return 2;
    close(fd);
    DM m = DM::from_file(path, "mtx");
    out("mtx_nnz", double(m.nnz()));
    out("mtx_sum", double(sum1(sum2(m))));
    out("mtx_32", double(m(2, 1)));
    unlink(path);
  }
  if (xml) {  // FMI description
    DaeBuilder dae("model");
    dae.load_fmi_description(xml);
    out("fmi_nx", double(dae.nx()));
    out("fmi_nu", double(dae.nu()));
  }
  return 0;
}
