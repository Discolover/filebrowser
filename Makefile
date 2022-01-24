filebrowser: tui.h charbuf.h util.h tree.h
	cc -W -g -o filebrowser filebrowser.c tui.c charbuf.c util.c tree.c -ltermbox -lm
