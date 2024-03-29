#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor,
#  Boston, MA  02110-1301, USA.
#
#-------------------------------------------------------------------------
EXE=prs2fpga.$(EXT)

TARGETS=$(EXE)
TARGETINCSUBDIR=act

OBJS=main.o build_graph.o graph.o arb.o timing.o print.o multidr.o debug.o

SRCS=$(OBJS:.o=.cc)

include $(ACT_HOME)/scripts/Makefile.std

$(EXE): $(OBJS) $(ACTPASSDEPEND) $(TARGETINCS)
	$(CXX) $(CFLAGS) $(OBJS) -o $(EXE) $(LIBACTPASS) -g

-include Makefile.deps
