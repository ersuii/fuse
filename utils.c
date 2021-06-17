#include "global.h"
#include "utils.h"

#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
char *vdisk_path = "/home/ersui/disk";
char *root_path = "/";  

void utils_init() {

	FILE * fp = NULL;
	fp = fopen(vdisk_path, "r+");
	if (fp == NULL) {
		fprintf(stderr, "unsuccessful!\n");
		return;
	}
	
	struct super_block *super_block_record = malloc(sizeof(struct super_block));
	fread(super_block_record, sizeof(struct super_block), 1, fp);
	
	TOTAL_BLOCK_NUM = super_block_record->fs_size;
	fclose(fp);
	fslog("INIT", "init success\n");
}


//读出src_的属性到dest_

void read_file_dir(struct u_fs_file_directory *dest_,
		struct u_fs_file_directory *src_) {
	
	strcpy(dest_->fname, src_->fname);
	strcpy(dest_->fext, src_->fext);
	dest_->fsize = src_->fsize;
	dest_->nStartBlock = src_->nStartBlock;
	dest_->flag = src_->flag;
}

 //根据块号移动指针，读出内容赋值给blk_info
 
int get_blkinfo_from_read_blkpos(long blk, struct u_fs_disk_block * blk_info) {
	FILE* fp;
	fp = fopen(vdisk_path, "r+");
	if (fp == NULL)
		return -1;
	fseek(fp, blk * FS_BLOCK_SIZE, SEEK_SET);
	fread(blk_info, sizeof(struct u_fs_disk_block), 1, fp);
	if (ferror(fp) || feof(fp)) {
		fclose(fp);
		return -1;
	}
	fclose(fp);
	return 0;

}

//将blk_info内容写进指定块号的磁盘块
 
int write_blkinfo_start_blk(long blk, struct u_fs_disk_block * blk_info) {
	FILE* fp;
	fp = fopen(vdisk_path, "r+");
	if (fp == NULL)
		return -1;
	if (fseek(fp, blk * FS_BLOCK_SIZE, SEEK_SET) != 0) {
		fclose(fp);
		return -1;
	}

	fwrite(blk_info, sizeof(struct u_fs_disk_block), 1, fp);
	if (ferror(fp) || feof(fp)) {
		fclose(fp);
		return -1;
	}
	fclose(fp);
	return 0;
}

 //读取path对应dirent（文件或目录）信息，并赋值到return_dirent中

int get_attr_from_open_pathblock(const char * path,
		struct u_fs_file_directory *attr) {

	
	struct u_fs_disk_block *blk_info;
	blk_info = malloc(sizeof(struct u_fs_disk_block));
	
	if (get_blkinfo_from_read_blkpos(0, blk_info) == -1) {
		fslog("readblk", "%s325path\n", path);
		free(blk_info);
		return -1;
	}

	struct super_block* sb_record;
	sb_record = (struct super_block*) blk_info;
	long start_blk;
	start_blk = sb_record->first_blk;

	char *p, *q, *tmp_path;
	tmp_path = strdup(path);
	p = tmp_path;
	
	if (strcmp(path, "/") == 0) {
		attr->flag = 2;
		attr->nStartBlock = start_blk;
		free(blk_info);
		return 0;
	}
	
	if (!p) {
		free(blk_info);
		return -1;
	}
	p++;
	q = strchr(p, '/');
	if (q != NULL) {
		tmp_path++;  
		*q = '\0';
		q++;
		p = q;     
	}
	q = strchr(p, '.');
	if (q != NULL) {
		*q = '\0';
		q++;     
	}

	
	if (get_blkinfo_from_read_blkpos(start_blk, blk_info) == -1) {
		free(blk_info);
		return -1;
	}
	
	struct u_fs_file_directory *file_dir =
			(struct u_fs_file_directory*) blk_info->data;
	int offset = 0;
	

	if (*tmp_path != '/') {     
		while (offset < blk_info->size) {     
			if (strcmp(file_dir->fname, tmp_path) == 0 && file_dir->flag == 2) { 
				start_blk = file_dir->nStartBlock;     
				break;
			}
			
			file_dir++;
			offset += sizeof(struct u_fs_file_directory);
		}
		
		if (offset == blk_info->size) {
			
			free(blk_info);
			return -1;
		}
		
		if (get_blkinfo_from_read_blkpos(start_blk, blk_info) == -1) {
			free(blk_info);
			return -1;
		}
		file_dir = (struct u_fs_file_directory*) blk_info->data;
	}
	
	offset = 0;
	while (offset < blk_info->size) {			

		if (file_dir->flag != 0 && strcmp(file_dir->fname, p) == 0
				&& (q == NULL || strcmp(file_dir->fext, q) == 0)) { 
			
			start_blk = file_dir->nStartBlock;
			
			read_file_dir(attr, file_dir);
			free(blk_info);
			return 0;
		}
		
		file_dir++;
		offset += sizeof(struct u_fs_file_directory);
	}
	
	free(blk_info);
	return -1;
}

