.PHONE:clean

gpiod: gpiod.o
	arm-none-linux-gnueabi-gcc -o $@ $^
gpiod.o: gpiod.c
	arm-none-linux-gnueabi-gcc -o $@ -c $^
clean:
	rm -rf gpiod gpiod.o
