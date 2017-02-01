#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define FREG_DEVICE_NAME "/dev/freg"

int main(int argc, char** argv)
{
    int fd = -1;
    int val = 0;

    fd = open(FREG_DEVICE_NAME, O_RDWR);
    if (fd < 0) {
        printf("Failed to open file\n");
        return -1;
    }

    printf("Read original value:\n");
    read(fd, &val, sizeof(val));
    printf("%d\n", val);

    val = 5;
    printf("Write value: %d\n", val);
    write(fd, &val, sizeof(val));

    printf("Read value again:\n");
    read(fd, &val, sizeof(val));
    printf("%d\n", val);

    return 0;
}
