TESTING SAMPLES ON RASPBERRY PI 3

To test a sample on Raspberry Pi 3, for example "slaveinfo", you need to:

- compile the slaveinfo sample, using "make",
  you need arm-none-eabi- toolchain being in your PATH;
  this will generate the kernel8-32.img file.
  
- copy all content of the "boot/" directory into an empty SD-card;

- copy "kernel8-32.img" into the SD-card;

Now your SD-card should be bootable.