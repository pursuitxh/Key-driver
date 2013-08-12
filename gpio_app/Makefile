.PHONE:clean

mtunl_gpiod: mtunl_gpiod.o
	arm-none-linux-gnueabi-gcc -o $@ $^
mtunl_gpiod.o: mtunl_gpiod.c
	arm-none-linux-gnueabi-gcc -o $@ -c $^
mtunl_gpio_d:
	$(FS_INSTALL) mtunl_gpiod /usr/sbin/mtunl_gpiod
clean:
	rm -rf mtunl_gpiod mtunl_gpiod.o
