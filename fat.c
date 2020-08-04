#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "fat.h"

struct fat_boot_sector_t bs;
static uint32_t root_directory;
static uint32_t number_of_entries;
static uint32_t cluster_size;
static uint32_t cluster_addr;

uint32_t *fat16;

void set_file(FILE *nf)
{
    f = nf;
}

int fat_open()
{
    int check = readblock(&bs, 0, 1);
    if(check != 1)
        return 1;
    
    //Check data
    if(bs.reserved_sectors < 1)
        return 1;
    if(bs.tables_count != 1 && bs.tables_count != 2)
        return 1;
    if(bs.all_sectors_1 == 0 || bs.all_sectors_2 != 0)
        return 1;
    if(bs.entries_in_root_directory == 0)
        return 1;
    if((bs.entries_in_root_directory * ENTRY_SIZE) % bs.bytes_per_sector != 0)
        return 1;
    if(bs.sectors_per_table < 16 || bs.sectors_per_table > 256)
        return 1;

    //Calculate necessary data
    cluster_size = bs.sectors_per_cluster * bs.bytes_per_sector;
    root_directory = bs.bytes_per_sector * (bs.reserved_sectors + bs.sectors_per_table * bs.tables_count);
    cluster_addr = ENTRY_SIZE * bs.entries_in_root_directory + root_directory;
    number_of_entries = bs.sectors_per_table * bs.bytes_per_sector / 2;

    //Reading FAT tables
    uint32_t fat_size = number_of_entries * 2;
    uint8_t fat[bs.tables_count][fat_size];
    for(uint8_t i = 0; i < bs.tables_count; i++)
    {
        uint32_t start_of_fat = bs.bytes_per_sector * (bs.reserved_sectors + bs.sectors_per_table * i);
        uint32_t number_of_start_sector = start_of_fat / SIZE_DISC_SECTOR;
        uint32_t offset_of_start_sector = start_of_fat % SIZE_DISC_SECTOR;
        uint32_t size_in_sectors = (fat_size + offset_of_start_sector) / SIZE_DISC_SECTOR;
        if((fat_size + offset_of_start_sector) % SIZE_DISC_SECTOR)
            size_in_sectors++;

        uint8_t stream[size_in_sectors * SIZE_DISC_SECTOR];
        check = readblock(stream, number_of_start_sector, size_in_sectors);
        if(check != size_in_sectors)
            return 1;
        /*check = fseek(f, number_of_start_sector * SIZE_DISC_SECTOR, SEEK_SET);
        if(check != 0)
            return 1;
        check = fread(stream, SIZE_DISC_SECTOR, size_in_sectors, f);
        if(size_in_sectors != check)
            return 1;*/
        memcpy(fat[i], stream + offset_of_start_sector, fat_size);
    }

    //Check tables
    if(bs.tables_count == 2)
    {
        for(uint32_t i = 0; i < fat_size; i++)
        {
            if(fat[0][i] != fat[1][i])
                return 1;
        }
    }
    //Save table
    fat16 = (uint32_t *)malloc(number_of_entries * 4);
    if(fat16 == NULL)
        return 1;
    for(int i = 0; i < number_of_entries; i++)
        fat16[i] = ((uint16_t *) fat[0])[i];

    return 0;
}

size_t readblock(void *buffer, uint32_t first_block, size_t block_count)
{
    if(buffer == NULL || block_count < 1)
        return 0;
    int check = fseek(f, first_block * SIZE_DISC_SECTOR, SEEK_SET);
    if(check != 0)
        return 0;
    size_t res = fread(buffer, SIZE_DISC_SECTOR, block_count, f);
    return res;
}

void pwd_cmd()
{
    printf("%s\n", current_path);
}

