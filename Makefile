#NAME: Matt Paterno
#ID: 904756085
#EMAIL: mpaterno@g.ucla.edu

.SILENT:

default:
	gcc -g -Wall -Wextra -o lab1a lab1a.c

dist:
	tar -czvf lab1a-904756085.tar.gz Makefile README lab1a.c