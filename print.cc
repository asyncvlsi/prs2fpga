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

std::string get_module_name (Process *p) {

  const char *mn = p->getName();
  int len = strlen(mn);
  std::string buf;

  for (auto i = 0; i < len; i++) {
    if (mn[i] == 0x3c) {
      continue;
    } else if (mn[i] == 0x3e) {
      continue;
    } else if (mn[i] == 0x2c) {
			buf += "_";
    } else {
      buf += mn[i];
    }
  }

  return buf;

}

static void prefix_id_print (Scope *cs, ActId *id, FILE *output, int dir) {

  if (cs->Lookup (id, 0)) {
    /*prefix_print ();*/
  }
  else {
    ValueIdx *vx;
    /* must be a global */
    vx = cs->FullLookupVal (id->getName());
    Assert (vx, "Hmm.");
    Assert (vx->global, "Hmm");
    if (vx->global == ActNamespace::Global()) {
      /* nothing to do */
    }
    else {
      char *tmp = vx->global->Name ();
      fprintf (output, "%s::", tmp);
      FREE (tmp);
    }
  }
  act_connection *c = id->Canonical(cs);
  ActId *pid = c->toid();
  char buf[10240];
  pid->sPrint(buf, 10240);
  fprintf (output, "\\");
  pid->Print(output);
  fprintf (output, " ");
  delete pid;
}


#define PREC_BEGIN(myprec, output)			\
  do {						\
    if ((myprec) < prec) {			\
      fprintf (output, "(");				\
    }						\
  } while (0)

#define PREC_END(myprec, output)			\
  do {						\
    if ((myprec) < prec) {			\
      fprintf (output, " )");				\
    }						\
  } while (0)

#define EMIT_BIN(myprec,sym,output,dir)			\
  do {						\
    PREC_BEGIN(myprec, output);				\
    _print_prs_expr (s, e->u.e.l, (myprec), flip,output, dir);	\
    fprintf (output,"%s", (sym));			\
    _print_prs_expr (s, e->u.e.r, (myprec), flip,output, dir);	\
    PREC_END (myprec, output);				\
  } while (0)

#define EMIT_UNOP(myprec,sym,output,dir)			\
  do {						\
    PREC_BEGIN(myprec, output);				\
    fprintf (output, "%s", sym);				\
    _print_prs_expr (s, e->u.e.l, (myprec), flip,output, dir);	\
    PREC_END (myprec, output);				\
  } while (0)

/*
  3 = ~
  2 = &
  1 = |
*/
static void _print_prs_expr (Scope *s, act_prs_expr_t *e, int prec, int flip, FILE *output, int dir)
{
  hash_bucket_t *b;
  act_prs_lang_t *pl;
  
  if (!e) return;
  switch (e->type) {
  case ACT_PRS_EXPR_AND:
    EMIT_BIN(2, "&",output,dir);
    break;
    
  case ACT_PRS_EXPR_OR:
    EMIT_BIN(1, "|",output,dir);
    break;
    
  case ACT_PRS_EXPR_VAR:
    if (flip) {
      fprintf (output, "~");
    }
    prefix_id_print (s, e->u.v.id, output, dir);
    break;
    
  case ACT_PRS_EXPR_NOT:
    EMIT_UNOP(3, "~", output,dir);
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
      fprintf (output, "~");
    }
    fprintf (output, "(");
    _print_prs_expr (s, pl->u.one.e, 0, flip, output, dir);
    fprintf (output,")");
    break;

  case ACT_PRS_EXPR_TRUE:
    fprintf (output, "true");
    break;
    
  case ACT_PRS_EXPR_FALSE:
    fprintf (output, "false");
    break;

  default:
    fatal_error ("What?");
    break;
  }
}