void dir_cmd()
{
    //Open current directory
    struct directory_t *directory = open_dir(current_path);
    if(directory == NULL)
    {
        printf("Can't open directory \n");
        return;
    }
    //Get entry
    struct fat_directory_entry_t ent;
    int check = readdir(directory, &ent);
    while(check == 0)
    {
        char full_path[MAX_PATH];
        strcpy(full_path, current_path);
        strcat(full_path, "/");
        strcat(full_path, ent.name);
        //Get information
        struct file_t file_stat;
        file_info_by_entry(&ent, &file_stat);
        //Print data
        print_date(ent.modify_date);
        print_time(ent.modify_time);
        if(file_stat.directory)
            printf(" < DIRECTORY > ");
        else
            printf("   File size: %d    ", file_stat.size);
        edit_filename(ent.name, ent.ext);
        printf(" %s\n", ent.name);
        //Get next entry
        check = readdir(directory, &ent);
    }
    closedir(directory);
}

int readdir(struct directory_t *dir, struct fat_directory_entry_t *entry)
{
    if(dir == NULL)
        return 1;
    while(1)
    {
        int check = get_entry_by_pos(dir->start, dir->position, entry);
        if(check == 1 || entry->name[0] == 0)
            return 1;
        dir->position++;
        if(!skip_entry(entry))
            break;
    }
    return 0;
}

struct directory_t *open_dir(char *path)
{
    //Check if directory exists
    struct file_t file_stat;
    int check = file_info(&file_stat, path);
    if(check == 1 || file_stat.directory == 0)
    {
        if(strcmp(path, "/") != 0)
            return NULL;
    }
    uint16_t cluster;
    if(check == 1)
        cluster = 0;
    else
        cluster = file_stat.start_cluster;
    //Set result
    struct directory_t *result = (struct directory_t *)malloc(sizeof(struct directory_t));
    if(result == NULL)
        return NULL;
    result->position = 0;
    result->start = cluster;
    return result;
}

void closedir(struct directory_t *dir)
{
    free(dir);
}

int end_cluster(uint32_t cluster)
{
    if(cluster >= 0xFFF8)
        return 1;
    if(cluster == 0)
        return 1;
    return 0;
}

int get_entry_by_pos(uint32_t start, uint32_t pos, struct fat_directory_entry_t *result)
{
    if(start == 0) //Main directory
    {
        //Find sector
        uint32_t addr = root_directory + pos * ENTRY_SIZE;
        uint32_t offset = addr % SIZE_DISC_SECTOR;
        uint32_t sector_num = addr / SIZE_DISC_SECTOR;
        uint8_t sector[SIZE_DISC_SECTOR];
        int check = readblock(sector, sector_num, 1);
        if(check != 1)
            return 1;
        //Return result
        struct fat_directory_entry_t *res = (struct fat_directory_entry_t *)(sector + offset);
        *result = *res;
        return 0;
    }
    //Calculate offset
    uint32_t offset = pos * ENTRY_SIZE;
    uint32_t current = start;
    //Finding cluster
    while(offset >= cluster_size)
    {
        current = fat16[current];
        offset = offset - cluster_size;
    }
    uint8_t cluster[cluster_size];
    uint32_t tmp = (cluster_addr + (current - 2) * cluster_size) / SIZE_DISC_SECTOR;
    int check = readblock(cluster, tmp, cluster_size / SIZE_DISC_SECTOR);
    if(check != cluster_size / SIZE_DISC_SECTOR)
        return 1;
    //Return result
    struct fat_directory_entry_t *res = (struct fat_directory_entry_t *)(cluster + offset);
    *result = *res;
    return 0;
}

int get_entry_by_path(char *path, struct fat_directory_entry_t *result)
{
    char *tab1 = strdup(path);
    char *tab2 = strtok(tab1, "/");
    if(tab2 == NULL)
        return 1;
    struct fat_directory_entry_t ent;
    int check = 1;
    uint32_t current = 0;
    while(tab2 != NULL)
    {
        if(check == 0) //Not directory, so return
        {
            free(tab1);
            return 1;
        }
        //Get entry
        int check2 = get_entry_by_name(current, &ent, tab2);
        if(check2 == 1)
        {
            free(tab1);
            return 1;
        }
        check = ent.attr & ATTR_SUBDIR; // 1 = directory
        current = ent.file_start;
        tab2 = strtok(NULL, "/");
    }
    *result = ent;
    free(tab1);
    return 0;
}

