#include <act/graph.h>
#include <vector>
#include <map>
#include <string.h>

namespace fpga {

static struct Hashtable *labels;
unsigned int ffd_num = 0;
unsigned int mdm_num = 0;
unsigned int arb_num = 0;

std::vector<port *> delayed_ports;

void print_ff (int func, std::string &ff) 
{
  ff += "`timescale 1ns/1ps\n\n";
  ff += "module \\ff_delay #(\n";
  ff += "\tparameter\tDELAY\t=\t1\n";
  ff += ")(\n";
  ff += "\tinput\t\tclock\n";
  ff += ",\tinput\t\treset\n";
  ff += ",\tinput\t\tin\n";
  ff += ",\toutput\t\tout\n";
  ff += ");\n\n\n";
  ff += "reg\t[DELAY-1:0]\tout_d = 0;\n\n";
  ff += "assign out = out_d[DELAY-1];\n\n";
  if (func == 1) {
    ff += "always @(posedge clock)\n";
    ff += "if (\\reset )\tout_d[0]\t<=\t1'b0;\n";
    ff += "else\t\t\tout_d[0] <= in;\n\n";
  } else {
    ff += "always @(*)\n";
    ff += "if (\\reset )\t#1 out_d[0]\t<=\t1'b0;\n";
    ff += "else\t\t\t#1 out_d[0] <= in;\n\n";
  }

  ff += "genvar i;\n";
  ff += "generate\n";
  ff += "for (i = 1; i < DELAY; i = i + 1) begin\n";
  ff += "\talways @(posedge clock)\n";
  ff += "\t\tout_d[i] <= out_d[i-1];\n";
  ff += "end\n";
  ff += "endgenerate\n\n";
  ff += "endmodule\n\n";
}

void print_delay (port *p, std::string &dd) 
{
  char buf[1024];

  dd += "ff_delay #(\n\t.DELAY(";
  dd += std::to_string(p->delay);
  dd += ")\n) ";
  dd += "ffd_";
  dd += std::to_string(ffd_num);
  dd += "(";
  dd += "\n\t.in(\\";
  p->c->toid()->sPrint(buf,1024);
  dd += buf;
  dd += "_delay ),\n";
  dd += "\t.out(\\";
  dd += buf;
  dd += " )\n";
  dd += " );\n\n";
  ffd_num++;
}

void print_arb_lo(std::string &arbl) {
  arbl += "module \\arb_lo #(\n";
  arbl += "\tparameter\tARB_NUM\t=\t2\n";
  arbl += ")(\n";
  arbl += "\tinput\t\t[ARB_NUM-1:0]\tin,\n";
  arbl += "\toutput\treg\t[ARB_NUM-1:0]\tout\n";
  arbl += ");\n\n";
  arbl += "genvar i;\n";
  arbl += "generate\n";
  arbl += "\tfor (i = 0; i < ARB_NUM-1; i = i + 1) begin\n";
  arbl += "\t\talways @ (*)\n";
  arbl += "\t\tif (!in[i] & &(in[ARB_NUM-1:i+1])) out[i] <= 1'b0;\n";
  arbl += "\t\telse\tout[i] <= 1'b1;\n";
  arbl += "\tend\n";
  arbl += "endgenerate\n\n";
  arbl += "always @ (*)\n";
  arbl += "if (!in[ARB_NUM-1])\tout[ARB_NUM-1]\t<= 1'b0;\n";
  arbl += "else\tout[ARB_NUM-1]\t<= 1'b1;\n\n";
  arbl += "endmodule\n\n";
}

void print_arb_hi(std::string &arbh) {
  arbh += "module \\arb_hi #(\n";
  arbh += "\tparameter\tARB_NUM\t=\t2\n";
  arbh += ")(\n";
  arbh += "\tinput\t\t[ARB_NUM-1:0]\tin,\n";
  arbh += "\toutput\treg\t[ARB_NUM-1:0]\tout\n";
  arbh += ");\n\n";
  arbh += "genvar i;\n";
  arbh += "generate\n";
  arbh += "\tfor (i = 0; i < ARB_NUM-1; i = i + 1) begin\n";
  arbh += "\t\talways @ (*)\n";
  arbh += "\t\tif (in[i] & &(!in[ARB_NUM-1:i+1])) out[i] <= 1'b1;\n";
  arbh += "\t\telse\tout[i] <= 1'b0;\n";
  arbh += "\tend\n";
  arbh += "endgenerate\n\n";
  arbh += "always @ (*)\n";
  arbh += "if (in[ARB_NUM-1])\tout[ARB_NUM-1]\t<= 1'b1;\n";
  arbh += "else\tout[ARB_NUM-1]\t<= 1'b0;\n\n";
  arbh += "endmodule\n\n";
}

void print_fair_hi(std::string &fh){
  fh += "module fair_hi #(\n";
  fh += "\tparameter   WIDTH = 8\n";
  fh += ")(\n";
  fh += "\t\tinput   clock,\n";
  fh += "\t\tinput   reset,\n";
  fh += "\t\tinput   [WIDTH-1:   0]  req,\n";
  fh += "\t\toutput  [WIDTH-1:   0]  grant\n";
  fh += "\t);\n";
  fh += "\t\n";
  fh += "reg\t[WIDTH-1:0]\tpriority [0:WIDTH-1];\n";
  fh += "\n";
  fh += "reg\t[WIDTH-1:0]\treq_d;\n";
  fh += "wire\t[WIDTH-1:0]\treq_neg;\n";
  fh += "\n";
  fh += "wire shift_prio;\n";
  fh += "\n";
  fh += "wire [WIDTH-1:0] match;\n";
  fh += "reg [WIDTH-1:0] arb_match;\n";
  fh += "\n";
  fh += "always @(posedge clock)\n";
  fh += "if (reset)\n";
  fh += "\treq_d <=  0;\n";
  fh += "else\n";
  fh += "\treq_d <= req;\n";
  fh += "\n";
  fh += "assign shift_prio = |(req_neg & priority[0]) & !(|arb_match);\n";
  fh += "\t\n";
  fh += "genvar j;\n";
  fh += "genvar i;\n";
  fh += "\n";
  fh += "generate\n";
  fh += "for (j = WIDTH; j > 0; j = j-1) begin    \n";
  fh += "\n";
  fh += "always @ (*)\n";
  fh += "if (j > 1)\n";
  fh += "\tif (match[j-1] & &(!match[j-2:0]))\tarb_match[j-1] <= 1'b1;\n";
  fh += "\telse\t\t\t\t\t\t\t\tarb_match[j-1] <= 1'b0;\n";
  fh += "else\n";
  fh += "\tif (match[j-1])\tarb_match[j-1]  <= 1'b1;\n";
  fh += "\telse\t\t\tarb_match[j-1] <= 1'b0;\n";
  fh += "end\n";
  fh += "endgenerate\n";
  fh += "\n";
  fh += "generate\n";
  fh += "for (j = 0; j < WIDTH; j = j+1) begin\n";
  fh += "\n";
  fh += "assign req_neg[j] = !req[j] & req_d[j];\n";
  fh += "\n";
  fh += "assign grant = req & priority[0];\n";
  fh += "\n";
  fh += "assign match[j] = |(req & priority[j]);\n";
  fh += "\n";
  fh += "always @(posedge clock)\n";
  fh += "if (reset)\n";
  fh += "\tpriority[j] <= {{j{1'b0}},1'b1,{(WIDTH-1-j){1'b0}}};\n";
  fh += "else if (shift_prio)\n";
  fh += "\tif (priority[j] == 1)\n";
  fh += "\t\tpriority[j] <= {1'b1, {(WIDTH-1){1'b0}}};\n";
  fh += "\telse\n";
  fh += "\t\tpriority[j] <= priority[j] >> 1;\n";
  fh += "else if (arb_match[j]) begin\n";
  fh += "\tpriority[0] <= priority[j];\n";
  fh += "\tpriority[j] <= priority[0];\n";
  fh += "end\n";
  fh += "\t\n";
  fh += "end\n";
  fh += "endgenerate\n";
  fh += "\t\n";
  fh += "endmodule\n";
}

void print_fair_lo(std::string &fl){
  fl += "module fair_lo #(\n";
  fl += "\tparameter   WIDTH = 8\n";
  fl += ")(\n";
  fl += "\t\tinput   clock,\n";
  fl += "\t\tinput   reset,\n";
  fl += "\t\tinput   [WIDTH-1:   0]  req,\n";
  fl += "\t\toutput  [WIDTH-1:   0]  grant\n";
  fl += "\t);\n";
  fl += "\t\n";
  fl += "reg [WIDTH-1:0] priority [0:WIDTH-1];\n";
  fl += "\n";
  fl += "reg  [WIDTH-1:0] req_d;\n";
  fl += "wire [WIDTH-1:0] req_pos;\n";
  fl += "\n";
  fl += "wire shift_prio;\n";
  fl += "\n";
  fl += "wire [WIDTH-1:0] match;\n";
  fl += "reg [WIDTH-1:0] arb_match;\n";
  fl += "\n";
  fl += "\n";
  fl += "always @(posedge clock)\n";
  fl += "if (reset)\n";
  fl += "\treq_d <=  {WIDTH{1'b1}};\n";
  fl += "else\n";
  fl += "\treq_d <= req;\n";
  fl += "\n";
  fl += "assign shift_prio = |(req_pos & priority[0]) & !(|arb_match);\n";
  fl += "\t\n";
  fl += "genvar j;\n";
  fl += "genvar i;\n";
  fl += "\n";
  fl += "generate\n";
  fl += "for (j = WIDTH; j > 0; j = j-1) begin    \n";
  fl += "\n";
  fl += "always @ (*)\n";
  fl += "if (j > 1)\n";
  fl += "\tif (match[j-1] & &(~match[j-2:0]))\tarb_match[j-1] <= 1'b1;\n";
  fl += "\telse\t\t\t\t\t\t\t\tarb_match[j-1] <= 1'b0;\n";
  fl += "else\n";
  fl += "\tif (match[j-1])\tarb_match[j-1]  <= 1'b1;\n";
  fl += "\telse\t\t\tarb_match[j-1] <= 1'b0;\n";
  fl += "end\n";
  fl += "endgenerate\n";
  fl += "\n";
  fl += "generate\n";
  fl += "for (j = 0; j < WIDTH; j = j+1) begin\n";
  fl += "\n";
  fl += "assign req_pos[j] = req[j] & !req_d[j];\n";
  fl += "\n";
  fl += "assign grant = ~(~req & priority[0]);\n";
  fl += "\n";
  fl += "assign match[j] = |(~req & priority[j]);\n";
  fl += "\n";
  fl += "always @(posedge clock)\n";
  fl += "if (reset)\n";
  fl += "\tpriority[j] <= {{j{1'b0}},1'b1,{(WIDTH-1-j){1'b0}}};\n";
  fl += "else if (shift_prio)\n";
  fl += "\tif (priority[j] == 1)\n";
  fl += "\t\tpriority[j] <= {1'b1, {(WIDTH-1){1'b0}}};\n";
  fl += "\telse\n";
  fl += "\t\tpriority[j] <= priority[j] >> 1;\n";
  fl += "else if (arb_match[j]) begin\n";
  fl += "\tpriority[0] <= priority[j];\n";
  fl += "\tpriority[j] <= priority[0];\n";
  fl += "end\n";
  fl += "\t\n";
  fl += "end\n";
  fl += "endgenerate\n";
  fl += "\t\n";
  fl += "endmodule\n";
}

std::string get_module_name (Process *p)
{
  const char *mn = p->getName();
  int len = strlen(mn);
  std::string buf;

  for (auto i = 0; i < len; i++) {
    if (mn[i] == 0x3c) { continue; } 
    else if (mn[i] == 0x3e) { continue; } 
    else if (mn[i] == 0x2c) { buf += "_"; } 
    else { buf += mn[i]; }
  }

  return buf;

}

static void prefix_id_print (Scope *cs, node *n, ActId *id, int dir, std::string &gb) 
{

  if (cs->Lookup (id, 0)) {
    /*prefix_print ();*/
  } else {
    ValueIdx *vx;
    /* must be a global */
    vx = cs->FullLookupVal (id->getName());
    Assert (vx, "Hmm.");
    Assert (vx->global, "Hmm");
    if (vx->global == ActNamespace::Global()) {
      /* nothing to do */
    } else {
      char *tmp = vx->global->Name ();
      gb += tmp;
      gb += "::";
      FREE (tmp);
    }
  }
  act_connection *c = id->Canonical(cs);
  ActId *pid = c->toid();
  char buf[1024];
  pid->sPrint(buf, 1024);
  gb += "\\";
  gb += buf;
  if (n->decl[c]->port == 2) {
    gb += "_out ";
  }
  delete pid;

  return;
}

void print_prs_expr(Scope *s,node *n,act_prs_expr_t *e,int flip,int dir,std::string &gb)
{
  hash_bucket_t *b;
  act_prs_lang_t *pl;
  
  if (!e) return;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
    gb += "(";
    print_prs_expr (s, n, e->u.e.l, flip, dir, gb);
    gb += " & ";
    print_prs_expr (s, n, e->u.e.r, flip, dir, gb);
    gb += " )";
    break;
    
  case ACT_PRS_EXPR_OR:
    gb += "(";
    print_prs_expr (s, n, e->u.e.l, flip, dir, gb);
    gb += " | ";
    print_prs_expr (s, n, e->u.e.r, flip, dir, gb);
    gb += " )";
    break;
    
  case ACT_PRS_EXPR_VAR:
    if (flip) { gb += "~"; }
    gb += "(";
    prefix_id_print (s, n, e->u.v.id, dir, gb);
    gb += " )";
    break;
    
  case ACT_PRS_EXPR_NOT:
    gb += "(";
    gb += "~";
    print_prs_expr (s, n, e->u.e.l, flip,dir, gb);
    gb += " )";
    break;
    
  case ACT_PRS_EXPR_LABEL:
    if (!labels) {
      fatal_error ("No labels defined!");
    }
    b = hash_lookup (labels, e->u.l.label);
    if (!b) {
      fatal_error ("Missing label `%s'", e->u.l.label);
    }
    pl = (act_prs_lang_t *) b->v;
    if (pl->u.one.dir == 0) {
      gb += "~";
    }
    gb += "(";
    print_prs_expr (s, n, pl->u.one.e, flip, dir, gb);
    gb += " )";
    break;

  case ACT_PRS_EXPR_TRUE:
    gb += "true";
    break;
    
  case ACT_PRS_EXPR_FALSE:
    gb += "false";
    break;

  default:
    fatal_error ("What?");
    break;
  }
  return;
}

