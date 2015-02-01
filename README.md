# Install/removal
To install enter the command "make". Insert the module by using the command "insmod randomizer.ko". You can remove the module
by entering the command "rmmod randomizer".

# Usage
Write random data to a file by using the following command "head -c 1m /dev/randomizer >> file.txt". This will write 1MB to
the file "file.txt".
