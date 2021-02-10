
namespace fpga {

void print_graph (graph *, FILE *);
void print_cp (graph *, FILE *);
void print_io (graph *, FILE *);
void print_owner (port *, FILE *);
void print_arb (graph *, FILE *);
void print_path (std::vector<port *> &path, FILE *);
void print_gate (gate *, FILE *);
void print_node (node *, FILE *);
void print_config (fpga_config *, FILE *);

}
