FILES_TO_BACKUP = my402list.c warmup2.c
BACKUP_FNAME = warmup2-A-backup-`date +%d%b%Y-%H%M%S`.tar.gz
BACKUP_DIR = $(HOME)/Desktop

warmup2: warmup2.o my402list.o
	gcc -o warmup2 -g warmup2.o my402list.o -lpthread -lm

warmup2.o: warmup2.c my402list.h
	gcc -g -c -Wall warmup2.c

my402list.o: my402list.c my402list.h
	gcc -g -c -Wall my402list.c

clean:
	rm -f *.o warmup2

backup:
	tar cvzf $(BACKUP_FNAME) $(FILES_TO_BACKUP)
	@if [ -d $(BACKUP_DIR)/ ]; then \
		mv $(BACKUP_FNAME) $(BACKUP_DIR)/$(BACKUP_FNAME); \
		echo ; \
		echo "Backup file created in shared folder: $(BACKUP_DIR)/$(BACKUP_FNAME)"; \
		/bin/ls -l $(BACKUP_DIR)/$(BACKUP_FNAME); \
	else \
		echo ; \
		echo "$(BACKUP_DIR) inaccessible, local backup file created: $(BACKUP_FNAME)"; \
		/bin/ls -l $(BACKUP_FNAME); \
	fi
