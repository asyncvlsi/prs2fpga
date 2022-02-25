/*******************************************************************************
 *
 * Copyright (c) 2022 Ruslan Dashkin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/

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
