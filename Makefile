all: sample ssi inf args

sample: sample.c
	gcc sample.c -lreadline -lhistory -ltermcap -o sample

ssi: ssi.c
	gcc ssi.c -lreadline -lhistory -ltermcap -o ssi

inf: inf.c
	gcc inf.c -o inf

args: args.c
	gcc args.c -o args

clean:
	rm -f *.o
	rm -f sample
	rm -f inf
	rm -f args
	rm -f ssi