void print_prs (Scope *s, act_prs_lang_t *prs, int dir, FILE *output) {
	if (prs->u.one.arrow_type == 0) {
		_print_prs_expr(s, prs->u.one.e, 0, 0, output, dir);
	} else if (prs->u.one.arrow_type == 1) {
    if (dir == 1) {
  		fprintf(output, "~(");
    } else {
  		fprintf(output, "(");
    }
		_print_prs_expr(s, prs->u.one.e, 0, 0, output, dir);
    fprintf(output, " )");
	} else if (prs->u.one.arrow_type == 2) {
		fprintf(output, "(");
		_print_prs_expr(s, prs->u.one.e, 0, dir, output, dir);
    fprintf(output, " )");
	}
}

void print_gate (Scope *cs, std::vector<act_prs_lang_t *> &netw, int dir, FILE *output){
  int prs_cnt = 0;
  prs_cnt = netw.size();
  for(auto br : netw) {
    if (br) {
      print_prs (cs, br, dir, output);
      prs_cnt--;
      if (prs_cnt != 0) {
        fprintf(output, "|");
      } 
    } else {
      fprintf(output, "1'b0");
    }
  }
}

void print_dir_ending (int i, FILE *output) {
  if (i == 0) {
    fprintf(output, "_up");
  } else if (i == 1) {
    fprintf(output, "_dn");
  } else if (i == 2) {
    fprintf(output, "_w_up");
  } else if (i == 3) {
    fprintf(output, "_w_dn");
  }
}

void print_flag_ending (port *p, FILE *output) {
  unsigned int d = 0;

  if (p->delay != 0) {
    fprintf(output, "_delay");
    for (auto dp : delayed_ports) {
      if (dp == p) {
        d = 0;
        break;
      } else {
        d = 1;
      }
    }
    if (d == 1) {
      delayed_ports.push_back(p);
      d = 0;
    }
  }
  if (p->forced != 0) {
    fprintf(output, "_forced");
  }
}

void print_port (port *p, int func, FILE *output) {
  p->c->toid()->Print(output);
  if (p->drive_type != 0) {
    if (p->owner == 1 && (p->primary == 0 || p->u.i.in->extra_inst != 0)) {
      ValueIdx *vx;
      vx = p->u.i.in->inst_name;
			if (vx) {
				fprintf(output, "_%s", vx->getName());
				if (p->u.i.in->array) {
					fprintf(output, "%s", p->u.i.in->array);
				}
			}
		}
  } else if (p->owner == 2 && p->dir == 1) {
    fprintf(output, "_gate");
  }
  if (func != 0) {
    print_dir_ending(func - 1, output);
  }
  print_flag_ending(p, output);
}

void print_ff (FILE *output) {
  fprintf(output, "`timescale 1ns/1ps\n\n");
  fprintf(output, "module \\ff_delay #(\n");
  fprintf(output, "\tparameter\tDELAY\t=\t1\n");
  fprintf(output, ")(\n");
  fprintf(output, "\tinput\t\tclock\n");
  fprintf(output, ",\tinput\t\treset\n");
  fprintf(output, ",\tinput\t\tin\n");
  fprintf(output, ",\toutput\t\tout\n");
  fprintf(output, ");\n");
  fprintf(output, "\n\n");
  fprintf(output, "reg\t[DELAY-1:0]\tout_d = 0;\n\n");

  fprintf(output, "assign out = out_d[DELAY-1];\n\n");

  fprintf(output, "always @(posedge clock)\n");
  fprintf(output, "if (\\reset )\tout_d[0]\t<=\t1'b0;\n");
  fprintf(output, "else\t\t\tout_d[0] <= in;\n\n");

  fprintf(output, "genvar i;\n");
  fprintf(output, "generate\n");
  fprintf(output, "for (i = 1; i < DELAY; i = i + 1) begin\n");
  fprintf(output, "\talways @(posedge clock)\n");
  fprintf(output, "\t\tout_d[i] <= out_d[i-1];\n");
  fprintf(output, "end\n");
  fprintf(output, "endgenerate\n\n");
  fprintf(output, "endmodule\n\n");
}

