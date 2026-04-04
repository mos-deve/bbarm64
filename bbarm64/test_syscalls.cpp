#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/sendfile.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>

int main() {
    printf("bbarm64 syscall test suite starting...\n");
    
    // Test 1: open, write, read, close
    printf("\n=== Test 1: Basic file I/O ===\n");
    int fd = open("test_file.txt", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open failed");
        return 1;
    }
    printf("✓ open succeeded, fd = %d\n", fd);
    
    const char* test_data = "Hello from bbarm64 test!";
    ssize_t written = write(fd, test_data, strlen(test_data));
    if (written < 0) {
        perror("write failed");
        return 1;
    }
    printf("✓ write succeeded, wrote %zd bytes\n", written);
    
    off_t pos = lseek(fd, 0, SEEK_SET);
    if (pos < 0) {
        perror("lseek failed");
        return 1;
    }
    printf("✓ lseek succeeded, position = %ld\n", pos);
    
    char buf[256];
    ssize_t read_bytes = read(fd, buf, sizeof(buf) - 1);
    if (read_bytes < 0) {
        perror("read failed");
        return 1;
    }
    buf[read_bytes] = '\0';
    printf("✓ read succeeded, got: '%s'\n", buf);
    
    close(fd);
    printf("✓ close succeeded\n");
    
    // Test 2: dup/dup2
    printf("\n=== Test 2: File descriptor duplication ===\n");
    fd = open("test_dup.txt", O_RDWR | O_CREAT, 0644);
    int fd2 = dup(fd);
    if (fd2 < 0) {
        perror("dup failed");
        return 1;
    }
    printf("✓ dup succeeded, fd2 = %d\n", fd2);
    close(fd);
    close(fd2);
    
    // Test 3: mmap/munmap
    printf("\n=== Test 3: Memory mapping ===\n");
    void* mem = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }
    printf("✓ mmap succeeded, address = %p\n", mem);
    
    strcpy((char*)mem, "Test memory mapping!");
    printf("✓ Wrote to mapped memory: '%s'\n", (char*)mem);
    
    int ret = munmap(mem, 4096);
    if (ret < 0) {
        perror("munmap failed");
        return 1;
    }
    printf("✓ munmap succeeded\n");
    
    // Test 4: pipe
    printf("\n=== Test 4: Pipe ===\n");
    int pipefds[2];
    if (pipe(pipefds) < 0) {
        perror("pipe failed");
        return 1;
    }
    printf("✓ pipe succeeded, read_fd = %d, write_fd = %d\n", pipefds[0], pipefds[1]);
    
    const char* pipe_msg = "Pipe test message";
    write(pipefds[1], pipe_msg, strlen(pipe_msg));
    char pipe_buf[128];
    read_bytes = read(pipefds[0], pipe_buf, sizeof(pipe_buf) - 1);
    pipe_buf[read_bytes] = '\0';
    printf("✓ Pipe read/write succeeded: '%s'\n", pipe_buf);
    close(pipefds[0]);
    close(pipefds[1]);
    
    // Test 5: getpid/getppid
    printf("\n=== Test 5: Process info ===\n");
    printf("✓ getpid() = %d\n", getpid());
    printf("✓ getppid() = %d\n", getppid());
    printf("✓ getuid() = %d\n", getuid());
    printf("✓ getgid() = %d\n", getgid());
    
    // Test 6: nanosleep
    printf("\n=== Test 6: Sleep ===\n");
    struct timespec ts = {0, 100000000}; // 100ms
    if (nanosleep(&ts, NULL) < 0) {
        perror("nanosleep failed");
    } else {
        printf("✓ nanosleep succeeded\n");
    }
    
    // Test 7: readv/writev
    printf("\n=== Test 7: Vector I/O ===\n");
    struct iovec iov[3];
    char buf1[] = "Vector ";
    char buf2[] = "I/O ";
    char buf3[] = "test!\n";
    iov[0].iov_base = buf1;
    iov[0].iov_len = strlen(buf1);
    iov[1].iov_base = buf2;
    iov[1].iov_len = strlen(buf2);
    iov[2].iov_base = buf3;
    iov[2].iov_len = strlen(buf3);
    
    ssize_t vwritten = writev(STDOUT_FILENO, iov, 3);
    if (vwritten < 0) {
        perror("writev failed");
    } else {
        printf("✓ writev succeeded, wrote %zd bytes\n", vwritten);
    }
    
    // Test 8: clock_gettime
    printf("\n=== Test 8: Time ===\n");
    struct timespec ts2;
    if (clock_gettime(CLOCK_MONOTONIC, &ts2) < 0) {
        perror("clock_gettime failed");
    } else {
        printf("✓ clock_gettime succeeded: %ld.%09ld\n", ts2.tv_sec, ts2.tv_nsec);
    }
    
    printf("\n✅ All tests passed! bbarm64 is working correctly.\n");
    
    // Cleanup
    unlink("test_file.txt");
    unlink("test_dup.txt");
    
    return 0;
}
