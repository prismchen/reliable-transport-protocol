all: sender receiver

sender: src/sender.c
	gcc -o sender src/sender.c

receiver: src/receiver.c
	gcc -o receiver src/receiver.c

clean:
	rm sender receiver
