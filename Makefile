mp-4: mp-4.c
	$(CC) -g $< -Impdm -Lmpdm -Impsl -Lmpsl `cat mpsl/config.ldflags` -lmpsl -lmpdm -lncursesw -o $@

clean:
	rm -f mp-4