//创建path的文件或目录，flag 为1表示文件，2为目录

int create_file_dir(const char* path, int flag) {
	int res;

	long p_dir_blk;
	char *p = malloc(15 * sizeof(char)), *q = malloc(15 * sizeof(char));
	
	if ((res = dv_path(p, q, path, &p_dir_blk, flag))) {
		fslog("dmcreate", "%lld\n", res);
		freePtrs(p, q, NULL);
		return res;
	}
	fslog("create", "%s183\n", path);
	struct u_fs_disk_block *blk_info = malloc(sizeof(struct u_fs_disk_block));
	
	if (get_blkinfo_from_read_blkpos(p_dir_blk, blk_info) == -1) {
		freePtrs(blk_info, p, q, NULL);
		return -ENOENT;
	}
	struct u_fs_file_directory *file_dir =
			(struct u_fs_file_directory*) blk_info->data;

	int offset = 0;
	int pos = blk_info->size;
	
	if ((res = exist_check(file_dir, p, q, &offset, &pos, blk_info->size, flag))) {
		freePtrs(blk_info, p, q, NULL);
		return res;
	}
	file_dir += offset / sizeof(struct u_fs_file_directory);

	long *tmp = malloc(sizeof(long));
	if (pos == blk_info->size) {
		
		if (blk_info->size > MAX_DATA_IN_BLOCK) {
			
			if ((res = enlarge_blk(p_dir_blk, file_dir, blk_info, tmp, p, q,
					flag))) {
				freePtrs(p, q, blk_info, NULL);
				return res;
			}
			freePtrs(p, q, blk_info, NULL);
			return 0;
		} else {
			
			blk_info->size += sizeof(struct u_fs_file_directory);
			
		}
	} else {	
		offset = 0;
		file_dir = (struct u_fs_file_directory*) blk_info->data;

		fslog("flag=0", "%d\t%d\t\n", offset, pos);
		
		while (offset < pos)
			file_dir++;
	}
	init_file_dir(file_dir, p, q, flag);
	
	tmp = malloc(sizeof(long));

	if ((res = get_free_blocks(1, tmp)) == 1) {
		file_dir->nStartBlock = *tmp;
	} else {
		
		freePtrs(blk_info, p, q, NULL);
		return -errno;
	}
	free(tmp);

	
	write_blkinfo_start_blk(p_dir_blk, blk_info);
	new_emp_blk(blk_info);
	
	write_blkinfo_start_blk(file_dir->nStartBlock, blk_info);

	freePtrs(p, q, blk_info, NULL);

	return 0;
}


 //找到num个连续空闲块，得到空闲块区的起始块号start_block，返回找到的连续空闲块个数（否则返回找到的最大值）

