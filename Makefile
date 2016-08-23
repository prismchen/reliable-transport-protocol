all: sender receiver

sender: src/sender.c src/sender_helper.c 
	gcc -o send_execute/sender src/sender.c src/sender_helper.c 

receiver: src/receiver.c src/receiver_helper.c
	gcc -o recv_execute/receiver src/receiver.c src/receiver_helper.c

clean:
	rm send_execute/sender recv_execute/receiver
