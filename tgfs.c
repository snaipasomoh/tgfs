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
#include <math.h>

const int CH_TIME = 10;
const int PCR_TIME = 20;

static struct tgfs_file{
	const char *name;
	char *content;
} tgf_state, tgf_food, tgf_poo, tgf_dead;

static struct tgfs_dir{
	int poo_exists;
	int food_exists;
	int dead_exists;
} dir_state = {.poo_exists = 0, .food_exists = 0, .dead_exists = 0};

static struct tgfs_state{
	int food;
	int dirt;
	time_t last_food;
	time_t last_poo;
} tg_state = {.food = 5, .dirt = 0};

void state_to_cont (){
	free(tgf_state.content);
	char food[32];
	char dirt[32];
	strcpy(food, "Food: ");
	strcpy(dirt, "Dirt: ");
	for (int i = 0; i < 5; i++){
		if (i < tg_state.food){
			strcat(food, "#");
		}
		else{
			strcat(food, ".");
		}
		if (i < tg_state.dirt){
			strcat(dirt, "#");
		}
		else{
			strcat(dirt, ".");
		}
	}
	strcat(food, "\n");
	strcat(dirt, "\n");

	char new_cnt[64];
	strcpy(new_cnt, food);
	strcat(new_cnt, dirt);
	tgf_state.content = strdup(new_cnt);
}

void update_state (){
	if (dir_state.dead_exists){
		return;
	}

	time_t now = time(NULL);
	int ft = now - tg_state.last_food;
	int pt = now - tg_state.last_poo;

	if (ft >= CH_TIME){
		if ((tg_state.food -= ft / CH_TIME) < 0){
			tg_state.food = 0;
		}
		tg_state.last_food += CH_TIME * (ft / CH_TIME);
	}

	if (!dir_state.poo_exists && (pt >= PCR_TIME)){
		printf("poo\n");
		dir_state.poo_exists = 1;
		tg_state.last_poo += PCR_TIME;
	}
	if (dir_state.poo_exists && (pt >= CH_TIME)){
		printf("poo is here\n");
		printf("%d\n", tg_state.dirt);
		if ((tg_state.dirt += (now - tg_state.last_poo) / CH_TIME) > 5){
			tg_state.dirt = 5;
		}
		tg_state.last_poo += CH_TIME * ((now - tg_state.last_poo) / CH_TIME);
	}

	if (tg_state.food == 0 && tg_state.dirt == 5){
		dir_state.dead_exists = 1;
		if (now - tg_state.last_food > now - tg_state.last_poo){
			printf("here\n");
			tgf_dead.content = strdup ("I am dead, because it was no food\n");
		}
		else{
			tgf_dead.content = strdup ("I am dead, because it was too dirty\n");
		}
	}
	else if (tg_state.food == 0){
		printf("foo\n");
		dir_state.dead_exists = 1;
		tgf_dead.content = strdup ("I am dead, because it was no food\n");
	}
	else if (tg_state.dirt == 5){
		dir_state.dead_exists = 1;
		tgf_dead.content = strdup ("I am dead, because it was too dirty\n");
	}
	else{
		if (dir_state.food_exists){
			dir_state.food_exists = 0;
			tg_state.food = 5;//+= (tg_state.food < 5) ? 1 : 0;
			tg_state.last_food = time(NULL);
		}
		if (!dir_state.poo_exists){
			printf("wtf\n");
			tg_state.dirt = 0;//+= (tg_state.dirt < 5) ? 1 : 0;
		}
	}

	state_to_cont();
}