int get_free_blocks(int num, long* start_blk) {
	
	*start_blk = 1 + BITMAP_BLOCK + 1;
	int tmp = 0;
	FILE* fp = NULL;
	fp = fopen(vdisk_path, "r+");
	if (fp == NULL)
		return 0;
	int start, left;
	unsigned int mask, f;
	int *flag;

	int max = 0;
	long max_start = -1;

	while (*start_blk < TOTAL_BLOCK_NUM - 1) {
		
		start = *start_blk / 8;
		left = *start_blk % 8;
		mask = 1;
		mask <<= left;
		fseek(fp, FS_BLOCK_SIZE + start, SEEK_SET);
		flag = malloc(sizeof(int));
		fread(flag, sizeof(int), 1, fp);	
		f = *flag;
		for (tmp = 0; tmp < num; tmp++) {
			if ((f & mask) == mask)	
				break;
			if ((mask & 0x80000000) == 0x80000000) {
				
				fread(flag, sizeof(int), 1, fp);
				f = *flag;
				mask = 1;				
			} else
				
				mask <<= 1;
		}
		
		if (tmp > max) {
			
			max_start = *start_blk;
			max = tmp;
		}
		
		if (tmp == num)
			break;
		
		*start_blk = (tmp + 1) + *start_blk;
		tmp = 0;
		
	}
	*start_blk = max_start;
	fclose(fp);
	int j = max_start;
	int i;
	for (i = 0; i < max; ++i) {
		
		if (utils_set_blk(j++, 1) == -1) {
			free(flag);
			return -1;
		}
	}
	free(flag);
	return max;

}

 //删除path的文件或目录，flag 为1表示文件，2为目录

int rm_file_dir(const char *path, int flag) {
	struct u_fs_file_directory *attr = malloc(
			sizeof(struct u_fs_file_directory));
	
	if (get_attr_from_open_pathblock(path, attr) == -1) {
		free(attr);
		return -ENOENT;
	}
	
	if (flag == 1 && attr->flag == 2) {
		free(attr);
		return -EISDIR;
	} else if (flag == 2 && attr->flag == 1) {
		free(attr);
		return -ENOTDIR;
	}
	
	struct u_fs_disk_block* blk_info = malloc(sizeof(struct u_fs_disk_block));
	if (flag == 1) {
		long next_blk = attr->nStartBlock;
		nextClear(next_blk, blk_info);
	} else if (!is_empty(path)) { 
		freePtrs(blk_info, attr, NULL);
		return -ENOTEMPTY;
	}

	attr->flag = 0; 
	if (utils_setattr(path, attr) == -1) {
		freePtrs(blk_info, attr, NULL);
		return -1;
	}

	freePtrs(blk_info, attr, NULL);
	return 0;
}

/**
 * This function should be able to output the running status information into log_path file
 * in order to debug or view.
 *
 *  tag: 标志符
 *  format：字符串格式
 */
void fslog(const char* tag, const char* format, ...) {
	/*if (!logFlag)
		return;
	char* logPath = strdup(log_path);
	char* q = logPath;
	q += strlen(vdisk_path) - 7;
	*q = 'l';
	q++;
	*q = 'o';
	q++;
	*q = 'g';
	q++;
	*q = '\0';
	FILE* fp = NULL;
	fp = fopen(log_path, "at");
	if (fp == NULL) {
		return;
	}
	time_t now;         //实例化time_t结构
	struct tm *timenow;         //实例化tm结构指针
	time(&now);
	//time函数读取现在的时间(国际标准时间非北京时间)，然后传值给now
	timenow = localtime(&now);
	//localtime函数把从time取得的时间now换算成你电脑中的时间(就是你设置的地区)
	fprintf(fp, "%d-%d-%d %d:%d:%d\t", timenow->tm_year + 1900,
			timenow->tm_mon + 1, timenow->tm_mday, timenow->tm_hour,
			timenow->tm_min, timenow->tm_sec);
	//上句中asctime函数把时间转换成字符，通过printf()函数输出
	fprintf(fp, "%s:\t", tag);
	va_list ap;
	va_start(ap, format);
	vfprintf(fp, format, ap);
	va_end(ap);
	free(logPath);
	fclose(fp);*/
}
/**
 * This function should be release one or more pointers.
 *
 * ptr: The pointer to be released
 */