void edit_filename(char *name, char *ext)
{
    int i = 0;
    for(; *(name + i) != ' ' && *(name + i) != '\0'; i++);
    if(*ext == ' ')
    {
        *(name + i) = '\0';
        return;
    }
    *(name + i) = '.';
    int j = 0;
    i++;
    for(;*(ext+j) != ' ' && *(ext+j) != '\0' && j <= 3;j ++)
    {
        *(name + i) = *(ext + j);
        i++;
    }
    *(name + i) = '\0';
}

void edit_name(char *name)
{
    int i = 0;
    for(; *(name + i) != ' ' && *(name + i) != '\0'; i++);
    *(name + i) = '\0';
}

void filename_to_upper(char *name)
{
    for(int i = 0; *(name + i) != '\0'; i++)
        *(name + i) = toupper(*(name + i));
}

void filename_to_lower(char *name)
{
    for(int i = 0; *(name + i) != '\0'; i++)
        *(name + i) = tolower(*(name + i));
}

int get_entry_by_name(uint32_t start, struct fat_directory_entry_t *result, char *name)
{
    uint32_t n_of_entries = get_num_of_entries(start);
    //Check name of entries
    for(uint32_t i = 0; i < n_of_entries; i++)
    {
        struct fat_directory_entry_t current;
        //Get entry
        int check = get_entry_by_pos(start, i, &current);
        if(check == 0 && !skip_entry(&current))
        {
            edit_filename(current.name, current.ext);
            edit_name(name);
            filename_to_lower(name);
            if(strcmp(current.name, name) == 0) //Found result
            {
                *result = current;
                return 0;
            }
            filename_to_upper(name);
            if(strcmp(current.name, name) == 0) //Found result
            {
                *result = current;
                return 0;
            }
        }
    }
    return 1;
}

int skip_entry(struct fat_directory_entry_t *entry)
{
    if(entry->name[0] == FREE_ENTRY)
        return 1;
    if(entry->name[0] == ENTRY_DELETED)
        return 1;
    if(entry->name[0] == ENTRY_DELETED2)
        return 1;
    if(entry->attr == ATTR_SYSTEM)
        return 1;
    if(entry->attr == ATTR_VOLUME)
        return 1; 
    return 0;   
}

uint32_t get_num_of_entries(uint32_t start)
{
    if(start == 0)
        return bs.entries_in_root_directory;
    uint32_t res = 0;
    uint32_t i = 0;
    while(1)
    {
        struct fat_directory_entry_t current;
        int check = get_entry_by_pos(start, i, &current);
        if(check == 1)
            return 0;
        if(current.name[0] == 0x00)
            return res;
        res++;
        i++;
    }
}

void get_cmd(char *filename)
{
    if(filename == NULL)
        return;

    char new_path[MAX_PATH];
    strcpy(new_path, current_path);
    if(strlen(new_path) > 1)
        strcat(new_path, "/");
    strcat(new_path, filename);

    //Get file with given name
    struct my_file_t *my_file = open(new_path, "r");
    if(my_file == NULL) //Check if exists
    {
        printf("Failed to open file\n");
        return;
    }
    //Create result file
    FILE *fresult = fopen(filename, "w");
    if(fresult == NULL)
    {
        close(my_file);
        printf("Failed to get file\n");
        return;
    }
    char character;
    int check = 1;
    //Read file and write result to file in local directory
    while(check > 0)
    {
        check = read(&character, 1, my_file);
        if(check == -1)
            break;
        fwrite(&character, 1, check, fresult);
    }
    close(my_file);
    fclose(fresult);
}