void print_prs (Scope *s, node *n, act_prs_lang_t *prs, int dir, std::string &gb) 
{
  if (prs->u.one.arrow_type == 0) {
    print_prs_expr(s, n, prs->u.one.e, 0, dir, gb);
  } else if (prs->u.one.arrow_type == 1) {
    if (dir == 1) { gb += "~("; } 
    else { gb += "("; }
    print_prs_expr(s, n, prs->u.one.e, 0, dir, gb);
    gb += " )";
  } else if (prs->u.one.arrow_type == 2) {
    gb += "(";
    print_prs_expr(s, n, prs->u.one.e, dir, dir, gb);
    gb += " )";
  }
  return;
}

void print_bi_dir (port *p, std::string &st)
{
  if (p->bi == 1) {
    if (p->u.i.in->extra_inst == 0) {
      if (p->dir == 0) { st += "_out"; } else if (p->dir == 1) { st += "_in"; }
    } else {
      if (p->dir == 0) { st += "_out"; } else if (p->dir == 1) { st += "_in"; }
    }
  }

  return; 
}

void print_flag_ending (port *p, std::string &flag_e) 
{

  unsigned int d = 0;

  if (p->delay != 0) {
    flag_e += "_delay";
    d = 1;
    for (auto dp : delayed_ports) {
      if (dp == p) {
        d = 0;
        break;
      }
    }
    if (d == 1) {
      delayed_ports.push_back(p);
      d = 0;
    }
  }
  if (p->forced != 0) { flag_e += "_forced"; }
  return;
}