void freePtrs(void*ptr, ...) {
	va_list argp; /* 定义保存函数参数的结构 */
	int argno = 0; /* 纪录参数个数 */
	char *para; /* 存放取出的字符串参数 */
	va_start(argp, ptr);
	free(ptr);
	while (1) {
		para = va_arg(argp, void *); /*    取出当前的参数，类型为char *. */
		if (para == NULL)
			break;
		free(para);
		argno++;
	}
	va_end(argp); /* 将argp置为NULL */
}

//通过路径path获得文件的信息，赋值给stat stbuf

int utils_getattr(const char *path, struct stat *stbuf) {
	fslog("utils_getattr", "path :%s", path);

	struct u_fs_file_directory *attr = malloc(
			sizeof(struct u_fs_file_directory));
	
	if (get_attr_from_open_pathblock(path, attr) == -1) {
		free(attr);
		return -ENOENT;
	}
	memset(stbuf, 0, sizeof(struct stat));
	
	if (attr->flag == 2) {   
		stbuf->st_mode = S_IFDIR | 0666;
	} else if (attr->flag == 1) {   
		stbuf->st_mode = S_IFREG | 0666;
		stbuf->st_size = attr->fsize;
	}
	free(attr);
	return 0;

}
/**
 * This function should look up the input path.
 * ensuring that it's a directory, and then list the contents.
 * return 0 on success
 */
int utils_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset) {

	struct u_fs_disk_block *blk_info;
	struct u_fs_file_directory *attr;
	blk_info = malloc(sizeof(struct u_fs_disk_block));
	attr = malloc(sizeof(struct u_fs_file_directory));

	if (get_attr_from_open_pathblock(path, attr) == -1) { 
		freePtrs(attr, blk_info, NULL);
		return -ENOENT;
	}

	long start_blk = attr->nStartBlock;
	if (attr->flag == 1) {   
		freePtrs(attr, blk_info, NULL);
		return -ENOENT;
	}
	if (get_blkinfo_from_read_blkpos(start_blk, blk_info)) {   
		freePtrs(attr, blk_info, NULL);
		return -ENOENT;
	}

	
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	
	struct u_fs_file_directory *file_dir =
			(struct u_fs_file_directory*) blk_info->data;
	int pos = 0;
	char name[MAX_FILENAME + MAX_EXTENSION + 2];
	while (pos < blk_info->size) {
		strcpy(name, file_dir->fname);
		if (strlen(file_dir->fext) != 0) {
			strcat(name, ".");
			strcat(name, file_dir->fext);
		}
		if (file_dir->flag != 0 && name[strlen(name) - 1] != '~'
				&& filler(buf, name, NULL, 0)) 
			break;
		file_dir++;
		pos += sizeof(struct u_fs_file_directory);
	}

	freePtrs(attr, blk_info, NULL);
	return 0;
}

//将buf里大小为size的内容，写入path指定的起始块后的第offset