void print_delay (port *p, FILE *output) {
  fprintf(output, "ff_delay #(.DELAY(%i))\n", p->delay);
  fprintf(output, "ffd_%i\n (",ffd_num);
  fprintf(output, ".in(\\");
  p->c->toid()->Print(output);
  fprintf(output, "_delay ),\n");
  fprintf(output, ".out(\\");
  p->c->toid()->Print(output);
  fprintf(output, " );\n");
  ffd_num++;
}

void print_arb_lo(FILE *output) {
  fprintf(output, "module \\arb_lo #(\n");
  fprintf(output, "\tparameter\tARB_NUM\t=\t2\n");
  fprintf(output, ")(\n");
  fprintf(output, "\tinput\t\t[ARB_NUM-1:0]\tin,\n");
  fprintf(output, "\toutput\treg\t[ARB_NUM-1:0]\tout\n");
  fprintf(output, ");\n\n");
  fprintf(output, "genvar i;\n");
  fprintf(output, "generate\n");
  fprintf(output, "\tfor (i = 0; i < ARB_NUM-1; i = i + 1) begin\n");
  fprintf(output, "\t\talways @ (*)\n");
  fprintf(output, "\t\tif (!in[i] & &(in[ARB_NUM-1:i+1])) out[i] <= 1'b0;\n");
  fprintf(output, "\t\telse\tout[i] <= 1'b1;\n");
  fprintf(output, "\tend\n");
  fprintf(output, "endgenerate\n\n");
  fprintf(output, "always @ (*)\n");
  fprintf(output, "if (!in[ARB_NUM-1])\tout[ARB_NUM-1]\t<= 1'b0;\n");
  fprintf(output, "else\tout[ARB_NUM-1]\t<= 1'b1;\n\n");
  fprintf(output, "endmodule\n\n");
}

void print_arb_hi(FILE *output) {
  fprintf(output, "module \\arb_hi #(\n");
  fprintf(output, "\tparameter\tARB_NUM\t=\t2\n");
  fprintf(output, ")(\n");
  fprintf(output, "\tinput\t\t[ARB_NUM-1:0]\tin,\n");
  fprintf(output, "\toutput\treg\t[ARB_NUM-1:0]\tout\n");
  fprintf(output, ");\n\n");
  fprintf(output, "genvar i;\n");
  fprintf(output, "generate\n");
  fprintf(output, "\tfor (i = 0; i < ARB_NUM-1; i = i + 1) begin\n");
  fprintf(output, "\t\talways @ (*)\n");
  fprintf(output, "\t\tif (in[i] & &(!in[ARB_NUM-1:i+1])) out[i] <= 1'b1;\n");
  fprintf(output, "\t\telse\tout[i] <= 1'b0;\n");
  fprintf(output, "\tend\n");
  fprintf(output, "endgenerate\n\n");
  fprintf(output, "always @ (*)\n");
  fprintf(output, "if (in[ARB_NUM-1])\tout[ARB_NUM-1]\t<= 1'b1;\n");
  fprintf(output, "else\tout[ARB_NUM-1]\t<= 1'b0;\n\n");
  fprintf(output, "endmodule\n\n");
}