void print_sh_keeper (gate *gn, std::string &keeper)
{
  char buf[1024];
  gn->id->sPrint(buf,1024);

  if (gn->type == 2) {
    keeper += "reg \\";
    keeper += buf;
    print_flag_ending(gn->p[0], keeper);
    keeper += "_reg = 0 ;\n";
    keeper += "always @(posedge clock)\n\t\\";
    keeper += buf;
    print_flag_ending(gn->p[0], keeper);
    keeper += "_reg <= \\";
    keeper += buf;
    print_flag_ending(gn->p[0], keeper);
    keeper += " ;\n\n"; 
  }
  return;
}

void print_header (gate *gn, std::string &header)
{
  if (gn->type == 0 || gn->type == 2 || 
      gn->type == 4 || gn->type == 5) { header += "always @(*)\n"; } 
  else { header += "always @(posedge \\clock )\n"; }
  return;
}

void print_dir_ending (int i, std::string &id) {
  if (i == 0)      { id += "_up";  } 
  else if (i == 1) { id += "_dn"; } 
  else if (i == 2) { id += "_weak_up"; } 
  else if (i == 3) { id += "_weak_dn"; }
  return;
}

void print_condition (Scope *cs, node *n, std::vector<act_prs_lang_t *> &netw, 
                  int dir, std::string &gb)
{
  int prs_cnt = 0;
  prs_cnt = netw.size();
  for(auto br : netw) {
    if (br) {
      print_prs (cs, n, br, dir, gb);
      prs_cnt--;
      if (prs_cnt != 0) { gb += "|"; } 
    } else { gb += "1'b0"; }
  }
  return; 
}

