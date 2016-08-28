all: sender receiver

sender: src/sender.cpp src/sender_helper.cpp 
	g++ -pthread -o send_execute/sender src/sender.cpp src/sender_helper.cpp 

receiver: src/receiver.cpp src/receiver_helper.cpp
	g++ -pthread -o recv_execute/receiver src/receiver.cpp src/receiver_helper.cpp

clean:
	rm send_execute/sender recv_execute/*