static void *tgfs_init (struct fuse_conn_info *conn, struct fuse_config *cfg){
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
	else if (strcmp(path+1, tgf_dead.name) == 0){
		st->st_mode = S_IFREG | 0444;
		st->st_nlink = 1;
		st->st_size = strlen(tgf_dead.content);
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

	if (strcmp(path, "/") != 0){
			return -ENOENT;
	}

	filler(buffer, ".", NULL, 0, 0);
	filler(buffer, "..", NULL, 0, 0);


	update_state();

	filler(buffer, tgf_state.name, NULL, 0, 0);

	if (dir_state.dead_exists){
		filler(buffer, tgf_dead.name, NULL, 0, 0);
	}
	if (dir_state.poo_exists){
		filler(buffer, tgf_poo.name, NULL, 0, 0);
	}
	if (dir_state.food_exists){
		filler(buffer, tgf_food.name, NULL, 0, 0);
	}
	return 0;
}

static int tgfs_utimens (const char *path, const struct timespec tv[2],
						struct fuse_file_info *fi){

	if (strcmp(path+1, tgf_food.name) != 0){
		return -ENOENT;
	}

	if (dir_state.food_exists){
		return -ENOENT;
	}

	dir_state.food_exists = 1;
	update_state();
	return 0;
}

static int tgfs_read (const char *path, char *buffer, size_t size, off_t offset,
					struct fuse_file_info *fi){
	(void) fi;
	size_t len;
	char *cnt;
	if (strcmp(path+1, tgf_state.name) == 0){
		update_state();
		len = strlen(tgf_state.content);
		cnt = tgf_state.content;
	}
	else if (strcmp(path+1, tgf_dead.name) == 0){
		len = strlen(tgf_dead.content);
		cnt = tgf_dead.content;
	}
	else{
		return -ENOENT;
	}

	if (offset < len){
		if (offset + size > len){
			size = len - offset;
		}
		memcpy(buffer, cnt + offset, size);
	}
	else{
		size = 0;
	}
	return size;
}

static int tgfs_mknod (const char *path, mode_t mode, dev_t rdev){

	// if (strcmp(path+1, "fp") == 0){
	// 	if (tg_state.food < 5){
	// 		tg_state.food++;
	// 	}
	// }
	// if (strcmp(path+1, "fm") == 0){
	// 	if (tg_state.food > 0){
	// 		tg_state.food--;
	// 	}
	// }
	// if (strcmp(path+1, "dp") == 0){
	// 	if (tg_state.dirt < 5){
	// 		tg_state.dirt++;
	// 	}
	// }
	// if (strcmp(path+1, "dm") == 0){
	// 	if (tg_state.dirt > 0){
	// 		tg_state.dirt++;
	// 	}
	// }
	// if (strcmp(path+1, "us") == 0){
	// 	update_state();
	// }

	if (strcmp(path+1, tgf_food.name) != 0){
		return -ENOENT;
	}

	if (dir_state.food_exists){
		return -ENOENT;
	}

	dir_state.food_exists = 1;
	update_state();

	return 0;
}

static int tgfs_unlink (const char *path){
	if (strcmp(path+1, tgf_poo.name) != 0){
		return -ENOENT;
	}

	if (!dir_state.poo_exists){
		return -ENOENT;
	}

	dir_state.poo_exists = 0;
	update_state();
	return 0;
}

static struct fuse_operations tg_operations = {
	.init = tgfs_init,
	.getattr = tgfs_getattr,
	.readdir = tgfs_readdir,
	.utimens = tgfs_utimens,
	// .open = tgfs_open,
	.read = tgfs_read,
	.mknod = tgfs_mknod,
	.unlink = tgfs_unlink,
};

int main (int argc, char **argv){
	tgf_state.name = strdup("state");
	state_to_cont(tgf_state.content, &tg_state);
	tgf_dead.name = strdup("DEAD");
	tgf_dead.content = strdup("");
	tgf_poo.name = strdup("poo");
	tgf_food.name = strdup("food");
	tg_state.last_food = time(NULL);
	tg_state.last_poo = time(NULL);
	int ret = fuse_main(argc, argv, &tg_operations, NULL);
	free((void *)tgf_state.name);
	free((void *)tgf_state.content);
	free((void *)tgf_dead.name);
	free((void *)tgf_dead.content);
	free((void *)tgf_poo.name);
	free((void *)tgf_food.name);
	return ret;
}
