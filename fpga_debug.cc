#include <act/fpga_proto.h>
#include <act/fpga_debug.h>

namespace fpga {

void print_graph (graph *g, FILE *output){
  for (auto n = g->hd; n; n = n->next) {
    fprintf(output, "-------------------------------------------------\n");
    if (n->proc) {
      fprintf(output, "PROCESS NAME : %s\n", n->proc->getName());
    } else {
      fprintf(output, "MD CONTROL : \n");
    }
    fprintf(output, "PARAMS : visited(%i), extra(%i)\n", n->visited, n->extra_node);
    fprintf(output, "PORTS :");
    for (auto pp : n->p) {
      fprintf (output, "  ");
      pp->c->toid()->Print(output);
      fprintf(output, "(%i,%i,%i,%i,%i)", pp->dir, pp->drive_type,
                                          pp->delay, pp->forced,
                                          pp->owner);
    }
    fprintf(output, "\n");
    fprintf(output, "GLOBAL PORTS :");
    for (auto pp : n->gp) {
      fprintf (output, "  ");
      pp->c->toid()->Print(output);
      fprintf(output, "(%i,%i,%i,%i,%i)", pp->dir, pp->drive_type,
                                          pp->delay, pp->forced,
                                          pp->owner);
    }
    fprintf(output, "\n");
    fprintf(output, "WEIGHT: %i\n", n->weight);
    fprintf(output, "\n");
    fprintf(output, "IO MAP:\n");
    fprintf(output, "\n");
    fprintf(output, "MAX_IO:\n");
    for (auto pair : n->max_io_delay){
      fprintf(output, "\t%i -> %i == %i\n", pair.first.first, pair.first.second, pair.second);
    }
    fprintf(output, "MIN_IO:\n");
    for (auto pair : n->min_io_delay){
      fprintf(output, "\t%i -> %i == %i\n", pair.first.first, pair.first.second, pair.second);
    }
    fprintf(output, "\n");
    fprintf(output, "INSTANCES(%i) :\n", n->i_num);
    for (auto nn = n->cgh; nn; nn = nn->next) {
      fprintf(output, "\t");
      if (nn->extra_inst == 0) {
        fprintf(output, "%s", nn->inst_name->getName());
        fprintf(output, "(%s)", nn->n->proc->getName());
      } else {
        fprintf(output, "md_mux");
      }
      if (nn->array) {
        fprintf(output, "%s", nn->array);
      }
      fprintf(output, "(%i) :", nn->visited);
      for(auto pp : nn->p) {
        fprintf(output, "\t");
        pp->c->toid()->Print(output);
        fprintf(output, "(%i,%i,%i,%i,%i)", pp->dir, pp->drive_type,
                                            pp->delay, pp->forced,
                                            pp->owner);
      }
      fprintf(output, "\n");
    }
    fprintf(output, "GATES(%i) :\n", n->g_num);
    for (auto nn = n->gh; nn; nn = nn->next) {
      fprintf(output, "\t");
      fprintf(output, "%s", nn->id->getName());
      if (nn->extra_gate == 0) {
        fprintf(output,"(%i,%i,%i,%i,%i,%i,%i)", nn->type, nn->drive_type,
                                              nn->p_up.size(),nn->p_dn.size(),
                                              nn->w_p_up.size(),nn->w_p_dn.size(),
                                              nn->visited);
        fprintf(output, " :");
        for (auto pp : nn->p) {
          fprintf(output, "\t");
          pp->c->toid()->Print(output);
          fprintf(output, "(%i,%i,%i,%i,%i)", pp->dir, pp->drive_type,
                                              pp->delay, pp->forced,
                                              pp->owner);
        }
      }
      for (auto pp : nn->extra_drivers) {
        fprintf(output, "\t");
        pp->c->toid()->Print(output);
      }
      fprintf(output, "\n");
    }
  }
  fprintf(output, "-------------------------------------------------\n"); 
  fprintf(output, "-------------------------------------------------\n"); 
}

void print_cp (graph *g, FILE *output) {
  for (auto n = g->hd; n; n = n->next) {
    fprintf(output, "-------------------------------------------------\n"); 
    fprintf(output, "PROCESS NAME : %s\n", n->proc->getName());
    for (auto pair : n->cp) {
      pair.first->toid()->Print(output);
      fprintf(output, " :");
      for (auto el : pair.second) {
        fprintf(output, " ");
        el->c->toid()->Print(output);
        fprintf(output, "(%i,%i)",el->dir,el->owner);
      }
      fprintf(output, "\n");
    }
  }
  fprintf(output, "-------------------------------------------------\n"); 
  fprintf(output, "-------------------------------------------------\n"); 
}

void print_io (graph *g, FILE *output) {
  for (auto n = g->hd; n; n = n->next) {
    fprintf(output, "-------------------------------------------------\n"); 
    fprintf(output, "PROCESS NAME : %s\n", n->proc->getName());
    for (auto pair : n->io_map) {
      n->p[pair.first]->c->toid()->Print(output);
      fprintf(output, "(%i,%i)->  ", n->p[pair.first]->dir, n->p[pair.first]->owner);
      for (auto op : pair.second) {
        n->p[op]->c->toid()->Print(output);
        fprintf(output, "(%i,%i)  ", n->p[op]->dir, n->p[op]->owner);
      }
      fprintf(output, "\n");
    }
    for (auto gn = n->gh; gn; gn = gn->next) {
      for (auto gp : gn->io_map) {
        gn->p[gp.first]->c->toid()->Print(output);
        fprintf(output, "(%i,%i)-> ", gn->p[gp.first]->dir,gn->p[gp.first]->owner);
        gn->p[gp.second]->c->toid()->Print(output);
        fprintf(output, "(%i,%i); ", gn->p[gp.second]->dir,gn->p[gp.second]->owner);
      }
      fprintf(output, "\n");
    }
  }
  fprintf(output, "-------------------------------------------------\n"); 
  fprintf(output, "-------------------------------------------------\n"); 
}

void print_owner (port *p, FILE *output) {
  fprintf(output, "OWNER -> ");
  if (p->owner == 0) {
    fprintf(output, "%s\n", p->u.p.n->proc->getName());
  } else if (p->owner == 1) {
    fprintf(output, "%s\n", p->u.i.in->n->proc->getName());
  } else if (p->owner == 2) {
    p->u.g.g->id->Print(output);
    fprintf(output, "\n");
  }
}

void print_arb (graph *g, FILE *output) {
  for (auto n = g->hd; n; n = n->next) {
    for (auto group : n->excl) {
      fprintf(output, "%i : ", group.first);
      for (auto elem : group.second) {
        elem->c->toid()->Print(output);
        fprintf(output, "(%i)   ", elem->forced);
      }
      fprintf(output, "\n");
    }
  }
}

void print_path (std::vector<port *> &path, FILE *output) {
  for (auto pp : path) {
    if (pp->owner == 0) {
      fprintf(output, "%s", pp->u.p.n->proc->getName());
    } else if (pp->owner == 1) {
      fprintf(output, "%s", pp->u.i.in->inst_name->getName());
    } else if (pp->owner == 2) {
      fprintf(output, "%s(%i)", pp->u.g.g->id->getName(), pp->u.g.g->type);
    }
    fprintf(output, "(");
    pp->c->toid()->Print(output);
    fprintf(output, ")->");
  }
  fprintf(output, "\n");
}

void print_node (node *n, FILE *output) {
  if (n->proc) {
    fprintf(output, "PROCESS: %s\n", n->proc->getName());
  } else {
    fprintf(output, "MD MUX (%i): \n", n->extra_node);
  }

  fprintf(output, "VISITED: %i\n", n->visited);
  fprintf(output, "EXTRA NODE: %i\n", n->extra_node);
  fprintf(output, "PORTS: \n");
  for (auto pp : n->p) {
    fprintf(output, "\t");
    pp->c->toid()->Print(output);
    fprintf(output, "\n");
  }
  if (n->spec) {
    fprintf(output, "SPEC: exist\n");
  } else {
    fprintf(output, "SPEC: no\n");
  }
  fprintf(output, "INST NUM: %i\n", n->i_num);
  for (auto in = n->cgh; in; in = in->next) {
    if (in->inst_name) {
      fprintf(output, "\t%s:", in->inst_name->getName());
    } else {
      fprintf(output, "\tmd_mux(%i)", in->extra_inst);
    }
    for (auto pin : in->p) {
      fprintf(output, "\t");
      pin->c->toid()->Print(output);
    }
    fprintf(output, "\n");
  }
  fprintf(output, "GATE NUM: %i\n", n->g_num);
  fprintf(output , "CONNECTION MAP: \n");
  for (auto pair : n->cp) {
    fprintf(output, "\t");
    pair.first->toid()->Print(output);
    fprintf(output, ":");
    for (auto pp : pair.second) {
      fprintf(output, "\t");
      pp->c->toid()->Print(output);
    }
    fprintf(output, "\n");
  }
}

void print_gate (gate *g, FILE *output) {
  fprintf(output, "GATE: ");
  g->id->Print(output);
  fprintf(output, "\nTYPE: %i\n", g->type);
  fprintf(output, "DRIVE TYPE: %i\n", g->drive_type);
  fprintf(output, "VISITED: %i\n", g->visited);
  fprintf(output, "EXTRA: %i\n", g->extra_gate);
  fprintf(output, "PORTS: \n");
  for (auto pp : g->p) {
    fprintf(output, "\t");
    pp->c->toid()->Print(output);
    fprintf(output, "\n");
  }
  fprintf(output, "IO_MAP: \n");
  for (auto pair : g->io_map) {
    fprintf(output, "\t%i => %i", pair.first, pair.second);
  }
  fprintf(output, "\n");
  fprintf(output, "EXTRA DRIVERS(%i): \n", g->extra_drivers.size());
  for (auto pp : g->extra_drivers) {
    fprintf(output, "\t");
    pp->c->toid()->Print(output);
    fprintf(output, "\n");
  }
  fprintf(output, "====================================\n");
}

void print_config (fpga_config *fc, FILE *output) {
  fprintf(output, "VENDOR: %i\n", fc->vendor);
  fprintf(output, "CHIP: %s\n", fc->chip.c_str());
  fprintf(output, "LUT: %i\n", fc->lut);
  fprintf(output, "FF: %i\n", fc->ff);
  fprintf(output, "DSP: %i\n", fc->dsp);
  fprintf(output, "CHIP NUM: %i\n", fc->num);
  fprintf(output, "FREQ: %i\n", fc->freq);
  fprintf(output, "PINS: %i\n", fc->pins);
  fprintf(output, "INTF: ");
  if (fc->intf == 0) {
    fprintf(output, "NON\n");
  } else if (fc->intf == 1) {
    fprintf(output, "TileLink\n");
  } else if (fc->intf == 2) {
    fprintf(output, "AXI\n");
  } else if (fc->intf == 3) {
    fprintf(output, "UART\n");
  }
  fprintf(output, "INC TCL: ");
  for (auto path : fc->inct) {
    fprintf(output, "%s;", path.c_str());
  }
  fprintf(output, "\n");
  fprintf(output, "INC XDC: ");
  for (auto path : fc->incx) {
    fprintf(output, "%s;", path.c_str());
  }
  fprintf(output, "\n");
  fprintf(output, "OPT: %i\n", fc->opt);
}

}