void print_gate_postfix(gate *gn, int dir, std::string &gb)
{
  if (gn->drive_type != 2) {
    if (gn->drive_type == 0 & gn->p[0]->dir == 0 && gn->p[0]->bi == 1) {
      gb += "_out";
    } else if (gn->drive_type == 0 & gn->p[0]->dir == 0 && gn->p[0]->bi == 1){
      gb += "_in";
    }
    if (gn->drive_type != 0) { print_dir_ending(dir, gb); }
    print_flag_ending(gn->p[0], gb);
  } else {
    
  }
  return;
}

void print_else (gate *gn, std::string &gb)
{
  if (gn->type == 2 || gn->type == 3 || gn->type == 5) {
    char buf[1024];
    gn->id->sPrint(buf,1024);
    
    gb += "\telse \\";
    if (gn->type == 5) { gb += " #1 \\"; }
    gb += buf;
    print_gate_postfix(gn, 0, gb);
    gb += " <= \\";
    gb += buf;
    print_gate_postfix(gn, 0, gb);
    if (gn->type == 2) { gb += "_reg ;\n\n"; }
    else { gb += " ;\n\n"; }
  }
  return;
}

void print_gate (Scope *cs, node *n, gate *gn, std::string &gb)
{
  char buf[1024];
  gn->id->sPrint(buf,1024);
  for (auto i = 0; i < 4; i++) {
    if (gn->drive_type == 0) {
      if (i == 0) {
        print_sh_keeper(gn, gb);
        print_header(gn,gb);
        gb += "\tif ("; 
      } else {
        gb += "\telse if (";
      }
    } else if (gn->drive_type == 1) {
      print_header(gn,gb);
      gb += "\t";
      if (gn->type == 4 || gn->type == 5) { gb += "#1 \\"; }
      else { gb += "\\"; }
      gb += buf;
      print_gate_postfix(gn, i, gb);
      gb += " <= ";
    }
    if (i == 0)      { print_condition(cs, n, gn->p_up,1, gb); }
    else if (i == 1) { print_condition(cs, n, gn->p_dn,0, gb); }
    else if (i == 2) { print_condition(cs, n, gn->w_p_up,1, gb); }
    else if (i == 3) { print_condition(cs, n, gn->w_p_dn,0, gb); }
    if (gn->drive_type == 0) {
      gb += " ) \\";
      gb += buf;
      print_gate_postfix(gn, i, gb);
      if (i == 0 || i == 2) { gb += " <= 1'b1;\n"; } 
      else { gb += " <= 1'b0;\n"; }
    } else { gb += " ? 1'b1 : 1'b0;\n"; }
  }
  if (gn->drive_type == 0) {
    print_else(gn, gb);
  }
  return;
}

