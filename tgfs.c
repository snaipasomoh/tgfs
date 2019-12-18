//gcc tgfs.c -o tgfs `pkg-config fuse3 --cflags --libs`
#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>

static struct tg_file {
	const char *name;
	char *content;
} tgf_state, tgf_food, tgf_poo;

static void *tgfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg){
	(void) conn;
	cfg->kernel_cache = 1;
	return NULL;
}

static int tgfs_getattr (const char *path, struct stat *st,
							struct fuse_file_info *fi){
	(void) fi;
	memset(st, 0, sizeof(struct stat));
	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime = time(NULL);
	st->st_mtime = time(NULL);

	if (strcmp(path, "/") == 0){
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
	}
	else if (strcmp(path+1, tgf_state.name) == 0){
		st->st_mode = S_IFREG | 0444;
		st->st_nlink = 1;
		st->st_size = strlen(tgf_state.content);
	}
	else if (strcmp(path+1, tgf_poo.name) == 0){
		st->st_mode = S_IFREG | 0222;
		st->st_nlink = 1;
		st->st_size = 0;
	}
	else if (strcmp(path+1, tgf_food.name) == 0){
		st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;
		st->st_size = 0;
	}
	else{
		return -ENOENT;
	}
	return 0;
}

static int tgfs_readdir (const char *path, void *buffer, fuse_fill_dir_t filler,
						off_t offset, struct fuse_file_info *fi,
						enum fuse_readdir_flags flags){
	(void) offset;
	(void) fi;
	(void) flags;


	filler(buffer, ".", NULL, 0, 0);
	filler(buffer, "..", NULL, 0, 0);
	if (strcmp(path, "/") != 0){
			return -ENOENT;
	}
	filler(buffer, tgf_state.name, NULL, 0, 0);
	filler(buffer, tgf_poo.name, NULL, 0, 0);
	return 0;
}

static int tgfs_open(const char *path, struct fuse_file_info *fi){
	if (strcmp(path+1, tgf_state.name) != 0){
		return -ENOENT;
	}
	// if ((fi->flags & O_ACCMODE) != O_RDONLY){
	// 	return -EACCES;
	// }
	return 0;
}

static int tgfs_read (const char *path, char *buffer, size_t size, off_t offset,
					struct fuse_file_info *fi){
	(void) fi;
	if (strcmp(path+1, tgf_state.name) != 0){
		return -ENOENT;
	}
	size_t len = strlen(tgf_state.content);
	if (offset < len){
		if (offset + size > len){
			size = len - offset;
		}
		memcpy(buffer, tgf_state.content + offset, size);
	}
	else{
		size = 0;
	}
	return size;
}

static int tgfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	if (strcmp(path+1, tgf_food.name) != 0){
		return -ENOENT;
	}

	if (mknod(path, mode, rdev) == -1)
		return -errno;

	return 0;
}

static int tgfs_unlink(const char *path){
	printf("Path: %s\n", path);
	if (unlink(path) == -1){
		return -errno;
	}
	return 0;
}

static struct fuse_operations tg_operations = {
	.init = tgfs_init,
	.getattr = tgfs_getattr,
	.readdir = tgfs_readdir,
	.open = tgfs_open,
	.read = tgfs_read,
	.mknod = tgfs_mknod,
	.unlink = tgfs_unlink,
};

int main (int argc, char **argv){
	tgf_state.name = strdup("state");
	tgf_state.content = strdup("Food: ####.\nDirt: ##...\n");
	tgf_poo.name = strdup("poo");
	tgf_food.name = strdup("food");
	return fuse_main(argc, argv, &tg_operations, NULL);
}