void zip_cmd(char *filename1, char *filename2, char *filename3)
{
    if(filename1 == NULL || filename2 == NULL || filename3 == NULL)
        return;

    char new_path1[MAX_PATH];
    strcpy(new_path1, current_path);
    if(strlen(new_path1) > 1)
        strcat(new_path1, "/");
    strcat(new_path1, filename1);

    char new_path2[MAX_PATH];
    strcpy(new_path2, current_path);
    if(strlen(new_path2) > 1)
        strcat(new_path2, "/");
    strcat(new_path2, filename2);

    //Check if first file exists
    struct my_file_t *my_file1 = open(new_path1, "r");
    if(my_file1 == NULL)
    {
        printf("Failed to open first file\n");
        return;
    }
    //Check if second file exists
    struct my_file_t *my_file2 = open(new_path2, "r");
    if(my_file2 == NULL)
    {
        close(my_file1);
        printf("Failed to open second file\n");
        return;
    }
    //Create result file
    FILE *fresult = fopen(filename3, "w");
    if(fresult == NULL)
    {
        close(my_file1);
        close(my_file2);
        printf("Failed to open third file\n");
        return;
    }

    int read1 = 1;
    int read2 = 1;

    while(read1 || read2)
    {
        char character = 's';
        //Read line from first file
        while(read1 && character != '\n')
        {
            read1 = read(&character, 1, my_file1);
            if(read1 == -1)
                break;
            fwrite(&character, 1, read1, fresult);
        }

        character = 's';
        //Read line from second file
        while (read2 && character != '\n')
        {
            read2 = read(&character, 1, my_file2);
            if(read2 == -1)
                break;
            fwrite(&character, 1, read2, fresult);
        }
    }
    close(my_file1);
    close(my_file2);
    fclose(fresult);
    printf("Zipped file\n");
}

void cat_cmd(char *filename)
{
    if(filename == NULL)
        return;

    char new_path[MAX_PATH];
    strcpy(new_path, current_path);
    if(strlen(new_path) > 1)
        strcat(new_path, "/");
    strcat(new_path, filename);

    //Check if file exists
    struct my_file_t *my_file = open(new_path, "r");
    if(my_file == NULL)
    {
        printf("Failed to open file\n");
        return;
    }
    char character;
    int check = 1;
    //Reading from file
    while(check > 0)
    {
        check = read(&character, 1, my_file);
        if(check == -1)
            break;
        printf("%c", character);
    }
    close(my_file);
    printf("\n");
}

void fileinfo(char *filename)
{
    if(filename == NULL)
        return ;
    char new_path[MAX_PATH];
    strcpy(new_path, current_path);
    if(strlen(new_path) > 1)
        strcat(new_path, "/");
    strcat(new_path, filename);
    //Check if file exists
    struct file_t my_file;
    int check = file_info(&my_file, new_path);
    if(check)
    {
        printf("File not found\n");
        return;
    }

    printf("Path: %s\n", new_path);
    //Get attributes
    char archive = '-', read_only = '-', system = '-', hidden = '-', device = '-', volume = '-';
    if(my_file.archive)
        archive = '+';
    if(my_file.read_only)
        read_only = '+';
    if(my_file.system)
        system = '+';
    if(my_file.hidden)
        hidden = '+';
    if(my_file.device)
        device = '+';
    if(my_file.volume)
        volume = '+';

    printf("Attributes: A%c R%c S%c H%c D%c V%c\n", archive, read_only, system, hidden, device, volume);
    printf("Size: %u bytes", my_file.size);
    printf("Last write: ");
    print_date(my_file.modify_time);
    print_time(my_file.modify_time);
    printf("\n");
    printf("Last access: ");
    print_date(my_file.access_time);
    print_time(my_file.access_time);
    printf("\n");
    printf("Created: ");
    print_date(my_file.create_time);
    print_time(my_file.create_time);
    printf("\n");
    printf("Clusters chain: ");
    //Get number of clusters
    if(my_file.number_of_clusters > 0)
    {
        printf("%hu ", my_file.start_cluster);
        uint32_t current = my_file.start_cluster;
        for(int i = 1; i < my_file.number_of_clusters; i++)
        {
            current = fat16[current];
            printf("%u ", current);
        }
    }
    printf("\n");
    printf("Number of clusters: %u\n", my_file.number_of_clusters);
}

