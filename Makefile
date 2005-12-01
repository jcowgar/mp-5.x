mp-4: mp-4.c mpdm/libmpdm.a mpsl/libmpsl.a
	$(CC) -Wall -g $< -Impdm -Lmpdm -Impsl -Lmpsl `cat mpdm/config.ldflags` `cat mpsl/config.ldflags` -lmpsl -lmpdm -lncursesw -o $@

clean:
	rm -f mp-4
