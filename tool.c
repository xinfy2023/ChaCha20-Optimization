#include "src/mercha.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LCG_A 1103515245UL  
#define LCG_C 12345UL       
#define LCG_M 2147483648UL  

typedef struct  
{
    char file_name[256];
    uint64_t length;
    uint8_t key[32];
    uint8_t nonce[12];
    uint8_t result[64];
    uint64_t generate_info;
} meta_info; 


meta_info parse(FILE* file){
    char buffer[1024];
    meta_info meta;
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        if (strncmp(buffer,"File name:", 10)==0){
            fgets(buffer, sizeof(buffer), file);
            int i = 0;
            while(buffer[i]!='\0' && buffer[i]==' '){
                i++;
            }
            strcpy(meta.file_name, buffer+i);
            meta.file_name[strlen(meta.file_name)-1]='\0';
        }
        if (strncmp(buffer,"Length:", 7)==0){
            fgets(buffer, sizeof(buffer), file);
            meta.length=atoi(buffer);
        }
        if (strncmp(buffer,"Key:", 4)==0){
            fgets(buffer, sizeof(buffer), file);
            int i = 0;
            while(buffer[i]!='\0' && buffer[i]==' '){
                i++;
            }
            i += 2; 
            for (size_t j = 0; j < 32; ++j) {
                char c_high = buffer[i+2*j];
                char c_low = buffer[i+2*j+1];
                
                uint8_t val_high = (c_high <= '9') ? c_high - '0' : tolower(c_high) - 'a' + 10;
                uint8_t val_low = (c_low <= '9') ? c_low - '0' : tolower(c_low) - 'a' + 10;  
                meta.key[j] = (val_high << 4) | val_low;
            }
        }
        if (strncmp(buffer,"Nonce:", 6)==0){
            fgets(buffer, sizeof(buffer), file);
            int i = 0;
            while(buffer[i]!='\0' && buffer[i]==' '){
                i++;
            }
            i += 2; 
            for (size_t j = 0; j < 12; ++j) {
                char c_high = buffer[i+2*j];
                char c_low = buffer[i+2*j+1];
                
                uint8_t val_high = (c_high <= '9') ? c_high - '0' : tolower(c_high) - 'a' + 10;
                uint8_t val_low = (c_low <= '9') ? c_low - '0' : tolower(c_low) - 'a' + 10;  
                meta.nonce[j] = (val_high << 4) | val_low;
            }
        }
        if (strncmp(buffer,"Result:", 7)==0){
            fgets(buffer, sizeof(buffer), file);
            int i = 0;
            while(buffer[i]!='\0' && buffer[i]==' '){
                i++;
            }
            i += 2; 
            for (size_t j = 0; j < 64; ++j) {
                char c_high = buffer[i+2*j];
                char c_low = buffer[i+2*j+1];
                
                uint8_t val_high = (c_high <= '9') ? c_high - '0' : tolower(c_high) - 'a' + 10;
                uint8_t val_low = (c_low <= '9') ? c_low - '0' : tolower(c_low) - 'a' + 10;  
                meta.result[j] = (val_high << 4) | val_low;
            }
        }
        if (strncmp(buffer,"Generate info:", 14)==0){
            fgets(buffer, sizeof(buffer), file);
            meta.generate_info=atoi(buffer);
        }
    }
    return meta;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Please input a meta file address");
        return -1;
    }

    FILE *file = fopen(argv[1], "r");

    meta_info meta = parse(file);
    
    fclose(file);

    printf("===META INFO===\n");
    printf("File name: \n");
    printf("   %s\n",meta.file_name);
    printf("Length:\n");
    printf("   %ld\n", meta.length);
    printf("Key:\n");
    printf("   0x");
    for(int i =0; i<32;++i){
        printf("%02x", meta.key[i]);
    }
    printf("\n");
    printf("Nonce:\n");
    printf("   0x");
    for(int i =0; i<12;++i){
        printf("%02x", meta.nonce[i]);
    }
    printf("\n");
    printf("Result:\n");
    printf("   0x");
    for(int i =0; i<64;++i){
        printf("%02x", meta.result[i]);
    }
    printf("\n");
    printf("Generate info:\n");
    printf("   %ld\n", meta.generate_info);

    printf("===GENERATING===\n");

    file = fopen(meta.file_name, "w+");
    if (file == NULL) {
        printf("Fail to create file %s!\n", meta.file_name);
        return -1;
    } else {
        printf("Success create file %s.\n", meta.file_name);
    }

    uint8_t *buffer = malloc(meta.length);
    for(int i=0; i<meta.length; ++i) {
        meta.generate_info = ((unsigned long long)LCG_A * meta.generate_info + LCG_C) % LCG_M;
        buffer[i] = meta.generate_info % 255;
    }
    int ret = fwrite(buffer, 1, meta.length, file);
    free(buffer);
    printf("Write %d bytes to file %s.\n", ret, meta.file_name);
    fclose(file);
    printf("===FINISH===\n");
    return 0;
}
