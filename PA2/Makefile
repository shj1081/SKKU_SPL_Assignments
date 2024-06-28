# Compiler
CC=gcc
CFLAGS=-Wall -std=gnu11 -D_GNU_SOURCE
LDFLAGS=-lreadline -lhistory

# Directories
SHELLDIR=shell_src
EXECDIR=executable_src
BINDIR=bin

# Source files
SHELL_OBJS=$(wildcard $(SHELLDIR)/*.c)
EXEC_OBJS=$(wildcard $(EXECDIR)/*.c)

# Executables
EXECUTABLES=$(BINDIR)/pa2 $(EXEC_OBJS:$(EXECDIR)/%.c=$(BINDIR)/%)
EXECUTABLE_NAMES=$(EXEC_SRCS:$(EXECDIR)/%.c=%)

# Targets
all: $(BINDIR) $(EXECUTABLES)

$(BINDIR)/pa2: $(SHELL_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BINDIR)/%: $(EXECDIR)/%.c | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -f $(EXECUTABLES)
	rm -r $(BINDIR)

pa2: all