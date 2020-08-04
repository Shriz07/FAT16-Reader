#include <stdio.h>
#include <string.h>
#include "fat.h"

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        printf("Incorrect input");
        return 1;
    }

    FILE *nf = fopen(argv[1], "rb+");
    if(nf == NULL)
    {
        printf("File not found");
        return 1;
    }
    set_file(nf);
    fat_open();
    info();
    char cmd[100];

    while(1)
    {
        printf(">>");
        fgets(cmd, 100, stdin);
        int len = strlen(cmd);
        cmd[len - 1] = '\0';
        if(strcmp(cmd, "exit") == 0)
        {
            free_fat();
            fclose(nf);
            break;
        }
        else if(strcmp(cmd, "pwd") == 0)
            pwd_cmd();
        else if(strcmp(cmd, "rootinfo") == 0)
            rootinfo_cmd();
        else if(strcmp(cmd, "spaceinfo") == 0)
            spaceinfo_cmd();
        else if(strcmp(cmd, "dir") == 0)
            dir_cmd();
        else if(cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' ')
        {
            char path[100];
            for(int i = 3; cmd[i] != '\0'; i++)
            {
                path[i-3] = cmd[i];
                path[i-2] = '\0';
            }
            cd_cmd(path);
        }
        else if(cmd[0] == 'f' && cmd[1] == 'i' && cmd[2] == 'l' && cmd[3] == 'e' && cmd[4] == 'i' && cmd[5] == 'n' && cmd[6] == 'f' && cmd[7] == 'o' && cmd[8] == ' ')
        {
            char filename[15];
            for(int i = 9; cmd[i] != '\0'; i++)
            {
                filename[i-9] = cmd[i];
                filename[i-8] = '\0';
            }
            fileinfo(filename);
        }
        else if(cmd[0] == 'c' && cmd[1] == 'a' && cmd[2] == 't' && cmd[3] == ' ')
        {
            char filename[15];
            for(int i = 4; cmd[i] != '\0'; i++)
            {
                filename[i-4] = cmd[i];
                filename[i-3] = '\0';
            }
            cat_cmd(filename);
        }
        else if(cmd[0] == 'g' && cmd[1] == 'e' && cmd[2] == 't' && cmd[3] == ' ')
        {
            char filename[15];
            for(int i = 4; cmd[i] != '\0'; i++)
            {
                filename[i-4] = cmd[i];
                filename[i-3] = '\0';
            }
            get_cmd(filename);
        }
        else if(cmd[0] == 'z' && cmd[1] == 'i' && cmd[2] == 'p' && cmd[3] == ' ')
        {
            char filename1[15];
            char filename2[15];
            char filename3[15];
            int i = 4;
            int j = 0;
            for(; cmd[i] != '\0' && cmd[i] != ' '; i++)
            {
                if(j == 14)
                    break;
                filename1[j] = cmd[i];
                filename1[j+1] = '\0';
                j++;
            }
            if(cmd[i] == '\0' || j == 14)
            {
                printf("Invalid arguments\n");
                continue;
            }
            i++;
            j = 0;
            for(; cmd[i] != '\0' && cmd[i] != ' '; i++)
            {
                if(j == 14)
                    break;
                filename2[j] = cmd[i];
                filename2[j+1] = '\0';
                j++;
            }
            if(cmd[i] == '\0' || j == 14)
            {
                printf("Invalid arguments\n");
                continue;
            }
            i++;
            j = 0;
            for(; cmd[i] != '\0' && cmd[i] != ' '; i++)
            {
                if(j == 14)
                    break;
                filename3[j] = cmd[i];
                filename3[j+1] = '\0';
                j++;
            }
            if(j == 14)
            {
                printf("Invalid arguments\n");
                continue;
            }
            zip_cmd(filename1, filename2, filename3);
        }
    }

    return 0;
}