void file_info_by_entry(struct fat_directory_entry_t *entry, struct file_t *file_s)
{
    //Get information about file
    file_s->create_time = entry->create_time;
    file_s->access_time = entry->access_date;
    file_s->modify_time = entry->modify_date;
    file_s->size = entry->file_size;
    file_s->start_cluster = entry->file_start;
    file_s->system = entry->attr & ATTR_SYSTEM;
    file_s->volume = entry->attr & ATTR_VOLUME;
    file_s->directory = entry->attr & ATTR_SUBDIR;
    file_s->hidden = entry->attr & ATTR_HIDDEN;
    file_s->device = entry->attr & ATTR_DEVICE;     
    file_s->archive = entry->attr & ATTR_ARCHIVE;   
    file_s->hidden = entry->attr & ATTR_HIDDEN;     

    uint32_t clusters = 0;
    uint32_t current = entry->file_start;
    //Get number of clusters
    while(end_cluster(current) == 0)
    {
        current = fat16[current];
        clusters++;
    }
    file_s->number_of_clusters = clusters;
}

int read(void *buffer, uint32_t size, struct my_file_t *my_file)
{
    if(!buffer || !my_file)
        return 0;

    struct fat_directory_entry_t entry;
    //Check if entry exists
    int check = get_entry_by_path(my_file->filename, &entry);
    if(check == 1)
        return 0;

    if(entry.file_size <= my_file->position)
        return -1;
    //Calculate how much data to read
    uint32_t to_read = entry.file_size - my_file->position;
    if(to_read < size)
        size = to_read;
    
    int read = 0;

    for(int i = 0; i < size; i++)
    {
        uint32_t current = entry.file_start;
        uint32_t offset = my_file->position;
        //Finding cluster
        while(offset >= cluster_size)
        {
            current = fat16[current];
            offset = offset - cluster_size;
        }

        uint8_t cluster[cluster_size];
        check = readblock(cluster, (cluster_addr + (current - 2) * cluster_size) / SIZE_DISC_SECTOR, cluster_size / SIZE_DISC_SECTOR);
        if(check == (cluster_size / SIZE_DISC_SECTOR))
        {
            ((uint8_t *)buffer)[i] = cluster[offset];
            offset++;
            read++;
            //Get next cluster
            if(cluster_size <= offset)
            {
                offset = 0;
                current = fat16[current];
                check = readblock(cluster, (cluster_addr + (current - 2) * cluster_size) / SIZE_DISC_SECTOR, cluster_size / SIZE_DISC_SECTOR);
                if(check != (cluster_size / SIZE_DISC_SECTOR))
                {
                    my_file->position += read;
                    return read;
                }
            }
        }
    }
    my_file->position += read;
    return read;
}

struct my_file_t *open(char *path, char *mode)
{
    //Check if file exists and it is not a directory
    struct file_t file_stat;
    int check = file_info(&file_stat, path);
    if(check == 1 || file_stat.directory == 1)
        return NULL;
    struct my_file_t *my_file = (struct my_file_t *)malloc(sizeof(struct my_file_t *));
    if(my_file == NULL)
        return NULL;
    //Set data
    my_file->position = 0;
    my_file->filename = path;
    return my_file;
}

void close(struct my_file_t *my_file)
{
    free(my_file);
}

void cd_cmd(char *path)
{
    if(path == NULL)
        return;
    if(*path == '.' && *(path + 1) == '.') //Get previous directory
    {
        if(strlen(current_path) == 1) //Previous directory doesn't exists
            return;
        char *pos = strrchr(current_path, '/');
        *(pos + 1) = '\0';
        return;
    }

    char new_path[MAX_PATH];
    strcpy(new_path, current_path);
    if(strlen(new_path) > 1)
        strcat(new_path, "/");
    strcat(new_path, path);
    //Check if directory exists
    struct directory_t *directory = open_dir(new_path);
    if(directory == NULL)
    {
        printf("Directory doesn't exists\n");
        return;
    }
    closedir(directory);
    strcpy(current_path, new_path);
}

void print_date(uint16_t date)
{
    uint8_t day = date & 31;
    uint8_t month = (date >> 5) & 15;
    uint16_t year = ((date >> 9) & 127) + 1980;
    printf("%02u/%02u/%04u", day, month, year);
}

void print_time(uint16_t time)
{
    uint8_t minute = (time >> 5) & 63;
    uint16_t hour = (time >> 11) & 31;
    printf(" %02d:%02d ", hour, minute);
}