void print_port_postfix (port *p, int i, std::string &st)
{
  if (p->drive_type != 0) { print_dir_ending(i, st); }
  print_flag_ending(p, st);

  return;
}

void print_mux (gate *gn, std::string &gb)
{
  char buf[1024];
  char id[1024];
  gn->id->sPrint(id, 1024);

  int dr_num = 0;

  for (auto i = 0; i < 4; i++) {
    dr_num = 0;
    if (gn->extra_drivers[0]->drive_type == 0) {
      if (i == 0) {
        print_header(gn, gb);
        gb += "\tif ("; 
      } else {
        gb += "\telse if (";
      }
    } else if (gn->extra_drivers[0]->drive_type == 1) {
      print_header(gn, gb);
      gb += "\t\\";
      gb += id;
      print_dir_ending(i, gb);
      print_flag_ending(gn->extra_drivers[0], gb);
      gb += " <= (";
    }
    for (auto pp : gn->extra_drivers) {
      if (pp->dir == 0) { continue; }
      gb += "\\";
      pp->c->toid()->sPrint(buf, 1024);
      gb += buf;
      print_dir_ending(i, gb);
      print_flag_ending(pp, gb);
      gb += "_";
      gb += std::to_string(dr_num);
      gb += " | ";
      dr_num++;
    }
    gb += "1'b0) ";
    if (gn->extra_drivers[0]->drive_type == 0) {
      gb += "\\";
      gb += id;
      if (i == 0 || i == 2) { gb += " <= 1'b1;\n"; } 
      else { gb += " <= 1'b0;\n"; }
    } else {  gb += ";\n"; }
  }

  return;
}