int utils_write(const char *path, const char *buf, size_t size, off_t offset) {
	struct u_fs_file_directory *attr = malloc(
			sizeof(struct u_fs_file_directory));
	
	get_attr_from_open_pathblock(path, attr);
	fslog("write", "%s:%d\n", path, attr->fsize);
	if (offset > attr->fsize) {
		free(attr);
		return -EFBIG;
	}

	long start_blk = attr->nStartBlock;
	if (start_blk == -1) {
		free(attr);
		return -errno;
	}

	int res;
	struct u_fs_disk_block *blk_info = malloc(sizeof(struct u_fs_disk_block));
	int para_offset = offset;

	int num;
	
	if ((res = (find_off_blk(&start_blk, &offset, blk_info)))) {
		return res;
	}
	
	char* pt = blk_info->data;
	
	pt += offset;

	int towrite = 0;
	int writen = 0;
	
	towrite = (
	MAX_DATA_IN_BLOCK - offset < size ?
	MAX_DATA_IN_BLOCK - offset :
										size);
	strncpy(pt, buf, towrite);	
	buf += towrite;	
	blk_info->size += towrite;	
	writen += towrite;	
	size -= towrite;	

	
	long* next_blk = malloc(sizeof(long));
	if (size > 0) {
		
		num = get_free_blocks(size / MAX_DATA_IN_BLOCK + 1, next_blk);
		
		if (num == -1) {
			freePtrs(attr, blk_info, next_blk, NULL);
			return -errno;
		}
		blk_info->nNextBlock = *next_blk;
		
		write_blkinfo_start_blk(start_blk, blk_info);
		int i;
		while (1) {
			for (i = 0; i < num; ++i) {
				
				towrite = (MAX_DATA_IN_BLOCK < size ? MAX_DATA_IN_BLOCK : size);
				blk_info->size = towrite;
				strncpy(blk_info->data, buf, towrite);
				buf += towrite;
				size -= towrite;
				writen += towrite;

				if (size == 0)		
					blk_info->nNextBlock = -1;
				else
					
					blk_info->nNextBlock = *next_blk + 1;
				
				write_blkinfo_start_blk(*next_blk, blk_info);
				*next_blk = *next_blk + 1;
			}
			if (size == 0)		
				break;
			
			num = get_free_blocks(size / MAX_DATA_IN_BLOCK + 1, next_blk);
			if (num == -1) {
				freePtrs(attr, blk_info, next_blk, NULL);
				return -errno;
			}
		}
	} else if (size == 0) {
		
		long next_blk;
		next_blk = blk_info->nNextBlock;
		
		blk_info->nNextBlock = -1;
		write_blkinfo_start_blk(start_blk, blk_info);
		
		nextClear(next_blk, blk_info);
	}
	size = writen;

	
	attr->fsize = para_offset + size;
	if (utils_setattr(path, attr) == -1) {
		size = -errno;
	}
	fslog("writesucceed", "%s:%d\n", path, size);
	freePtrs(attr, blk_info, next_blk, NULL);
	return size;
}

//根据路径path找到文件起始位置，再偏移offset长度开始读取数据到buf中，返回文件大小

int utils_read(const char *path, char *buf, size_t size, off_t offset) {

	
	struct u_fs_file_directory *attr = malloc(
			sizeof(struct u_fs_file_directory));

	
	if (get_attr_from_open_pathblock(path, attr) == -1) {
		free(attr);
		return -ENOENT;
	}
	
	if (attr->flag == 2) {
		free(attr);
		return -EISDIR;
	}

	struct u_fs_disk_block *blk_info;		
	blk_info = malloc(sizeof(struct u_fs_disk_block));

	
	if (get_blkinfo_from_read_blkpos(attr->nStartBlock, blk_info) == -1) {
		freePtrs(attr, blk_info, NULL);
		return -1;
	}

	
	if (offset < attr->fsize) {
		if (offset + size > attr->fsize)
			size = attr->fsize - offset;
	} else
		size = 0;
	/*int tmp = size;
	 char *pt = blk_info->data;
	 pt += offset;
	 strcpy(buf, pt);
	 tmp -= blk_info->size;
	 while (tmp > 0) {
	 if (get_blkinfo_from_read_blkpos(blk_info->nNextBlock, blk_info) == -1)
	 break;
	 strcat(buf, blk_info->data);
	 tmp -= blk_info->size;
	 }
	 freePtrs(attr, blk_info, NULL);
	 return size;*/

	int real_offset, blk_num, i, ret = 0;
	long start_blk = blk_info->nNextBlock;

	blk_num = offset / MAX_DATA_IN_BLOCK;
	real_offset = offset % MAX_DATA_IN_BLOCK;

	
	for (i = 0; i < blk_num; i++) {
		if (get_blkinfo_from_read_blkpos(blk_info->nNextBlock, blk_info) == -1
				|| start_blk == -1) {
			printf("read_block_from_pos failed!\n");
			freePtrs(attr, blk_info, NULL);
			return -1;
		}
	}
	
	int temp = size;
	char *pt = blk_info->data;
	pt += real_offset;
	ret = (MAX_DATA_IN_BLOCK - real_offset < size ?
	MAX_DATA_IN_BLOCK - real_offset :
													size);
	memcpy(buf, pt, ret);
	temp -= ret;

	while (temp > 0) {
		if (get_blkinfo_from_read_blkpos(blk_info->nNextBlock, blk_info)
				== -1) {
			printf("read_block_from_pos failed!\n");
			break;
		}
		if (temp > MAX_DATA_IN_BLOCK) {
			memcpy(buf + size - temp, blk_info->data, MAX_DATA_IN_BLOCK);
			temp -= MAX_DATA_IN_BLOCK;
		} else {
			memcpy(buf + size - temp, blk_info->data, temp);
			break;
		}
	}
	freePtrs(attr, blk_info, NULL);
	return size;
}

