# 1. Tell the Linux kernel build system what object file to create.
# The kernel looks for 'obj-m' to know what needs to be compiled as a module.
obj-m += bmp280.o

# 2. Define where the compiled kernel source tree lives inside Buildroot.
# This path points to the exact kernel directory Buildroot extracted and configured.
KDIR ?= /mnt/data/buildroot/output/build/linux-custom

# 3. Define the cross-compiler toolchain built by Buildroot.
# We must tell the build system to target the ARM architecture and use the Buildroot compiler.
ARCH ?= arm64
CROSS_COMPILE ?= /mnt/data/buildroot/output/host/bin/aarch64-linux-

# The default target triggered when you type 'make'
# -C shifts the execution context entirely to the kernel source directory (KDIR).
# M=$(PWD) tells the kernel build system: "Once you read your own rules, come back 
# to my current directory to find the source files and compile them."
all:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules

# Clean up build artifacts
clean:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) clean