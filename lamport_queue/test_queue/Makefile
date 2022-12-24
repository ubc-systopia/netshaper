all: consumer producer initializer

initializer: initializer.c
	gcc -o initializer initializer.c

producer: producer.c
	gcc -o producer producer.c

consumer: consumer.c
	gcc -o consumer consumer.c -lm

clean: 
	rm -f producer consumer	