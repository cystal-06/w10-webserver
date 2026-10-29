# ==============================================================================
# CS631 - Advanced Programming in the UNIX Environment
# Group Project: Simple Web Server (sws)
# Team: Nguyen Cong Duc (Leader) & Team Members
# Target Reference Platforms: NetBSD 11.0, Linux, OmniOS
# ==============================================================================

CC      ?= gcc
CFLAGS  += -Wall -Wextra -Werror -pedantic -std=c99 \
           -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -D_BSD_SOURCE
LDFLAGS += -lmagic

SRCDIR  := src
TESTDIR := test
BIN     := sws

SRCS    := $(SRCDIR)/main.c \
           $(SRCDIR)/network.c \
           $(SRCDIR)/http.c \
           $(SRCDIR)/resource.c \
           $(SRCDIR)/cgi.c \
           $(SRCDIR)/logger.c

OBJS    := $(SRCS:.c=.o)
HDRS    := $(SRCDIR)/sws.h \
           $(SRCDIR)/network.h \
           $(SRCDIR)/http.h \
           $(SRCDIR)/resource.h \
           $(SRCDIR)/cgi.h \
           $(SRCDIR)/logger.h

.PHONY: all clean test dist

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(BIN) $(TESTDIR)/access.log *.tar

test: all
	@echo "=== Running sws Verification Test Suite ==="
	@./$(BIN) -h
	@echo "Help test passed."

dist: clean
	@tar -cvf user1-user2-user3.tar Makefile README checklist SPECIFICATION.md src test
	@echo "Package user1-user2-user3.tar generated successfully."
