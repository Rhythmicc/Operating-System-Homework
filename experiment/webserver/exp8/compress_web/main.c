#include <stdio.h>
#include <stdlib.h>
#include "../fs.h"

int main(int argc, char **argv) {
    fs_t fs = empty_fs("fs_dt");
    char buffer[105];
    char as_filename[105];
    for(int i=0;i<5000;++i){
        sprintf(buffer, "/usr/shiyou1/Crawler4jDate/%d.html", i);
        sprintf(as_filename, "%d.html", i);
        fs_load(fs, buffer, as_filename);
    }
    fs_export(fs, "fs");
    delete_fs(fs);
    return 0;
}