void print_fair_hi(FILE *output){
fprintf(output, "module fair_hi #(\n");
fprintf(output, "\tparameter   WIDTH = 8\n");
fprintf(output, ")(\n");
fprintf(output, "\t\tinput   clock,\n");
fprintf(output, "\t\tinput   reset,\n");
fprintf(output, "\t\tinput   [WIDTH-1:   0]  req,\n");
fprintf(output, "\t\toutput  [WIDTH-1:   0]  grant\n");
fprintf(output, "\t);\n");
fprintf(output, "\t\n");
fprintf(output, "reg\t[WIDTH-1:0]\tpriority [0:WIDTH-1];\n");
fprintf(output, "\n");
fprintf(output, "reg\t[WIDTH-1:0]\treq_d;\n");
fprintf(output, "wire\t[WIDTH-1:0]\treq_neg;\n");
fprintf(output, "\n");
fprintf(output, "wire shift_prio;\n");
fprintf(output, "\n");
fprintf(output, "wire [WIDTH-1:0] match;\n");
fprintf(output, "reg [WIDTH-1:0] arb_match;\n");
fprintf(output, "\n");
fprintf(output, "always @(posedge clock)\n");
fprintf(output, "if (reset)\n");
fprintf(output, "\treq_d <=  0;\n");
fprintf(output, "else\n");
fprintf(output, "\treq_d <= req;\n");
fprintf(output, "\n");
fprintf(output, "assign shift_prio = |(req_neg & priority[0]) & !(|arb_match);\n");
fprintf(output, "\t\n");
fprintf(output, "genvar j;\n");
fprintf(output, "genvar i;\n");
fprintf(output, "\n");
fprintf(output, "generate\n");
fprintf(output, "for (j = WIDTH; j > 0; j = j-1) begin    \n");
fprintf(output, "\n");
fprintf(output, "always @ (*)\n");
fprintf(output, "if (j > 1)\n");
fprintf(output, "\tif (match[j-1] & &(!match[j-2:0]))\tarb_match[j-1] <= 1'b1;\n");
fprintf(output, "\telse\t\t\t\t\t\t\t\tarb_match[j-1] <= 1'b0;\n");
fprintf(output, "else\n");
fprintf(output, "\tif (match[j-1])\tarb_match[j-1]	<= 1'b1;\n");
fprintf(output, "\telse\t\t\tarb_match[j-1]	<= 1'b0;\n");
fprintf(output, "end\n");
fprintf(output, "endgenerate\n");
fprintf(output, "\n");
fprintf(output, "generate\n");
fprintf(output, "for (j = 0; j < WIDTH; j = j+1) begin\n");
fprintf(output, "\n");
fprintf(output, "assign req_neg[j] = !req[j] & req_d[j];\n");
fprintf(output, "\n");
fprintf(output, "assign grant = req & priority[0];\n");
fprintf(output, "\n");
fprintf(output, "assign match[j] = |(req & priority[j]);\n");
fprintf(output, "\n");
fprintf(output, "always @(posedge clock)\n");
fprintf(output, "if (reset)\n");
fprintf(output, "\tpriority[j] <= {{j{1'b0}},1'b1,{(WIDTH-1-j){1'b0}}};\n");
fprintf(output, "else if (shift_prio)\n");
fprintf(output, "\tif (priority[j] == 1)\n");
fprintf(output, "\t\tpriority[j] <= {1'b1, {(WIDTH-1){1'b0}}};\n");
fprintf(output, "\telse\n");
fprintf(output, "\t\tpriority[j] <= priority[j] >> 1;\n");
fprintf(output, "else if (arb_match[j]) begin\n");
fprintf(output, "\tpriority[0] <= priority[j];\n");
fprintf(output, "\tpriority[j] <= priority[0];\n");
fprintf(output, "end\n");
fprintf(output, "\t\n");
fprintf(output, "end\n");
fprintf(output, "endgenerate\n");
fprintf(output, "\t\n");
fprintf(output, "endmodule\n");
}

