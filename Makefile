.PHONY: all

all:
	sudo apt install libcurl4-openssl-dev -y > /dev/null
	g++ -o packify packify.cpp -lcurl