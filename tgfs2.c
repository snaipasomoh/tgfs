//gcc tgfs2.c -o tgfs2 `pkg-config fuse3 --cflags --libs`
#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <dirent.h>
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

char *MNT_PATH;

static struct tg_file {
	const char *name;
	char *content;
} tgf_state, tgf_food, tgf_poo;

static void *tgfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg){
	(void) conn;
	cfg->kernel_cache = 1;
	mknod("/state", S_IFREG | 0444, 0);
	int fd = open("/state", O_WRONLY);
	// if (fd == -1){
	// 	return errno;
	// }
	int res = pwrite(fd, tgf_state.content, strlen(tgf_state.content), 0);
	// if (res == -1){
	// 	return errno;
	// }
	close(fd);
	return NULL;
}

static int tgfs_getattr(const char *path, struct stat *stbuf,
						struct fuse_file_info *fi){
	(void) fi;
	int res;

	res = lstat(path, stbuf);
	if (res == -1){
		return -errno;
	}
	return 0;
}

static int tgfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
						off_t offset, struct fuse_file_info *fi,
						enum fuse_readdir_flags flags){
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;
	(void) flags;

	dp = opendir(MNT_PATH);
	if (dp == NULL){
		return -errno;
	}
	while ((de = readdir(dp)) != NULL){
		if (filler(buf, de->d_name, NULL, 0, 0)){
			break;
		}
	}

	closedir(dp);
	return 0;
}

static int tgfs_open(const char *path, int flags){
	int res;

	res = open(path, flags);
	if (res == -1){
		return -errno;
	}
	close(res);
	return 0;
}

static int tgfs_read(const char *path, char *buf, size_t size, off_t offset,
					struct fuse_file_info *fi){
	int fd;
	int res;

	if (fi == NULL){
		fd = open(path, O_RDONLY);
	}
	else{
		fd = fi->fh;
	}

	if (fd == -1){
		return -errno;
	}

	res = pread(fd, buf, size, offset);
	if (res == -1){
		res = -errno;
	}

	if (fi == NULL){
		close(fd);
	}
	return res;
}

static int tgfs_mkdir(const char *path, mode_t mode)
{
	int res;

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static struct fuse_operations tg_operations = {
	.init = tgfs_init,
	.getattr = tgfs_getattr,
	.readdir = tgfs_readdir,
	.mkdir = tgfs_mkdir,
	// .open = tgfs_open,
	.read = tgfs_read,
	// .mknod = tgfs_mknod,
	// .unlink = tgfs_unlink,
};

int main (int argc, char **argv){
	tgf_state.name = strdup("state");
	tgf_state.content = strdup("Food: ####.\nDirt: ##...\n");
	tgf_poo.name = strdup("poo");
	tgf_food.name = strdup("food");
	MNT_PATH = strdup(argv[1]);
	return fuse_main(argc, argv, &tg_operations, NULL);
}
