CC=gcc
CFLAGS=-Wall -Wextra -O2 -pthread
SRC_DIR=src
BIN_DIR=bin

UNSYNC=$(BIN_DIR)/unsync_matchmaking
SYNC=$(BIN_DIR)/sync_matchmaking

.PHONY: all unsync sync clean run-unsync run-sync stress run-demo-12

all: unsync sync

unsync: $(UNSYNC)

sync: $(SYNC)

$(UNSYNC): $(SRC_DIR)/unsync_matchmaking.c $(SRC_DIR)/common.h
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC_DIR)/unsync_matchmaking.c -o $(UNSYNC)

$(SYNC): $(SRC_DIR)/sync_matchmaking.c $(SRC_DIR)/common.h
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC_DIR)/sync_matchmaking.c -o $(SYNC)

run-unsync: unsync
	bash scripts/run_unsync.sh

run-sync: sync
	bash scripts/run_sync.sh

stress: all
	bash scripts/run_stress.sh

run-demo-12: all
	bash scripts/run_demo_12.sh

clean:
	rm -rf $(BIN_DIR)/*.o $(UNSYNC) $(SYNC)
	rm -f logs/*.log results/*.txt
