/**
 * finding_filesystems
 * CS 341 - Spring 2024
 */
#include "minixfs.h"
#include "minixfs_utils.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define MIN(x, y) (x < y ? x : y)

/**
 * Virtual paths:
 *  Add your new virtual endpoint to minixfs_virtual_path_names
 */
char *minixfs_virtual_path_names[] = {"info", /* add your paths here*/};

/**
 * Forward declaring block_info_string so that we can attach unused on it
 * This prevents a compiler warning if you haven't used it yet.
 *
 * This function generates the info string that the virtual endpoint info should
 * emit when read
 */
static char *block_info_string(ssize_t num_used_blocks) __attribute__((unused));
static char *block_info_string(ssize_t num_used_blocks)
{
    char *block_string = NULL;
    ssize_t curr_free_blocks = DATA_NUMBER - num_used_blocks;
    asprintf(&block_string,
             "Free blocks: %zd\n"
             "Used blocks: %zd\n",
             curr_free_blocks, num_used_blocks);
    return block_string;
}

// Don't modify this line unless you know what you're doing
int minixfs_virtual_path_count =
    sizeof(minixfs_virtual_path_names) / sizeof(minixfs_virtual_path_names[0]);

int minixfs_chmod(file_system *fs, char *path, int new_permissions)
{
    // try to get inode at path and check for error
    inode *nody = get_inode(fs, path);
    if (!nody)
    {
        errno = ENOENT;
        return -1;
    }

    // bitmask to apply new permissions
    nody->mode &= (~0x1FF);                  // zeros bottom 9 bits
    nody->mode |= (new_permissions & 0x1FF); // copies bottom 9 from new_permissions

    // update ctim
    clock_gettime(CLOCK_REALTIME, &nody->ctim);

    return 0;
}

int minixfs_chown(file_system *fs, char *path, uid_t owner, gid_t group)
{
    // try to get inode at path and check for error
    inode *nody = get_inode(fs, path);
    if (!nody)
    {
        errno = ENOENT;
        return -1;
    }

    // update uid and gid if applicable
    if (owner != ((uid_t)-1))
    {
        nody->uid = owner;
    }
    if (group != ((gid_t)-1))
    {
        nody->gid = group;
    }

    // update ctim
    clock_gettime(CLOCK_REALTIME, &nody->ctim);

    return 0;
}

inode *minixfs_create_inode_for_path(file_system *fs, const char *path)
{
    // preliminary 'path' check:
    if (path == NULL)
        return NULL;

    // check if inode already exists (it shouldn't)
    inode *nody = get_inode(fs, path);
    if (nody != NULL)
    {
        errno = EEXIST;
        return NULL;
    }

    // create file located at path

    // get parent inode ('fn' will contain filename after calling parent_directory)
    char *fn = NULL;
    const char **fnp = (const char **)&fn;
    inode *parent = parent_directory(fs, path, fnp);

    // filename validity check
    if (valid_filename(fn) != 1)
    {
        errno = ENOENT;
        return NULL;
    }

    // get OFFSET of first available inode (and error check)
    inode_number num = first_unused_inode(fs);
    if (num == -1)
    {
        errno = ENOSPC;
        return NULL;
    }

    // retrieve inode corresponding to 'num' and initialize to parent
    nody = fs->inode_root + num;
    init_inode(parent, nody);

    // get data block to associate with inode number

    // 'parent' is a DIRECTORY inode,
    // so its data_block_numbers refer to FILE inodes and not actual data
    // and each of its FILE inodes begins w/ metadata that
    // corresponds to the string result from make_string_from_dirent (filename and inode_num)

    // number of FULL blocks
    size_t parent_blocks_full = parent->size / sizeof(data_block);

    // offset of any PARTIAL block
    size_t parent_blocks_partial_offset = parent->size % sizeof(data_block);

    // FIRST: we need a data_block_number for storing info about the new inode
    data_block_number db_num;

    if (parent_blocks_full < NUM_DIRECT_BLOCKS)
    { // add to direct blocks

        // Note: if offset is not zero, there should always be at least 256 bytes available
        // to write the entry string (16KB / 256 == space for 64 entries per data_block)
        if (parent_blocks_partial_offset != 0)
        { // partial available
            db_num = parent->direct[parent_blocks_full];
        }

        else
        { // no existing/partial available, start new
            db_num = add_data_block_to_inode(fs, parent);
            if (db_num == -1)
            {
                errno = ENOSPC;
                return NULL;
            }
        }
    }

    else
    { // add to indirect blocks

        // first check if we still need to populate the indirect block
        if (parent->indirect == UNASSIGNED_NODE)
        {
            if (add_single_indirect_block(fs, parent) == -1)
            {
                errno = ENOSPC;
                return NULL;
            }
        }

        // get start of parent's indirect block
        // cast as data_block_number * to allow pointer arithmetic
        data_block_number *indir_blocks = (data_block_number *)(fs->data_root + (parent->indirect));

        if (parent_blocks_partial_offset != 0)
        { // partial available
            db_num = *(indir_blocks + parent_blocks_full - NUM_DIRECT_BLOCKS);
        }
        else
        { // no existing/partial available, start new
            db_num = add_data_block_to_indirect_block(fs, indir_blocks);
            if (db_num == -1)
            {
                errno = ENOSPC;
                return NULL;
            }
        }
    }

    // so now db_num is OFFSET corresponding to data_block we need to write to
    data_block *current_block = fs->data_root + db_num;

    // SECOND: we need to store the (filename + inode_num) string representing the new inode
    minixfs_dirent d;
    d.name = fn;
    d.inode_num = num;

    make_string_from_dirent((char *)current_block + parent_blocks_partial_offset, d);

    // increase parent's size
    parent->size += FILE_NAME_ENTRY;

    return nody;
}