void inst_arb(int dir, std::vector<port *> &fp, FILE *output){
  int len = 0;
  len = fp.size();
  if (dir == 1) {
    fprintf(output, "\\fair_lo #(.ARB_NUM(%i))\n",len);
  } else {
    fprintf(output, "\\fair_hi #(.ARB_NUM(%i))\n",len);
  }
  fprintf(output, "\\arb_%i\n(\n", arb_num);
  fprintf(output, "\t.in({");
  for (auto i = 0; i < len; i++) {
    fprintf(output, "\\");
    fp[i]->c->toid()->Print(output);
    if (fp[i]->delay != 0) {
      fprintf(output, "_delay");
    }
    fprintf(output, "_forced");
    if (i < len -1) {
      fprintf(output, " ,");
    }
  }
  fprintf(output, " }),\n");
  fprintf(output ,"\t.out({");
  for (auto i = 0; i < len; i++) {
    fprintf(output, "\\");
    fp[i]->c->toid()->Print(output);
    if (fp[i]->delay != 0) {
      fprintf(output, "_delay");
    }
    if (i < len -1) {
      fprintf(output, " ,");
    }
  }
  fprintf(output, " })\n");
  fprintf(output, ")\n\n");
  arb_num++;
}

void print_inst_port (port *p, int func, std::string &ins) 
{

  char buf[1024];

  p->c->toid()->sPrint(buf,1024);
  ins += buf;

  if (p->owner == 0 && p->u.p.n->proc) { print_bi_dir(p, ins); } 
  if (p->owner == 1) { print_bi_dir(p, ins); } 
 
  if (p->drive_type != 0) {
    if (p->owner == 1 && (p->primary == 0 || p->u.i.in->extra_inst != 0)) {
      if (p->u.i.in->inst_name) {
        ins += "_";
        if (p->u.i.in->extra_inst == 0) {
          ins += p->u.i.in->inst_name->getName();
        } else {
          ins += "md_";
          ins += std::to_string(p->u.i.in->extra_inst);
        }
        if (p->u.i.in->array) { ins += p->u.i.in->array; }
      }
    }
  } else if (p->owner == 2 && p->dir == 1) { ins += "_gate"; }

  if (func != 0) { print_dir_ending(func - 1, ins); }
  print_flag_ending(p, ins);

  return;
}