//按路径打开文件/目录

int utils_mkdir(const char *path, mode_t mode) {
	int res = create_file_dir(path, 2);
	return res;
}

int utils_rmdir(const char *path) {
	int res = rm_file_dir(path, 2);
	return res;
}

int utils_mknod(const char *path, mode_t mode, dev_t rdev) {
	int res = create_file_dir(path, 1);
	return res;
}

int utils_unlink(const char *path) {
	int res = rm_file_dir(path, 1);
	return res;
}

/* Just a stub.  This method is optional and can safely be left
 unimplemented */
int utils_truncate(const char *path, off_t size) {
	(void) path;
	(void) size;
	return 0;
}

/* Just a stub.  This method is optional and can safely be left
 unimplemented */
int utils_flush(const char*path, struct fuse_file_info *fi) {
	(void) path;
	(void) fi;
	return 0;
}

//通过分割路径，读取父目录的信息。p_dir_blk为父目录的第一块子文件的位置

int dv_path(char*name, char*ext, const char*path, long*p_dir_blk, int flag) {
	char*tmp_path, *p, *q;
	tmp_path = strdup(path);
	struct u_fs_file_directory* attr = malloc(
			sizeof(struct u_fs_file_directory));
	p = tmp_path;
	if (!p)
		return -errno;
	
	p++;

	q = strchr(p, '/');
	
	if (flag == 2 && q != NULL)
		return -EPERM;
	else if (q != NULL) {		
		*q = '\0';	
		q++;
		p = q;      
		fslog("dvpath", "%s81\n", tmp_path);
		if (get_attr_from_open_pathblock(tmp_path, attr) == -1) { 
			fslog("dvpath", "%s83\n", tmp_path);
			free(attr);
			return -ENOENT;
		}
	}

	if (q == NULL) { 
		if (get_attr_from_open_pathblock("/", attr) == -1) { 
			fslog("dvpath", "%s90\n", path);
			free(attr);
			return -ENOENT;
		}
	}

	if (flag == 1) { 
		q = strchr(p, '.');
		if (q != NULL) { 
			*q = '\0';
			q++;
		}
	}
	fslog("dvpath", "%s102\n", path);
	
	if (flag == 1) {
		fslog("flag", "%d\n", flag);
		if (strlen(p) > MAX_FILENAME + 1) {
			fslog("create", "namelong\n");
			free(attr);
			return -ENAMETOOLONG;
		} else if (strlen(p) > MAX_FILENAME) {
			if (*(p + MAX_FILENAME) != '~') {
				fslog("create", "namelong\n");
				free(attr);
				return -ENAMETOOLONG;
			}
		} else if (q != NULL) {
			if (strlen(q) > MAX_EXTENSION + 1) {
				fslog("create", "extlong");
				free(attr);
				return -ENAMETOOLONG;
			} else if (strlen(q) > MAX_EXTENSION) {
				if (*(q + MAX_EXTENSION) != '~') {
					fslog("create", "extlong\n");
					free(attr);
					return -ENAMETOOLONG;
				}
			}
		}
	} else if (flag == 2) {
		if (strlen(p) > MAX_FILENAME) {
			fslog("mkdir", "namelong\n");
			free(attr);
			return -ENAMETOOLONG;
		}
	}
	*name = *ext = '\0';
	if (p != NULL)
		strcpy(name, p);
	if (q != NULL)
		strcpy(ext, q);
	free(tmp_path);
	
	*p_dir_blk = attr->nStartBlock;
	free(attr);
	if (*p_dir_blk == -1) {
		return -ENOENT;
	}
	return 0;
}

