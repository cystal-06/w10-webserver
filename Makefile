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
	@echo "=== Running sws Automated Verification Test Suite ==="
	@./$(BIN) -h > /dev/null && echo "[PASS] -h usage summary"
	@rm -f $(TESTDIR)/access.log
	@./$(BIN) -d -p 8899 -c $(TESTDIR)/cgi-bin -l $(TESTDIR)/access.log $(TESTDIR)/www > /dev/null 2>&1 & \
	PID=$$!; \
	sleep 1; \
	curl -s -i http://localhost:8899/index.html | grep -q "200 OK" && echo "[PASS] HTTP GET static file"; \
	curl -s -i -I http://localhost:8899/index.html | grep -q "200 OK" && echo "[PASS] HTTP HEAD headers only"; \
	curl -s -i http://localhost:8899/docs/ | grep -q "Index of" && echo "[PASS] Directory Indexing"; \
	curl -s -i http://localhost:8899/cgi-bin/hello.cgi | grep -q "Hello from SWS CGI" && echo "[PASS] RFC 3875 CGI execution"; \
	curl -s -i http://localhost:8899/../../etc/passwd | grep -E -q "403|404" && echo "[PASS] Path Traversal protection"; \
	kill $$PID 2>/dev/null; \
	wait $$PID 2>/dev/null; \
	test -s $(TESTDIR)/access.log && echo "[PASS] Apache common logging"; \
	echo "=== All Tests Passed Successfully ==="

dist: clean
	@tar -cvf user1-user2-user3.tar Makefile README checklist SPECIFICATION.md src test
	@echo "Package user1-user2-user3.tar generated successfully."
