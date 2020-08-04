#ifndef __FAT_H__
#define __FAT_H__

#include <stdint.h>
#include <stdio.h>
#include <time.h>

static FILE *f = NULL;

#define SIZE_DISC_SECTOR 512
#define ENTRY_SIZE 32

#define FREE_ENTRY 0x00
#define ENTRY_DELETED 0x05
#define ENTRY_DELETED2 0xE5

#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME 0x08
#define ATTR_SUBDIR 0x10
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_ARCHIVE 0x20
#define ATTR_DEVICE 0x40

#define MAX_PATH 200

static char current_path[MAX_PATH] = "/";

struct fat_boot_sector_t
{
    uint8_t jump_addr[3];
    uint8_t oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t tables_count;
    uint16_t entries_in_root_directory;
    uint16_t all_sectors_1;
    uint8_t media_type;
    uint16_t sectors_per_table;
    uint16_t sectors_per_path;
    uint16_t paths_per_cylinder;
    uint32_t hidden_sectors;
    uint32_t all_sectors_2;
    uint8_t drive_number;
    uint8_t reserved;
    uint8_t boot_signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t fs_type[8];
    uint8_t loader[448];
    uint16_t boot_sector_end;
}__attribute__((packed));

struct fat_directory_entry_t
{
    char name[8];
    char ext[3];
    uint8_t attr;
    uint8_t sp1;
    uint8_t sp2;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t sth;
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t file_start;
    uint16_t file_size;
}__attribute__((packed));

struct directory_t
{
    uint16_t start;
    uint32_t position;
};

struct file_t
{
    uint32_t size;
    uint16_t create_time;
    uint16_t modify_time;
    uint16_t access_time;
    uint32_t start_cluster;
    uint32_t number_of_clusters;
    int hidden;
    int read_only;
    int system;
    int volume;
    int directory;
    int archive;
    int device;
};

struct my_file_t
{
    char *filename;
    uint32_t position;
};

void set_file(FILE *nf);
int fat_open();
void info();

void rootinfo_cmd();
void spaceinfo_cmd();
void dir_cmd();
void cd_cmd(char *path);
void pwd_cmd();
void cat_cmd(char *filename);
void get_cmd(char *filename);
void zip_cmd(char *filename1, char *filenam2, char *filename3);
void fileinfo(char *filename);

size_t readblock(void *buffer, uint32_t first_block, size_t block_count);

struct directory_t *open_dir(char *path);
void closedir(struct directory_t *dir);
int readdir(struct directory_t *dir, struct fat_directory_entry_t *entry);

int file_info(struct file_t *file_s, char *path); //Function to get information about file
void file_info_by_entry(struct fat_directory_entry_t *entry, struct file_t *file_s);

struct my_file_t *open(char *path, char *mode);
void close(struct my_file_t *my_file);
int read(void *buffer, uint32_t size, struct my_file_t *my_file);

int get_entry_by_pos(uint32_t start, uint32_t pos, struct fat_directory_entry_t *result); //Function to get entry by given pos
int get_entry_by_name(uint32_t start, struct fat_directory_entry_t *result, char *name); //Function to get entry by given name
int get_entry_by_path(char *path, struct fat_directory_entry_t *result); //Function to get entry by given path

uint32_t get_num_of_entries(uint32_t start); //Function to get number of entries

int skip_entry(struct fat_directory_entry_t *entry); //Function to check if entry is skippable

void print_date(uint16_t date); //Function that prints data in readable way
void print_time(uint16_t time); //Function that prints time in readable way
void edit_filename(char *name, char *ext); //Function to edit filename
void edit_name(char *name); //Function to edit name
void filename_to_upper(char *name); //Function to change all characters to upper
void filename_to_lower(char *name); //Function to change all characters to lower
void free_fat();
int end_cluster(uint32_t cluster);
#endif