ssize_t minixfs_virtual_read(file_system *fs, const char *path, void *buf,
                             size_t count, off_t *off)
{

    // printf("inside virtual_read function...\n");

    if (!strcmp(path, "info"))
    {
        // printf("match for 'info'...\n");

        // generate the block_info_string
        // copy a certain number of bytes of string
        // from a desired offset to the user buffer

        // computer number of USED blocks (below function calculates the frees)
        // GET_DATA_MAP tracks 'used' blocks (1 for used, 0 otherwise)
        // Example usage: GET_DATA_MAP(fs_pointer->meta)[data_block_number]
        // fs->meta is a pointer to the superblock
        // fs->meta->dblock_count is total number of data blocks
        ssize_t num_used = 0;

        for (uint64_t i = 0; i < fs->meta->dblock_count; i++)
        {
            if (GET_DATA_MAP(fs->meta)[i] == 1)
            {
                num_used++;
            }
        }

        // (see implementation at top of page)
        char *bis = block_info_string(num_used);

        size_t num_to_copy = count;

        // if offset is >= strlen(bis), no bytes left to copy
        if ((size_t)*off >= strlen(bis))
        {
            return 0;
        }

        // num_to_copy is minimum between (strlen(bis) - *off) and count
        if (strlen(bis) - *off < count)
        {
            num_to_copy = strlen(bis) - *off;
        }

        memcpy(buf, bis + *off, num_to_copy);
        *off += num_to_copy;

        return num_to_copy;
    }

    errno = ENOENT;
    return -1;
}