//遍历目录下的所有文件和目录，如果已存在同名文件或目录，返回
 
int exist_check(struct u_fs_file_directory *file_dir, char *p, char *q,
		int* offset, int* pos, int size, int flag) {
	
	while (*offset < size) {
		if (flag == 0)
			*pos = *offset;
		else if (flag == 1 && file_dir->flag == 1
				&& strcmp(p, file_dir->fname) == 0
				&& ((*q == '\0' && strlen(file_dir->fext) == 0)
						|| (*q != '\0' && strcmp(q, file_dir->fext) == 0)))
			return -EEXIST;
		else if (flag == 2 && file_dir->flag == 2
				&& strcmp(p, file_dir->fname) == 0)
			return -EEXIST;

		file_dir++;
		
		*offset += sizeof(struct u_fs_file_directory);
	}
	
	return 0;
}

//为上层目录增加一个后续块

int enlarge_blk(long p_dir_blk, struct u_fs_file_directory *file_dir,
		struct u_fs_disk_block *blk_info, long *tmp, char*p, char*q, int flag) {
	long blk;
	tmp = malloc(sizeof(long));
	
	if (get_free_blocks(1, tmp) == 1)
		blk = *tmp;
	else {
		freePtrs(p, q, blk_info, NULL);
		return -errno;
	}
	
	free(tmp);
	
	blk_info->nNextBlock = blk;
	
	write_blkinfo_start_blk(p_dir_blk, blk_info);

	blk_info->size = sizeof(struct u_fs_file_directory);
	blk_info->nNextBlock = -1;
	
	file_dir = (struct u_fs_file_directory*) blk_info->data;
	init_file_dir(file_dir, p, q, flag);

	tmp = malloc(sizeof(long));
	if (get_free_blocks(1, tmp) == 1)
		file_dir->nStartBlock = *tmp;
	else {
		return -errno;
	}
	free(tmp);
	
	write_blkinfo_start_blk(blk, blk_info);
	
	new_emp_blk(blk_info);
	write_blkinfo_start_blk(file_dir->nStartBlock, blk_info);
	return 0;
}
//在bitmap中标记块是否被使用

int utils_set_blk(long start_blk, int flag) {

	if (start_blk == -1)
		return -1;
	
	int start = start_blk / 8;
	int left = start_blk % 8;
	int f;

	int mask = 1;
	mask <<= left;

	FILE* fp = NULL;
	fp = fopen(vdisk_path, "r+");
	if (fp == NULL)
		return -1;
	fseek(fp, FS_BLOCK_SIZE + start, SEEK_SET);
	int *tmp = malloc(sizeof(int));
	fread(tmp, sizeof(int), 1, fp);
	f = *tmp;
	if (flag) 
		f |= mask;
	else
		
		f &= ~mask;

	*tmp = f;
	fseek(fp, FS_BLOCK_SIZE + start, SEEK_SET);
	fwrite(tmp, sizeof(int), 1, fp);
	fclose(fp);
	free(tmp);
	return 0;
}
//初始化要创建的文件或目录

void init_file_dir(struct u_fs_file_directory *file_dir, char*name, char*ext,
		int flag) {
	
	strcpy(file_dir->fname, name);
	if (flag == 1 && *ext != '\0')
		strcpy(file_dir->fext, ext);
	file_dir->fsize = 0;
	file_dir->flag = flag;
}
//初始化disk_block单元

void new_emp_blk(struct u_fs_disk_block *blk_info) {
	/* initialize the file or the directory block */
	blk_info->size = 0;
	blk_info->nNextBlock = -1;
	strcpy(blk_info->data, "\0");
}

