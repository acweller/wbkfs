/*
 * WBkFS: Sistema de arquivos de brinquedo que realiza backup dos arquivos gravados.
 *    Baseado nos sistemas de arquivos: LwnFS, IsleneFS e GOGIsleneFS.
 * 
 * Obs.: Código atualizado para rodar com kernel 3.10.x.
 *
 * Detalhes da implementação:
 * - inode->i_private - Armazena o conteúdo do arquivo.
 * - inode->i_size    - Armazena o número de bytes gravados no arquivo.
 *                      Obs.: Por algum motivo (???) no método Write o valor aparece zerado.
 *
 * Autor: Anderson Coelho Weller
 * Data : 2º Semestre 2013
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/vfs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/current.h>
#include <asm/uaccess.h>


// Os inodes sao atribuidos em ordem crescente, sem reaproveitamento
static int inode_number = 0;


// Protótipos das funções.
struct inode *wbkfs_find_bkp(struct file *file, const char *name);
static struct inode *wbkfs_create_backup(struct super_block *sb, struct dentry *dir, const char *name);
static struct inode *wbkfs_make_inode(struct super_block *sb, int mode);


static const struct super_operations wbkfs_ops = {
        .statfs		= simple_statfs,
        .drop_inode	= generic_delete_inode,
};

// Operações sobre arquivos
ssize_t wbkfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos);
ssize_t wbkfs_write(struct file *file, const char __user *buf, size_t count, loff_t *pos);
static int wbkfs_open(struct inode *inode, struct file *file);
const struct file_operations wbkfs_file_operations = {
	.read		= wbkfs_read,
	.write		= wbkfs_write,
	.open		= wbkfs_open,
};
static const struct inode_operations wbkfs_file_inode_operations = {
	.getattr	= simple_getattr,
};


// Operações sobre diretórios
static int wbkfs_create (struct inode *dir, struct dentry * dentry, umode_t mode, bool excl);
static int wbkfs_link(struct dentry * old_dentry, struct inode * dir, struct dentry * dentry);
static int wbkfs_unlink(struct inode * dir, struct dentry *dentry );
static int wbkfs_symlink(struct inode * dir, struct dentry *dentry, const char * symname);
static int wbkfs_mkdir(struct inode * dir, struct dentry *dentry, umode_t mode);
//static int wbkfs_rmdir(struct inode * dir, struct dentry *dentry);
static const struct inode_operations wbkfs_dir_inode_operations = {
	.lookup		= simple_lookup,
	.create		= wbkfs_create,
	.link		= wbkfs_link,
	.unlink		= wbkfs_unlink,
	.symlink	= wbkfs_symlink,
	.mkdir		= wbkfs_mkdir,
//	.rmdir		= wbkfs_rmdir,
};








// Efetiva a leitura do arquivo
ssize_t wbkfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	struct inode *inode = file->f_path.dentry->d_inode;
	int size = count;

	if (inode->i_size < count)
		size = inode->i_size;

	if ((*pos + size) >= inode->i_size)
		size = inode->i_size - *pos;

	if (copy_to_user(buf, inode->i_private + *pos, size)) {
		return -EFAULT;
	}
	*pos += size;

	return size;
}


// Localizar o arquivo de backup (que deve estar no mesmo diretório do arquivo original).
struct inode *wbkfs_find_bkp(struct file *file, const char *name)
{
	struct dentry *parent;
	struct dentry *child;
	struct list_head *next;

	// Pega o dentry pai do arquivo
	parent = file->f_path.dentry->d_parent;

	// Pega o primeiro filho da lista
	next = parent->d_subdirs.next;

	// Para cada dentry filho, verifique se ele tem o nome a ser localizado
	while (next != &parent->d_subdirs) {
		child = list_entry(next, struct dentry, d_u.d_child);
		if (strcmp(child->d_name.name, name)==0) {
			return child->d_inode;
		}
		next = next->next;
	}
	return NULL;
}


// Grava o conteúdo de "buf" no respectivo inode, e
// grava o conteúdo antigo no inode de backup.
ssize_t wbkfs_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	struct inode *inode = file->f_path.dentry->d_inode;
	struct inode *inode_bkp;

	size_t ctd_arquivo;

	const char *name  = file->f_path.dentry->d_name.name;
	char ext[]        = ".BKP";
	char name_bkp[80] = ".";
	char *txt_bkp;

	// Armazena o tamanho do arquivo
	ctd_arquivo = strlen(inode->i_private);
	printk("*** ctd_arquivo(%i) - i_size(%lld)***\n", ctd_arquivo, inode->i_size);

	// Realiza backup apenas se arquivo não tem a extensão ".BKP"
	if (strstr(name, ext) == NULL) {

		// Monta o nome do arquivo de Backup
		strcat(name_bkp, name);
		strcat(name_bkp, ext);

		// Tenta localizar o arquivo de backup
		inode_bkp = wbkfs_find_bkp(file, name_bkp);

		// Se ele não existe, então
		if (!inode_bkp) {
			// Cria o arquivo de backup
			inode_bkp = wbkfs_create_backup(inode->i_sb, file->f_path.dentry->d_parent, name_bkp);
			if (!inode_bkp){
				return -ENOENT;
			}
		}

		// Limpa o conteúdo do inode (antes de gravar o novo texto)
		txt_bkp = inode_bkp->i_private;
		memset(txt_bkp, 0, strlen(txt_bkp));

		// Copia o conteúdo do inode para o backup
		strncpy(txt_bkp, inode->i_private, ctd_arquivo);

		printk("***01 inode----->i_private=(%s) ***\n", (char*)inode->i_private);
		printk("***02 inode_bkp->i_private=(%s) ***\n", (char*)inode_bkp->i_private);
		
		// Armazena o tamanho do texto no inode de backup
		inode_bkp->i_size = strlen(inode_bkp->i_private);
		printk("*** Backup-ISIZE (%lld) ***\n", inode_bkp->i_size);
	}

	// Limpa o conteúdo do inode (antes de gravar o novo texto)
	memset(inode->i_private, 0, ctd_arquivo);

	// Realiza o procedimento normal (Gravar no arquivo original)
	if (copy_from_user(inode->i_private + *pos, buf, count)) {
		return -EFAULT;
	}
	printk("***GRAVADO=(%s) ***\n", (char*)inode->i_private);

	// Grava o tamanho atual do arquivo
	inode->i_size = count;
	printk("***i_SIZE=(%lld) - count(%i) ***\n", inode->i_size, count);

	return count;
}


// Abre um arquivo
static int wbkfs_open(struct inode *inode, struct file *file)
{
	if (inode->i_private) {
		file->private_data = inode->i_private;
	}
	return 0;
}


// Cria de um arquivo (Chamado pelo sistema)
static int wbkfs_create (struct inode *dir, struct dentry * dentry, umode_t mode, bool excl)
{
	struct inode *inode;

	mode |= S_IFREG;

	// Cria um novo inode
	inode = wbkfs_make_inode(dir->i_sb, mode);
	if (!inode) {
		goto ret_erro;
	}

	// Associa o dentry com o inode
	d_instantiate(dentry, inode);
	dget(dentry);

	return 0;

ret_erro:
	return -ENOSPC;
}


// Criação do arquivo de backup
static struct inode *wbkfs_create_backup(struct super_block *sb, struct dentry *dir, const char *name)
{
	struct qstr    qname;
	struct dentry *dentry;
	struct inode  *inode;

	// Make a hashed version of the name to go with the dentry.
	qname.name = name;
	qname.len  = strlen (name);
	qname.hash = full_name_hash(name, qname.len);

	// Now we can create our dentry and the inode to go with it.
	dentry = d_alloc(dir, &qname);
	if (! dentry){
		goto out_dentry;
	}

	// Cria o inode
	inode = wbkfs_make_inode(sb, 0644 | S_IFREG);
	if (!inode){
		goto out_inode;
	}

	// Associa o dentry com o inode
	d_add(dentry, inode);

	return inode;

// Then again, maybe it didn't work.
out_inode:
	dput(dentry);
out_dentry:
	return NULL;
}


static int wbkfs_link(struct dentry * old_dentry, struct inode * dir, struct dentry * dentry) {

    struct inode * inode = old_dentry->d_inode;

    inode->i_ctime = CURRENT_TIME;
    inode_inc_link_count(inode);
    atomic_inc(&inode->i_count);

    d_instantiate(dentry,inode);
    dget(dentry);

    return 0;
}


static int wbkfs_unlink(struct inode * dir, struct dentry *dentry ) {

    struct inode * inode = dentry->d_inode;
    free_pages((unsigned long)(inode->i_private), get_order(inode->i_size));

    inode->i_ctime = dir->i_ctime;
    inode_dec_link_count(inode);

    return 0;			
}


static int wbkfs_symlink(struct inode * dir, struct dentry *dentry, const char * symname) {

    struct inode * inode;
    int error = - ENOSPC;

    inode = wbkfs_make_inode(dir->i_sb, S_IFLNK | S_IRWXUGO);

    if (inode) {
        int l = strlen(symname)+1;
        error = page_symlink(inode, symname, l);
        if (!error) {
            if (dir->i_mode & S_ISGID)
                inode->i_gid = dir->i_gid;
            d_instantiate(dentry, inode);
            dget(dentry);
            dir->i_mtime = dir->i_ctime = CURRENT_TIME;
        }
        else 
            iput(inode);
    }
    return error;
}


static int wbkfs_mkdir(struct inode * dir, struct dentry *dentry, umode_t mode) {

    struct inode * inode;

    mode |= S_IFDIR;

    inode = wbkfs_make_inode(dir->i_sb, mode);

    if (inode) {
        if (dir->i_mode & S_ISGID) {
            inode->i_gid = dir->i_gid;
            if (S_ISDIR(mode))
                inode->i_mode |= S_ISGID;
        }

        d_instantiate(dentry, inode);
        dget(dentry);
        dir->i_mtime = dir->i_ctime = CURRENT_TIME;
        inode->i_ino = ++inode_number;
        inc_nlink(dir);

        return 0;
    }
    return -ENOMEM;
}

//static int wbkfs_rmdir(struct inode * dir, struct dentry *dentry) {
//    printk("*** Tentando remover o Diretório (%s)", dentry->d_name.name);
//    return 0;
//}

// Cria um novo inode.
static struct inode *wbkfs_make_inode(struct super_block *sb, int mode)
{
	struct inode *inode;
	struct page  *page;

	// Cria um novo inode
	inode = new_inode(sb);

	if (inode) {
		inode->i_ino     = ++inode_number;
		inode->i_uid     = current->cred->fsuid;
		inode->i_gid     = current->cred->fsgid;
		inode->i_size    = 0;
		inode->i_atime   = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
		inode->i_blocks  = 0;
		inode->i_bytes   = 0;
		inode->i_mode    = mode;
		inode->i_sb      = sb;
		switch (mode & S_IFMT) {
			case S_IFREG:
				// Aloca uma nova página para o conteúdo do arquivo
				page = alloc_page(GFP_KERNEL);
				if (!page) {
					iput(inode);
					return NULL;
				} else {
					memset(page, 0, sizeof(page));
				}
				inode->i_private = page_address(page);
				inode->i_op  = &wbkfs_file_inode_operations;
				inode->i_fop = &wbkfs_file_operations;
				break;
			case S_IFDIR:
				inode->i_op  = &wbkfs_dir_inode_operations;
				inode->i_fop = &simple_dir_operations;
				/* directory inodes start off with i_nlink == 2 (for "." entry) */
				inc_nlink(inode);
				break;
		}
	}
	return inode;
}