ssize_t minixfs_write(file_system *fs, const char *path, const void *buf,
                      size_t count, off_t *off)
{
    // try to get inode at path
    inode *nody = get_inode(fs, path);

    // if it doesn't exist, create
    if (!nody)
    {
        nody = minixfs_create_inode_for_path(fs, path);

        // check for error
        if (!nody)
        {
            errno = ENOENT;
            return -1;
        }
    }

    // check that write request (*off + count) is within bounds of maximum file size
    // 11 'direct' blocks, all of max size 16KB
    // 'NUM_INDIRECT_BLOCKS' 'indirect' blocks, all pointing to data_blocks of max size 16KB
    if ((size_t)*off + count > (16 * KILOBYTE * (NUM_DIRECT_BLOCKS + NUM_INDIRECT_BLOCKS)))
    {
        errno = ENOSPC;
        return -1;
    }

    // calculate how many blocks are needed (including any partial)
    size_t block_count = ((size_t)*off + count) / sizeof(data_block);
    if (((size_t)*off + count) % sizeof(data_block) != 0)
        block_count++;

    // Note: will return 0 if already has sufficient number of blocks (like when file is bigger)
    // use 'min_blockcount' function to allocate blocks of memory for nodes
    // this should ensure that I don't have to 'add' direct or indirect blocks within the write loop (!)
    int mbc = minixfs_min_blockcount(fs, path, (int)block_count);
    if (mbc == -1)
    {
        errno = ENOSPC;
        return -1;
    }

    // calculate which block to start at, and adjust offset to disregard any skipped blocks
    size_t block_idx = *off / sizeof(data_block);
    size_t new_offset = *off % sizeof(data_block);
    if (new_offset != 0)
    {
        block_idx++;
    }

    data_block_number *indir_blocks = (data_block_number *)(fs->data_root + (nody->indirect));
    data_block *current;
    data_block_number *indir_dbn;
    int first = 1;
    size_t write_size;
    size_t bytes_written = 0;

    while (bytes_written < count)
    {
        if (block_idx < NUM_DIRECT_BLOCKS)
        { // write to DIRECT blocks
            current = fs->data_root + (nody->direct[block_idx]);
        }
        else
        { // write to INDIRECT blocks
            indir_dbn = indir_blocks + (block_idx - NUM_DIRECT_BLOCKS);
            current = fs->data_root + (*indir_dbn);
        }

        if (first)
        {
            write_size = sizeof(data_block) - new_offset;

            // check for partial fill
            if (write_size > count)
            {
                write_size = count;
            }

            memcpy(current->data + new_offset, buf, write_size);
            bytes_written += write_size;
            first = 0;
            write_size = sizeof(data_block);
        }
        else
        {
            // check for partial fill (should only happen for a last partial block)
            if (write_size > (count - bytes_written))
            {
                write_size = count - bytes_written;
            }

            memcpy(current->data, buf + bytes_written, write_size);
            bytes_written += write_size;
        }

        block_idx++;
    }

    // update 'atim' and 'mtim'
    clock_gettime(CLOCK_REALTIME, &nody->atim);
    clock_gettime(CLOCK_REALTIME, &nody->mtim);

    // if file size increases b/c of write, update inode's 'size'
    if (*off + count > nody->size)
    {
        nody->size = *off + count;
    }

    // update offset
    *off += bytes_written;

    return (ssize_t)bytes_written;
}

ssize_t minixfs_read(file_system *fs, const char *path, void *buf, size_t count,
                     off_t *off)
{
    const char *virtual_path = is_virtual_path(path);
    if (virtual_path)
        return minixfs_virtual_read(fs, virtual_path, buf, count, off);

    // try to get inode at path, check for error
    inode *nody = get_inode(fs, path);
    if (!nody)
    {
        errno = ENOENT;
        return -1;
    }

    // validate offset is within file size (type uint64_t)
    if ((uint64_t)*off >= nody->size)
    {
        return 0;
    }

    // calculate which data_block to start at based on provided offset
    // and adjust the offset to disregard any skipped data_blocks
    size_t block_idx = *off / sizeof(data_block);
    size_t new_offset = *off % sizeof(data_block);

    // TOTAL bytes to read
    size_t bytes_to_read = MIN(count, nody->size - *off);

    size_t bytes_read = 0;
    data_block_number *indir_blocks = (data_block_number *)(fs->data_root + (nody->indirect));
    data_block *current;
    data_block_number *indir_dbn;
    int first = 1;
    size_t read_size;

    while (bytes_read < bytes_to_read)
    {
        if (block_idx < NUM_DIRECT_BLOCKS)
        { // read to DIRECT blocks
            current = fs->data_root + (nody->direct[block_idx]);
        }
        else
        { // read to INDIRECT blocks
            indir_dbn = indir_blocks + (block_idx - NUM_DIRECT_BLOCKS);
            current = fs->data_root + (*indir_dbn);
        }

        if (first)
        {
            read_size = sizeof(data_block) - new_offset;

            // check for partial fill
            if (read_size > bytes_to_read)
            {
                read_size = bytes_to_read;
            }

            memcpy(buf, current->data + new_offset, read_size);
            first = 0;
            bytes_read += read_size;
            read_size = sizeof(data_block);
        }
        else
        {
            // check for partial fill (should only happen once on a final partial block)
            if (read_size > (bytes_to_read - bytes_read))
            {
                read_size = bytes_to_read - bytes_read;
            }

            memcpy(buf + bytes_read, current->data, read_size);
            bytes_read += read_size;
        }

        block_idx++;
    }

    // increment *off by total number of bytes read
    *off += bytes_read;

    // update 'atim'
    clock_gettime(CLOCK_REALTIME, &nody->atim);

    // return number of bytes read (as ssize_t)
    return (ssize_t)bytes_read;
}
