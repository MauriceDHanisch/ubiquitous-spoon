#include <stdio.h>
#include <string.h>

struct vuln{
    char buffer[0x10001];  
    void (*func_ptr)(void); // Capabilities need to be 16 byte alligned 
};

void safe_fn() {
    printf("Safe function executed.\n");
};

void unsafe_fn() {
    printf("\n!!! Unsafe function executed !!!\n");
};

int main(void) {
    struct vuln v;
    v.func_ptr = safe_fn; 

    printf("Before overflow:\n");
    v.func_ptr();

    printf("\nOverflowing buffer to overwrite function pointer...\n");

    // Prepare the payload
    size_t aligned_offset = (0x10001 + (16 - 1)) & ~(16 - 1);
    char payload[aligned_offset + 16]; // initialize a payload 
    memset(payload, 'A', aligned_offset);
    void (*hijack)() = unsafe_fn;
    memcpy(payload + aligned_offset, &hijack, sizeof(hijack)); // copy hijack into payload starting at padding end!

    // Overwrite the buffer + ptr address
    memcpy(v.buffer, payload, sizeof(payload));

    printf("after overflow:\n");
    v.func_ptr();

    return(0);
}