static int wbkfs_fill_super(struct super_block *sb, void *data, int silent)
{
        struct inode * inode;
        struct dentry * root;

        sb->s_maxbytes		= 4096;
        sb->s_magic		= 0xDE5AF105;
	sb->s_blocksize		= 1024;
	sb->s_blocksize_bits	= 10;
        sb->s_op		= &wbkfs_ops;
        sb->s_time_gran		= 1;

        inode = new_inode(sb);

        if (!inode) {
                return -ENOMEM;
	}

        inode->i_ino	= ++inode_number;
        inode->i_mtime	= inode->i_atime = inode->i_ctime = CURRENT_TIME;
        inode->i_blocks	= 0;
        inode->i_uid	= inode->i_gid = 0;
        inode->i_mode	= S_IFDIR | S_IRUGO | S_IXUGO | S_IWUSR;
        inode->i_op	= &wbkfs_dir_inode_operations;
        inode->i_fop	= &simple_dir_operations;
        set_nlink(inode, 2);

        root = d_make_root(inode);
        if (!root) {
                iput(inode);
                return -ENOMEM;
        }
        sb->s_root = root;
        return 0;
}


static struct dentry *wbkfs_get_sb(struct file_system_type *fs_type, int flags, const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, wbkfs_fill_super);
}


static struct file_system_type wbkfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "wbkfs",
	.mount		= wbkfs_get_sb,
	.kill_sb	= kill_litter_super,
};


static int __init init_wbkfs_fs(void)
{
	int err;
	inode_number = 0;
	err=register_filesystem(&wbkfs_fs_type);
	if (!err) {
		goto out;
	}
	printk(KERN_INFO "*** WBKFS: Support added --- Anderson ***\n");
out:
	return err;
}

static void __exit exit_wbkfs_fs(void)
{
	unregister_filesystem(&wbkfs_fs_type);
}

module_init(init_wbkfs_fs)
module_exit(exit_wbkfs_fs)

MODULE_LICENSE("GPL");


