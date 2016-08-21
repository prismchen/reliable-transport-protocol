all: sender receiver

sender: src/sender.c
	gcc -o send/sender src/sender.c

receiver: src/receiver.c
	gcc -o recv/receiver src/receiver.c

clean:
	rm send/sender recv/receiver