void print_inst_ports (inst_node *in, std::string &ins)
{
  char buf[1024];

  int jdr_num = 0;
  if (in->n) {
    for (auto gp : in->n->gp) {
      gp->c->toid()->sPrint(buf,1024);
      ins += "\t,.\\";
      ins += buf;
      ins += "\t(\\";
      ins += buf;
      ins += " )\n";
    }
  }
  for (auto i = 0; i < in->p.size(); i++) {
    in->p[i]->c->toid()->sPrint(buf,1024);
    if (in->p[i]->drive_type == 0) {
      ins += "\t,.\\";
      print_inst_port(in->n->p[i], 0, ins);
      ins += "\t(\\";
      print_inst_port(in->p[i], 0, ins);
      ins += " )\n";
    } else {
      for (auto j = 0; j < 4; j++) {
        ins += "\t,.\\";
        print_inst_port(in->n->p[i], j+1, ins);
        if (in->extra_inst != 0 && in->p[i]->dir == 1) {
          ins += "_";
          ins += std::to_string(jdr_num);
        }
        ins += "\t(\\";
        print_inst_port(in->p[i], j+1, ins);
        ins += " )\n";
      }
      if (in->p[i]->dir == 1) { jdr_num++; }
    }
  }

  return;
}

void print_inst_name (inst_node *in, std::string &ins)
{
  std::string mn;

  if (in->proc && in->n->copy == 0) {
    ins += "\\";
    ins += get_module_name(in->proc);
    ins += " \\";
    ins += in->inst_name->getName();
  } else if (in->proc && in->n->copy == 1) {
    ins += "\\";
    ins += get_module_name(in->proc);
    ins += "_";
    ins += std::to_string(in->n->extra_node);
    ins += " \\";
    ins += in->inst_name->getName();
  } else {
    ins += "\\md_mux_";
    ins += std::to_string(in->extra_inst);
    ins += " \\md_mux_",
    ins += std::to_string(in->extra_inst);
  }
  if (in->array) {
    ins += in->array;
  }

  return;
}
void print_inst(inst_node *in, std::string &ins)
{
  print_inst_name(in, ins);
  ins += " (\n\t .\\clock (\\clock )\n";
  print_inst_ports(in, ins);
  ins += ");\n\n";

  return;
}

void print_var_postfix (var *v, int i, std::string &st)
{
  if (v->drive_type != 0) { print_dir_ending(i, st); }
  if (v->delay == 1) { st += "_delay"; }
  if (v->forced == 1) { st += "_forced"; }

  return;
}

void print_vars (node *n, std::string &vs)
{
  char buf[1024];

  int cps = 1;

  for (auto v : n->decl) {
    if (v.second->type == 0) { continue; }
    if (v.second->drive_type != 0) { cps = 4; }
    else { cps = 1; }
    for (auto i = 0; i < cps; i++) {
      if (v.second->type == 1) { vs += "wire \\"; } 
      else if (v.second->type == 2) { vs += "reg \\"; }
      v.first->toid()->sPrint(buf,1024);
      vs += buf;
      print_var_postfix(v.second, i, vs);
      vs += " ;\n";
    }
  }

  return;
}

void print_module_port (port *p, int &cnt, std::string &mh)
{
  char buf[1024];

  int cps = 1;

  if (p->drive_type != 0) { cps = 4; }

  p->c->toid()->sPrint(buf,1024);

  if (p->dir == 0) {
    for (auto i = 0; i < cps; i++) {
      mh += "\t,output\t";
      if (p->wire == 0) { mh += "reg"; }
      mh += "\t\t\\";
      mh += buf;
      if (p->u.p.n->proc) { print_bi_dir(p, mh); }
      print_port_postfix(p, i, mh);
      mh += "\n";
    }
  } else {
    for (auto i = 0; i < cps; i++) {
      mh += "\t,input\t\t\t\\";
      mh += buf;
      if (p->u.p.n->proc) { print_bi_dir(p, mh); }
      print_port_postfix(p, i, mh);
      if (p->u.p.n->extra_node != 0 && p->u.p.n->proc == NULL) { 
        mh += "_";
        mh += std::to_string(cnt); 
      }
      mh += "\n";
    }
    cnt++;
  }

  return;
}

void print_module_ports (node *n, std::string &mh)
{
  char buf[1024];
  int cnt = 0;

  mh += "\t input\t\t\t\\clock\n";
  for (auto gp : n->gp) {
    mh += "\t,input\t\t\t\\";
    gp->c->toid()->sPrint(buf, 1024);
    mh += buf;
    mh += "\n";
  }

  int idr_num = 0;
  for (auto p : n->p) {
    print_module_port(p, cnt, mh);
  }
  return;
}

