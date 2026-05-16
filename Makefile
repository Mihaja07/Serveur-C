CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lpthread

# Toutes les cibles
all: serveur_fork serveur_threads serveur_select

# Version avec fork()
serveur_fork: serveur_fork.c
	$(CC) $(CFLAGS) -o $@ $<

# Version avec threads
serveur_threads: serveur_threads.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

# Version avec select()
serveur_select: serveur_select.c
	$(CC) $(CFLAGS) -o $@ $<

# Nettoyer les executables
clean:
	rm -f serveur_fork serveur_threads serveur_select

# Compiler et executer une version
run_fork: serveur_fork
	./serveur_fork

run_threads: serveur_threads
	./serveur_threads

run_select: serveur_select
	./serveur_select

.PHONY: all clean run_fork run_threads run_select
