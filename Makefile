install:
	gcc -W -g -o filebrowser filebrowser.c tui.c util.c tree.c charbuf.c -l termbox -lm
