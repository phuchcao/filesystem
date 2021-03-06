
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include <iostream>
#include <string>

#include "fslib.hpp"
#include "vfs.hpp"
#include "mysh.hpp"

using namespace std;



int f_open(Vnode *vn, const char *filename, int flags) {

}

int find_fat(unsigned short& start, int& offset) {
	int n = offset / BLOCKSIZE + (offset % BLOCKSIZE ? 1 : 0);
	unsigned short res = start;
	for (int i = 0; i < n - 1; i++) {
		res = g_FAT_table[res];
	}
	start = res;
	offset %= BLOCKSIZE;
	return 1;
}

size_t 	f_read(Vnode *vn, void *data, size_t size, int num, int fd) {
	int res;

	if (fd >= MAXFTSIZE) {
		// errno
		return 0;
	}

	FtEntry entry = g_file_table[fd];
	if (entry.index == -1) {
		// errno
		return 0;
	}

	if (! (entry.flag & READ) || ! (entry.flag & RDWR)) {
		// errno
		return 0;
	}

	vnode* vn = entry.vn;
	int fat_pos = vn->fatPtr;
	int offs = entry.offset;

	find_fat(fat_pos, offs);

	int i = 0, buf_pos = 0;
	do {
		res = lseek(g_disk_fd, g_superblock.data_offset * fat_pos + offs, SEEK_SET);
		if (res == -1) {
			// errno
			return 0;
		}

		if (size < BLOCKSIZE - offs) {
			res = read(g_disk_fd, (char*)data + buf_pos, size);
			if (res == -1) {
				// errno
				return i;
			}

			buf_pos += res;
			offs += res;
			entry.offset += res;

			
		}
		else {
			int size_cur_block = BLOCKSIZE - offs;
			res = read(g_disk_fd, (char*)data + buf_pos, size_cur_block);
			if (res == -1) {
				// errno
				return i;
			}

			buf_pos += res;
			offs += res;
			entry.offset += res;

			if (g_FAT_table[fat_pos] == -1) {
				break;
			}

			fat_pos = g_FAT_table[fat_pos];
			offs = 0;
			res = lseek(g_disk_fd, g_superblock.data_offset * fat_pos, SEEK_SET);
			if (res == -1) {
				// errno
				return 0;
			}

			res = read(g_disk_fd, (char*)data + buf_pos, size - size_cur_block);
			if (res == -1) {
				// errno
				return i;
			}

			buf_pos += res;
			offs += res;
			entry.offset += res;
		}

		i++;

	} while (i < num);

	return i;
}

size_t 	f_write(Vnode *vn, void *data, size_t size, int num, int fd) {
	int res;

	if (fd >= MAXFTSIZE) {
		// errno
		return 0;
	}

	FtEntry entry = g_file_table[fd];
	if (entry.index == -1) {
		// errno
		return 0;
	}

	if (! (entry.flag & WRITE) || ! (entry.flag & RDWR)) {
		// errno
		return 0;
	}

	vnode* vn = entry.vn;
	int fat_pos = vn->fatPtr;
	int offs = entry.offset;

	find_fat(fat_pos, offs);

	int i = 0, buf_pos = 0;
	do {
		res = lseek(g_disk_fd, g_superblock.data_offset * fat_pos + offs, SEEK_SET);
		if (res == -1) {
			// errno
			return 0;
		}

		if (size < BLOCKSIZE - offs) {
			res = write(g_disk_fd, (char*)data + buf_pos, size);
			if (res == -1) {
				// errno
				return i;
			}

			buf_pos += res;
			offs += res;
			entry.offset += res;

			
		}
		else {
			int size_cur_block = BLOCKSIZE - offs;
			res = write(g_disk_fd, (char*)data + buf_pos, size_cur_block);
			if (res == -1) {
				// errno
				return i;
			}

			buf_pos += res;
			offs += res;
			entry.offset += res;

			if (g_FAT_table[fat_pos] == -1 && size - size_cur_block > 0) {
				int free_fat = getNextFreeBlock();
				g_FAT_table[fat_pos] = free_fat;
				g_FAT_table[free_fat] = -1;
			}

			fat_pos = g_FAT_table[fat_pos];
			offs = 0;
			res = lseek(g_disk_fd, g_superblock.data_offset * fat_pos, SEEK_SET);
			if (res == -1) {
				// errno
				return 0;
			}

			res = write(g_disk_fd, (char*)data + buf_pos, size - size_cur_block);
			if (res == -1) {
				// errno
				return i;
			}

			buf_pos += res;
			offs += res;
			entry.offset += res;
		}

		i++;

	} while (i < num);

	return i;
}



int f_seek(Vnode *vn, int offset, int whence, int fd) {
	FtEntry* entry = g_file_table.getFileEntry(fd);
	if (! entry) {
		//errno
		return -1;
	}

	Vnode* cur_node = entry->vn;

	if (whence == S_CUR) {
		offset += entry->offset;
	}
	else if (whence == S_END) {
		offset = entry->offset - offset; 
	}

	if (offset <= 0) {
		entry->offset = 0;
		return 0;
	}

	if (offset >= cur_node->size) {
		entry->offset = cur_node->size;
		return 0;
	}

	entry->offset = offset;
	return 0;
}



int f_rewind(Vnode *vn, int fd) {
	return f_seek(vn, 0, S_SET, fd);
}



int	f_stat(Vnode *vn, Stat *buf, int fd) {
	FtEntry* entry = g_file_table.getFileEntry(fd);
	if (! entry) {
		//errno
		return -1;
	}

	Vnode* vnode = entry->vnode;
	strncpy(buf->name, vnode->name, 255);
	buf->uid = vnode->uid;
	buf->gid = vnode->gid;
	buf->size = vnode->size;
	buf->permission = vnode->permission;
	buf->type = vnode->type;
	buf->timestamp = vnode->timestamp;

	return 0;
}