//判断该path中是否含有子目录或子文件

int is_empty(const char* path) {

	struct u_fs_disk_block *blk_info = malloc(sizeof(struct u_fs_disk_block));
	struct u_fs_file_directory *attr = malloc(
			sizeof(struct u_fs_file_directory));
	
	if (get_attr_from_open_pathblock(path, attr) == -1) {
		freePtrs(blk_info, attr, NULL);
		return 0;
	}
	long start_blk;
	start_blk = attr->nStartBlock;
	if (attr->flag == 1) {     
		freePtrs(blk_info, attr, NULL);
		return 0;
	}
	if (get_blkinfo_from_read_blkpos(start_blk, blk_info) == -1) {     
		freePtrs(blk_info, attr, NULL);
		return 0;
	}

	struct u_fs_file_directory *file_dir =
			(struct u_fs_file_directory*) blk_info->data;
	int pos = 0;
	
	while (pos < blk_info->size) {

		if (file_dir->flag != 0) {
			freePtrs(blk_info, attr, NULL);
			return 0;
		}
		file_dir++;
		pos += sizeof(struct u_fs_file_directory);
	}

	freePtrs(blk_info, attr, NULL);
	return 1;
}
//将attr属性赋值给path所指的文件属性

int utils_setattr(const char* path, struct u_fs_file_directory* attr) {
	int res;
	struct u_fs_disk_block* blk_info = malloc(sizeof(struct u_fs_disk_block));

	fslog("write", "%s:%d\n", path, attr->fsize);

	char *p = malloc(15 * sizeof(char)), *q = malloc(15 * sizeof(char));
	long start_blk;
	
	if ((res = dv_path(p, q, path, &start_blk, 1))) {
		freePtrs(p, q, NULL);
		return res;
	}
	fslog("setattr", "%d\n", start_blk);
	
	if (get_blkinfo_from_read_blkpos(start_blk, blk_info) == -1) {
		res = -1;
		freePtrs(blk_info, NULL);
		return res;
	}
	struct u_fs_file_directory *file_dir =
			(struct u_fs_file_directory*) blk_info->data;
	int offset = 0;
	while (offset < blk_info->size) {
		
		if (file_dir->flag != 0 && strcmp(p, file_dir->fname) == 0
				&& (*q == '\0' || strcmp(q, file_dir->fext) == 0)) {     
			
			set_file_dir(attr, file_dir);
			res = 0;
			fslog("set", "find and set\n");
			break;
		}
		
		file_dir++;
		offset += sizeof(struct u_fs_file_directory);
	}
	freePtrs(p, q, NULL);
	
	if (write_blkinfo_start_blk(start_blk, blk_info) == -1) {
		res = -1;
	}
	fslog("set", "write back\n");
	freePtrs(blk_info, NULL);
	return res;
}
//读出src_的属性到dest_

void set_file_dir(struct u_fs_file_directory *src_,
		struct u_fs_file_directory *dest_) {
	strcpy(dest_->fname, src_->fname);
	strcpy(dest_->fext, src_->fext);
	dest_->fsize = src_->fsize;
	dest_->nStartBlock = src_->nStartBlock;
	dest_->flag = src_->flag;
}

//从next_blk起清空blk_info后续块

void nextClear(long next_blk, struct u_fs_disk_block* blk_info) {
	while (next_blk != -1) {
		utils_set_blk(next_blk, 0);
		get_blkinfo_from_read_blkpos(next_blk, blk_info);
		next_blk = blk_info->nNextBlock;
	}
}
//根据起始块和偏移量寻找位置

int find_off_blk(long*start_blk, long*offset, struct u_fs_disk_block *blk_info) {
	while (1) {
		if (get_blkinfo_from_read_blkpos(*start_blk, blk_info) == -1) {
			return -errno;
		}
		
		if (*offset <= blk_info->size)
			break;
		
		*offset -= blk_info->size;
		*start_blk = blk_info->nNextBlock;
	}
	return 0;
}