void print_module_header (node *n, std::string &mh)
{
  if (n->extra_node == 0) {
    mh += "module \\";
    mh += get_module_name(n->proc);
    mh += " (\n";
  } else if (n->extra_node != 0 && n->copy == 1) {
    mh += "module \\";
    mh += get_module_name(n->proc);
    mh += "_";
    mh += std::to_string(n->extra_node);
    mh += " (\n";
  } else {
    mh += "module \\md_mux_";
    mh += std::to_string(n->extra_node);
    mh += " (\n";
  }
  print_module_ports(n,mh);
  mh += ");\n";

  return;
}

void print_verilog (project *proj, FILE *output) {

  graph *g = proj->g;

  int ff_flag = 0;
  int flag_arb_hi = 0;
  int flag_arb_lo = 0;

  if (proj->need_delay != 0) { ff_flag = 1; } 
  else if (proj->need_hi_arb == 1) { flag_arb_hi = 1; } 
  else if (proj->need_lo_arb == 1) { flag_arb_lo = 1; }

  std::string ff, arbh, arbl;
  if (ff_flag == 1) { print_ff(proj->need_delay, ff); fprintf(output, "%s", ff.c_str());}
  if (flag_arb_hi == 1) { print_fair_hi(arbh); fprintf(output, "%s", arbh.c_str());}
  if (flag_arb_lo == 1) { print_fair_lo(arbl); fprintf(output, "%s", arbl.c_str());}

  std::string tmp;

  for (auto n = g->hd; n; n = n->next) {

    char tmp1[1024];
    char tmp2[1024];

    int wire = 0;

    delayed_ports.clear();

    Scope *cs = NULL;
    if (n->proc) {
      cs = n->proc->CurScope();
    }

    std::string mn = "";
    std::string gate_body = "";
    std::string instance = "";
    std::string info = "";
    std::string module_head = "";
    std::string delay = "";
    std::string vars = "";

    print_module_header(n, module_head);
    fprintf(output, "%s", module_head.c_str());
    module_head.clear();

    info += "/*----------REGSandWIRES----------*/\n";
    fprintf(output, "%s", info.c_str());
    info.clear();
    print_vars(n, vars);
    fprintf(output, "%s", vars.c_str());
    vars.clear();
    info += "/*----------BODY----------*/\n";
    fprintf(output, "%s", info.c_str());
    info.clear();
    int prs_cnt = 0;
    if (n->gh) {
      for (auto gn = n->gh; gn; gn = gn->next) {
        for (auto gp : gn->p) {
          if (gp->delay > 0) {
            delayed_ports.push_back(gp);
          }
        }
      }
      for (auto gn = n->gh; gn; gn = gn->next) {
        if (gn->drive_type == 0 || gn->drive_type == 1) {
          print_gate(cs, n, gn, gate_body);
          fprintf(output, "%s\n", gate_body.c_str());
        } else if (gn->drive_type == 2 || gn->drive_type == 3) {
          print_mux(gn, gate_body);
          fprintf(output, "%s\n", gate_body.c_str());
        }
        gate_body.clear();
      }
    }
    info += "/*----------INSTANCES----------*/\n";
    fprintf(output, "%s", info.c_str());
    info.clear();
    for (auto in = n->cgh; in; in = in->next) {
      print_inst (in, instance);
      fprintf(output, "%s", instance.c_str());
      instance.clear();
    }
    info += "/*----------ARBITERS----------*/\n";
    fprintf(output, "%s", info.c_str());
    info.clear();
    for (auto pair : n->excl) {
      if (pair.first == 0) {
        inst_arb(0, pair.second, output);
      } else {
        inst_arb(1, pair.second, output);
      }
    }
    info += "/*----------DELAYS----------*/\n";
    fprintf(output, "%s", info.c_str());
    info.clear();
    for (auto dp : delayed_ports) {
      print_delay(dp, delay);
      fprintf(output, "%s",delay.c_str());
      delay.clear();
    }
    module_head += "endmodule\n\n";
    fprintf(output, "%s", module_head.c_str());
    module_head.clear();
  }
}

}