void print_fair_lo(FILE *output){
fprintf(output, "module fair_lo #(\n");
fprintf(output, "\tparameter   WIDTH = 8\n");
fprintf(output, ")(\n");
fprintf(output, "\t\tinput   clock,\n");
fprintf(output, "\t\tinput   reset,\n");
fprintf(output, "\t\tinput   [WIDTH-1:   0]  req,\n");
fprintf(output, "\t\toutput  [WIDTH-1:   0]  grant\n");
fprintf(output, "\t);\n");
fprintf(output, "\t\n");
fprintf(output, "reg [WIDTH-1:0] priority [0:WIDTH-1];\n");
fprintf(output, "\n");
fprintf(output, "reg  [WIDTH-1:0] req_d;\n");
fprintf(output, "wire [WIDTH-1:0] req_pos;\n");
fprintf(output, "\n");
fprintf(output, "wire shift_prio;\n");
fprintf(output, "\n");
fprintf(output, "wire [WIDTH-1:0] match;\n");
fprintf(output, "reg [WIDTH-1:0] arb_match;\n");
fprintf(output, "\n");
fprintf(output, "\n");
fprintf(output, "always @(posedge clock)\n");
fprintf(output, "if (reset)\n");
fprintf(output, "\treq_d <=  {WIDTH{1'b1}};\n");
fprintf(output, "else\n");
fprintf(output, "\treq_d <= req;\n");
fprintf(output, "\n");
fprintf(output, "assign shift_prio = |(req_pos & priority[0]) & !(|arb_match);\n");
fprintf(output, "\t\n");
fprintf(output, "genvar j;\n");
fprintf(output, "genvar i;\n");
fprintf(output, "\n");
fprintf(output, "generate\n");
fprintf(output, "for (j = WIDTH; j > 0; j = j-1) begin    \n");
fprintf(output, "\n");
fprintf(output, "always @ (*)\n");
fprintf(output, "if (j > 1)\n");
fprintf(output, "\tif (match[j-1] & &(~match[j-2:0]))\tarb_match[j-1] <= 1'b1;\n");
fprintf(output, "\telse\t\t\t\t\t\t\t\tarb_match[j-1] <= 1'b0;\n");
fprintf(output, "else\n");
fprintf(output, "\tif (match[j-1])\tarb_match[j-1]	<= 1'b1;\n");
fprintf(output, "\telse\t\t\tarb_match[j-1]	<= 1'b0;\n");
fprintf(output, "end\n");
fprintf(output, "endgenerate\n");
fprintf(output, "\n");
fprintf(output, "generate\n");
fprintf(output, "for (j = 0; j < WIDTH; j = j+1) begin\n");
fprintf(output, "\n");
fprintf(output, "assign req_pos[j] = req[j] & !req_d[j];\n");
fprintf(output, "\n");
fprintf(output, "assign grant = ~(~req & priority[0]);\n");
fprintf(output, "\n");
fprintf(output, "assign match[j] = |(~req & priority[j]);\n");
fprintf(output, "\n");
fprintf(output, "always @(posedge clock)\n");
fprintf(output, "if (reset)\n");
fprintf(output, "\tpriority[j] <= {{j{1'b0}},1'b1,{(WIDTH-1-j){1'b0}}};\n");
fprintf(output, "else if (shift_prio)\n");
fprintf(output, "\tif (priority[j] == 1)\n");
fprintf(output, "\t\tpriority[j] <= {1'b1, {(WIDTH-1){1'b0}}};\n");
fprintf(output, "\telse\n");
fprintf(output, "\t\tpriority[j] <= priority[j] >> 1;\n");
fprintf(output, "else if (arb_match[j]) begin\n");
fprintf(output, "\tpriority[0] <= priority[j];\n");
fprintf(output, "\tpriority[j] <= priority[0];\n");
fprintf(output, "end\n");
fprintf(output, "\t\n");
fprintf(output, "end\n");
fprintf(output, "endgenerate\n");
fprintf(output, "\t\n");
fprintf(output, "endmodule\n");
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

void print_verilog (graph *g, FILE *output) {

  int ff_flag = 0;
  int flag_arb_hi = 0;
  int flag_arb_lo = 0;

  for (auto n = g->hd; n; n = n->next) {
    act_spec *sp = n->spec;
    while (sp) {
      if (ACT_SPEC_ISTIMING (sp)) {
        ff_flag = 1;
      } else if (sp->type == 2) {
        flag_arb_hi = 1;
      } else if (sp->type == 3) {
        flag_arb_lo = 1;
      }
      sp = sp->next;
    }
  }
  if (ff_flag == 1) {
    print_ff(output);
  }
  if (flag_arb_hi == 1) {
    print_fair_hi(output);
  }
  if (flag_arb_lo == 1) {
    print_fair_lo(output);
  }

  for (auto n = g->hd; n; n = n->next) {

    char tmp1[1024];
    char tmp2[1024];

    int wire = 0;

    delayed_ports.clear();

    Scope *cs = NULL;
    if (n->proc) {
      cs = n->proc->CurScope();
    }

    std::string mn;

    if (n->extra_node == 0) {
      mn = get_module_name(n->proc);
      fprintf(output, "module \\%s (\n", mn.c_str());
    } else if (n->extra_node != 0 && n->copy == 1) {
      mn = get_module_name(n->proc);
      fprintf(output, "module \\%s_%u (\n", mn.c_str(), n->extra_node);
    } else {
      fprintf(output, "module \\md_mux_%u (\n", n->extra_node);
    }
    fprintf(output, "\t input\t\t\t\\clock\n");
    for (auto gp : n->gp) {
      fprintf(output,"\t,input\t\t\t\\");
      gp->c->toid()->Print(output);
      fprintf(output, " \n");
    }
    int idr_num = 0;
    for (auto p : n->p) {
      if (p->dir == 0) {
        for (auto in = n->cgh; in; in = in->next) {
          if (in->proc == NULL){
            continue;
          }
          for (auto pp : in->p) {
            if (pp->c == p->c && pp->dir == 0) {
              wire = 1;
            }
          }
        }
        if (p->drive_type == 1) {
          for (auto i = 0; i < 4; i++) {
            if (wire == 0) {
              fprintf(output, "\t,output\treg\t\t\\");
            } else {
              fprintf(output, "\t,output\t\t\t\\");
            }
            p->c->toid()->Print(output);
            print_dir_ending(i, output);
            print_flag_ending(p, output);
            fprintf(output, "\n");
          }
          wire = 0;
        } else {
          if (wire == 0) {
            fprintf(output, "\t,output\treg\t\t\\");
          } else {
            fprintf(output, "\t,output\t\t\t\\");
            wire = 0;
          }
          p->c->toid()->Print(output);
        }
      } else if (p->drive_type == 2) {
      } else {
        if (p->drive_type != 0) {
          for (auto i = 0; i < 4; i++) {
            fprintf(output, "\t,input\t\t\t\\");
            p->c->toid()->Print(output);
            print_dir_ending(i, output);
            print_flag_ending(p, output);
            fprintf(output, "_%i", idr_num);
            fprintf(output, "\n");
          }
          idr_num++;
        } else {
          fprintf(output, "\t,input\t\t\t\\");
          p->c->toid()->Print(output);
          print_flag_ending(p, output);
        }
      }
      fprintf(output, "\n");
    }
    fprintf(output, ");\n");

    fprintf(output, "/*----------REGSandWIRES----------*/\n");
		if (n->extra_node == 0 || n->extra_node != 0 && n->copy == 1) {
    std::map<port *, int> reg_intercon;
    std::vector<port *> wire_intercon;
    int decl = 1;
    for (auto gn = n->gh; gn; gn = gn->next) {
      gn->id->sPrint(tmp1, 1024);
      for (auto np : n->p) {
        np->c->toid()->sPrint(tmp2, 1024);
        if (strcmp(tmp1, tmp2) == 0) {
          decl = 0;
          break;
        } else {
          decl = 1;
        }
      }
      if (decl == 1) {
        reg_intercon[gn->p[0]] = gn->type;
      }
    }
    for (auto pair : reg_intercon) {
      fprintf(output, "reg  \\");
      pair.first->c->toid()->Print(output);
      if (pair.first->delay != 0) {
        fprintf(output, "_delay");
      }
      if (pair.first->forced != 0) {
        fprintf(output, "_forced");
      }
      fprintf(output, " = 1'b0 ;\n");
    }
    decl = 1;
    for (auto in = n->cgh; in; in = in->next) {
      for (auto ip : in->p) {
        if (ip->dir == 1) {
          continue;
        }
        ip->c->toid()->sPrint(tmp1, 1024);
        for (auto dp : wire_intercon) {
          dp->c->toid()->sPrint(tmp2, 1024);
          if (strcmp(tmp1, tmp2) == 0) {
            decl = 0;
            break;
          } else {
            decl = 1;
          }
        }
        if (decl == 1) {
          for (auto gn = n->gh; gn; gn = gn->next) {
            for (auto gp : gn->p) {
              if (gp->dir == 0) {
                continue;
              }
              gp->c->toid()->sPrint(tmp2, 1024);
              if (strcmp(tmp1, tmp2) == 0) {
                wire_intercon.push_back(gp);
              }
            }
          }
        }
      }
    }
    for (auto wp : wire_intercon) {
      fprintf(output, "wire \\");
      wp->c->toid()->Print(output);
      fprintf(output, " ;\n");
    }
		}
    fprintf(output, "/*----------BODY----------*/\n");
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
        if (gn->drive_type == 0) {
          if (gn->type == 2) {
            fprintf(output, "reg  \\");
            gn->id->Print(output);
            print_flag_ending(gn->p[0], output);
            fprintf(output, "_reg = 0 ;\n");
            fprintf(output, "always @(posedge clock)\n\t\\");
            gn->id->Print(output);
            print_flag_ending(gn->p[0], output);
            fprintf(output, "_reg <= \\");
            gn->id->Print(output);
            print_flag_ending(gn->p[0], output);
            fprintf(output, " ;\n\n"); 
          }
          if (gn->type == 0 || gn->type == 2) {
            fprintf(output, "always @(*)\n");
          } else  {
            fprintf(output, "always @(posedge \\clock )\n");
          }
          for (auto i = 0; i < 4; i++) {
            if (i == 0) {
              fprintf(output, "\tif (");
            } else {
              fprintf(output, "\telse if (");
            }
            if (i == 0) {
              print_gate(cs, gn->p_up,1, output);
            } else if (i == 1) {
              print_gate(cs, gn->p_dn,0, output);
            } else if (i == 2) {
              print_gate(cs, gn->w_p_up,1, output);
            } else if (i == 3) {
              print_gate(cs, gn->w_p_dn,0, output);
            }
            fprintf(output, " )\t\\");
            gn->id->Print(output);
            print_flag_ending(gn->p[0], output);
            if (i % 2 == 0) {
              fprintf(output, " <= 1'b1;\n");
            } else {
              fprintf(output, " <= 1'b0;\n");
            }
          }
          if (gn->type == 2) {
            fprintf(output, "\telse \\");
            gn->id->Print(output);
            print_flag_ending(gn->p[0], output);
            fprintf(output, " <= \\");
            gn->id->Print(output);
            print_flag_ending(gn->p[0], output);
            fprintf(output, "_reg ;");
          } else if (gn->type == 3) {
            fprintf(output, "\telse \\");
            gn->id->Print(output);
            print_flag_ending(gn->p[0], output);
            fprintf(output, " <= \\");
            gn->id->Print(output);
            print_flag_ending(gn->p[0], output);
            fprintf(output, " ;");
          }
          fprintf(output, "\n\n");
        } else if (gn->drive_type == 1) {
          for (auto i = 0; i < 4; i++) {
            if (gn->type == 0 || gn->type == 2) {
              fprintf(output, "always @ (*)\n");
            } else {
              fprintf(output, "always @ (posedge \\clock )\n");
            }
            fprintf(output, "\t\\");
            gn->id->Print(output);
            print_dir_ending(i, output);
            print_flag_ending(gn->p[0], output);
            fprintf(output, " <= ");
            if (i == 0) {
              print_gate(cs, gn->p_up,1, output);
            } else if (i == 1) {
              print_gate(cs, gn->p_dn,0, output);
            } else if (i == 2) {
              print_gate(cs, gn->w_p_up,1, output);
            } else if (i == 3) {
              print_gate(cs, gn->w_p_dn,0, output);
            }
            fprintf(output, ";");
            fprintf(output, "\n");
          }
          fprintf(output, "\n\n");
        } else if (gn->drive_type == 2) {
          fprintf(output, "always @ (*)\n");
          for (auto i = 0; i < 4; i++) {
            if (i == 0) {
              fprintf(output, "if (");
            } else { 
              fprintf(output, "else if (");
            }
            int dr_num = 0;
            for (auto pp : gn->extra_drivers) {
              fprintf(output, "\\");
              pp->c->toid()->Print(output);
              print_dir_ending(i, output);
              print_flag_ending(pp, output);
              fprintf(output, "_%i", dr_num);
              fprintf(output, " |");
              dr_num++;
            }
            fprintf(output, "1'b0) \\");
            gn->id->Print(output);
            if (i % 2 == 0) {
              fprintf(output, " <= 1'b1;\n");
            } else {
              fprintf(output, " <= 1'b0;\n");
            }
          }
          fprintf(output, "\n");
        }
      }
    }
    fprintf(output, "/*----------INSTANCES----------*/\n");
    for (auto in = n->cgh; in; in = in->next) {
      if (in->proc && in->n->copy == 0) {
        mn = get_module_name(in->proc);
        fprintf(output,"\\%s \\%s", 
                                  mn.c_str(), in->inst_name->getName());
      } else if (in->proc && in->n->copy == 1) {
        mn = get_module_name(in->proc);
        fprintf(output,"\\%s_%i \\%s", 
                                  mn.c_str(), in->n->extra_node, 
                                  in->inst_name->getName());
      } else {
        fprintf(output, "\\md_mux_%i \\md_mux_%i",
                                  in->extra_inst, in->extra_inst);
      }
			if (in->array) {
				fprintf(output, "%s", in->array);
			}
			fprintf(output, " (\n\t .\\clock (\\clock )\n");
      int jdr_num = 0;
      if (in->n) {
        for (auto gp : in->n->gp) {
          fprintf(output, "\t,.\\");
          gp->c->toid()->Print(output);
          fprintf(output, "\t(\\");
          gp->c->toid()->Print(output);
          fprintf(output, " )\n");
        }
      }
      for (auto i = 0; i < in->p.size(); i++) {
        if (in->p[i]->drive_type == 0) {
          fprintf(output, "\t,.\\");
          print_port(in->n->p[i], 0, output);
          fprintf(output, "\t(\\");
          print_port(in->p[i], 0, output);
          fprintf(output, " )\n");
        } else {
          for (auto j = 0; j < 4; j++) {
            fprintf(output, "\t,.\\");
            print_port(in->n->p[i], 0, output);
            print_dir_ending(j, output);
            if (in->n->copy == 0) {
              fprintf(output, "_%i", jdr_num);
            }
            fprintf(output, "\t(\\");
            print_port(in->p[i], j+1, output);
            fprintf(output, " )\n");
          }
          jdr_num++;
        }
      }
      fprintf(output, ");\n\n");
    }
    fprintf(output, "/*----------ARBITERS----------*/\n");
    for (auto pair : n->excl) {
      if (pair.first == 0) {
        inst_arb(0, pair.second, output);
      } else {
        inst_arb(1, pair.second, output);
      }
    }
    fprintf(output, "/*----------DELAYS----------*/\n");
    for (auto dp : delayed_ports) {
      print_delay(dp, output);
      fprintf(output, "\n");
    }
    fprintf(output, "endmodule\n\n");
  }
}

}
