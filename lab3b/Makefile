#NAME: Robathan Harries
#ID: 904836501
#EMAIL: r0b@ucla.edu
#SLIPDAYS: 0

all: lab3b


lab3b: lab3b.py start_script.sh
	@cp start_script.sh lab3b

clean:
	@rm -f lab3b-904836501.tar.gz lab3b

dist:
	@tar -czvf lab3b-904836501.tar.gz lab3b.py start_script.sh README Makefile