void spaceinfo_cmd()
{
    uint32_t free = 0, used = 0, bad = 0, end = 0;
    printf("%u\n", number_of_entries);
    //Get information about clusters
    for(uint32_t i = 0; i < number_of_entries; i++)
    {
        uint32_t tmp = fat16[i];
        if((tmp >= 0xFFF8 || tmp == 0) && tmp != 0)
            end += 1;
        if(tmp == 0xFFF7)
            bad += 1;
        if(tmp == 0)
            free += 1;
        if(tmp != 0)
            used+= 1;
    }
    printf("Used clusters: %u\n", used);
    printf("Free clusters: %u\n", free);
    printf("Bad clusters: %u\n", bad);
    printf("End clusters: %u\n", end);
    printf("Cluster size in bytes: %u\n", bs.bytes_per_sector * bs.sectors_per_cluster);
    printf("Cluster size in sectors: %u\n", bs.sectors_per_cluster);
}

void rootinfo_cmd()
{
    int used = 0;
    int free = 0;
    //Get number of free entires
    for(int i = 0; i < bs.entries_in_root_directory; i++)
    {
        uint32_t e_addr = root_directory + i * ENTRY_SIZE;
        uint32_t number_of_sector = e_addr / SIZE_DISC_SECTOR;

        uint8_t sector[SIZE_DISC_SECTOR];
        int check = readblock(sector, number_of_sector, 1);
        if(check != 1)
            return;

        struct fat_directory_entry_t *e = (struct fat_directory_entry_t *)(sector + (e_addr % SIZE_DISC_SECTOR));
        if(e->name[0] == ENTRY_DELETED || e->name[0] == ENTRY_DELETED2 || e->name[0] == FREE_ENTRY)
            free += 1;
    }
    //Get number of used entries
    used = bs.entries_in_root_directory;
    used -= free;
    printf("All entries: %u\n", used+free);
    printf("Used entries: %u\n", used);
    printf("Free entries: %u\n", free);
    printf("Percentage: %f%%\n", 100.0 * used / (used + free));
}

int file_info(struct file_t *file_s, char *path)
{
    if(file_s == NULL || path == NULL)
        return 1;
    //Check if file exists
    struct fat_directory_entry_t entry;
    int check = get_entry_by_path(path, &entry);
    if(check != 0)
        return 1;
    //Get file information
    file_s->create_time = entry.create_time;
    file_s->access_time = entry.access_date;
    file_s->modify_time = entry.modify_date;
    file_s->size = entry.file_size;
    file_s->start_cluster = entry.file_start;
    file_s->system = entry.attr & ATTR_SYSTEM;
    file_s->volume = entry.attr & ATTR_VOLUME;
    file_s->directory = entry.attr & ATTR_SUBDIR;
    file_s->hidden = entry.attr & ATTR_HIDDEN;
    file_s->device = entry.attr & ATTR_DEVICE;     
    file_s->archive = entry.attr & ATTR_ARCHIVE;   
    file_s->hidden = entry.attr & ATTR_HIDDEN;     
    //Get number of clusters
    uint32_t clusters = 0;
    uint32_t current = entry.file_start;
    while(end_cluster(current) == 0)
    {
        current = fat16[current];
        clusters++;
    }
    file_s->number_of_clusters = clusters;
    return 0;
}

void free_fat()
{
    free(fat16);
}

void info()
{
    printf("----------ABOUT THIS FAT----------\n\n");
    printf("OEM name: %s\n", bs.oem_name);
    printf("Bytes per sector: %u\n", bs.bytes_per_sector);
    printf("Sectors per cluster: %u\n", bs.sectors_per_cluster);
    printf("Reserved sectors: %u\n", bs.reserved_sectors);
    printf("Number of tables: %u\n", bs.tables_count);
    printf("Media type: %u\n", bs.media_type);
    printf("Sectors 1: %u\n", bs.all_sectors_1);
    printf("Sectors 2: %u\n", bs.all_sectors_2);
    printf("Sectors per table: %u\n", bs.sectors_per_table);
    printf("Sectors per path: %u\n", bs.sectors_per_path);
    printf("Sectors per cylinder: %u\n", bs.paths_per_cylinder);
    printf("\n------------------------------------\n");
}